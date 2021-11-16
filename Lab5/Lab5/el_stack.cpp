#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

constexpr int MAX_THREADS = 16;

class NODE {
public:
	int value;
	NODE* next;
	NODE(int x) : value(x), next(nullptr) {}
	NODE() : next(nullptr) {}
	~NODE() {}
};

class null_mutex
{
public:
	void lock() {}
	void unlock() {}
};

class C_STACK {
	NODE* top;
	null_mutex c_lock;

public:
	C_STACK() : top(nullptr)
	{
	}
	~C_STACK()
	{
		Init();
	}
	void Init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void Push(int x)
	{
		NODE* e = new NODE(x);
		c_lock.lock();
		e->next = top;
		top = e;
		c_lock.unlock();
		return;
	}
	int Pop()
	{
		c_lock.lock();
		if (nullptr == top) return -2;
		NODE* ptr = top;
		int value = ptr->value;
		top = top->next;
		delete ptr;
		c_lock.unlock();
		return value;
	}
	void Verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

class LF_STACK {
	NODE* volatile top;

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int volatile*>(&top),
			reinterpret_cast<int*>(&old_ptr),
			reinterpret_cast<int>(new_ptr)
		);
	}

public:
	LF_STACK() : top(nullptr)
	{
	}
	~LF_STACK()
	{
		Init();
	}
	void Init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void Push(int x)
	{
		NODE* e = new NODE(x);

		while (true) {
			// 현재 자료구조를 Atomic하게 파악
			NODE* ptr = top;
			// 변경될 자료구조를 준비
			e->next = ptr;
			// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, e))
				return;
			// 실패하면 처음부터
		}
	}
	int Pop()
	{
		while (true) {
			NODE* ptr = top;
			if (nullptr == ptr) return -2;
			int value = ptr->value;
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, ptr->next)) {
				// delete ptr;
				return value;
			}
		}
	}
	void Verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

class BackOff {
	int minDelay, maxDelay;
	int limit;
public:
	BackOff(int min, int max)
		: minDelay(min), maxDelay(max), limit(min) {}

	void InterruptedException() {
		int delay = 0;
		if (limit != 0) delay = rand() % limit;
		limit *= 2;
		if (limit > maxDelay) limit = maxDelay;
		this_thread::sleep_for(chrono::microseconds(delay));;
	}
};

class LFBO_STACK {
	NODE* volatile top;

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int volatile*>(&top),
			reinterpret_cast<int*>(&old_ptr),
			reinterpret_cast<int>(new_ptr)
		);
	}

public:
	LFBO_STACK() : top(nullptr)
	{
	}
	~LFBO_STACK()
	{
		Init();
	}
	void Init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void Push(int x)
	{
		NODE* e = new NODE(x);
		BackOff bo{ 1, 100 };

		while (true) {
			// 현재 자료구조를 Atomic하게 파악
			NODE* ptr = top;
			// 변경될 자료구조를 준비
			e->next = ptr;
			// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, e))
				return;
			else
				bo.InterruptedException();
			// 실패하면 처음부터
		}
	}
	int Pop()
	{
		BackOff bo{ 1, 100 };
		while (true) {
			NODE* ptr = top;
			if (nullptr == ptr) return -2;
			int value = ptr->value;
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, ptr->next)) {
				// delete ptr;
				return value;
			}
			else
				bo.InterruptedException();
		}
	}
	void Verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

constexpr int EMPTY = 0;
constexpr int WAITING = 0x40000000;
constexpr int BUSY = 0x80000000;

class LockFreeExchanger {
	atomic <unsigned int> slot;
public:
	LockFreeExchanger() {}
	~LockFreeExchanger() {}
	int exchange(int value, bool* time_out, bool* busy) {
		while (true) {
			unsigned int cur_slot = slot;
			unsigned int slot_state = cur_slot & 0xC0000000;
			unsigned int slot_value = cur_slot & 0x3FFFFFFF;
			switch (slot_state) {
			case EMPTY:
				unsigned int new_slot = WAITING & value;
				if (true == atomic_compare_exchange_strong(&slot, &cur_slot, new_slot)) {
					for (int i = 0; i < 10; ++i) {
						if (slot & 0xC0000000 == BUSY) {
							int ret_value = slot & 0x3FFFFFFF;
							slot = EMPTY;
							return ret_value;
						}
					}
					// timeout
					if (true == atomic_compare_exchange_strong(&slot, &new_slot, EMPTY)) {
						*time_out = true;
						*busy = false;
						return 0;
					}
					else {
						int ret_value = slot & 0x3FFFFFFF;
						slot = EMPTY;
						return ret_value;
					}
				}
				else continue;
			case WAITING:
				unsigned int new_value = BUSY | value;
				if (true == atomic_compare_exchange_strong(&slot, &cur_slot, new_value)) {
					return slot_value;
				}
				else {
					*time_out = false;
					*busy = true;
					return 0;
				}
			case BUSY:
				*time_out = false;
				*busy = true;
				return 0;
			}
		}
	}
};

class EliminationArray {
	int range;
	int _num_threads;
	LockFreeExchanger exchanger[MAX_THREADS / 2];
public:
	EliminationArray(int num_threads) : _num_threads(num_threads) { range = 1; }
	~EliminationArray() {}
	int Visit(int value, bool* time_out) {
		int slot = rand() % range;
		bool busy;
		int ret = exchanger[slot].exchange(value, time_out, &busy);
		if ((true == *time_out) && (range > 1)) range--;
		if ((true == busy) && (range <= _num_threads / 2)) range++;
		// MAX RANGE is # of thread / 2
		return ret;
	}
};


class LFEL_STACK {
	NODE* volatile top;
	EliminationArray _earray;

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int volatile*>(&top),
			reinterpret_cast<int*>(&old_ptr),
			reinterpret_cast<int>(new_ptr)
		);
	}

public:
	LFEL_STACK() : top(nullptr)
	{
	}
	~LFEL_STACK()
	{
		Init();
	}
	void Init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void Push(int x)
	{
		NODE* e = new NODE(x);

		while (true) {
			// 현재 자료구조를 Atomic하게 파악
			NODE* ptr = top;
			// 변경될 자료구조를 준비
			e->next = ptr;
			// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, e))
				return;
			else {
				bool time_out;
				int ret = _earray.Visit(x, &time_out);
				.....
			}
			// 실패하면 처음부터
		}
	}
	int Pop()
	{
		BackOff bo{ 1, 100 };
		while (true) {
			NODE* ptr = top;
			if (nullptr == ptr) return -2;
			int value = ptr->value;
			if (ptr != top) continue;
			if (true == CAS_TOP(ptr, ptr->next)) {
				// delete ptr;
				return value;
			}
			else
				bo.InterruptedException();
		}
	}
	void Verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

LFBO_STACK my_stack;

void Benchmark(int num_threads)
{
	const int NUM_TEST = 10000000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if ((rand() % 2) || i < 1000 / num_threads)
			my_stack.Push(i);
		else
			my_stack.Pop();
	}
}

int main()
{
	for (int i = 1; i <= 16; i = i * 2) {
		vector <thread> worker;
		my_stack.Init();
		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j)
			worker.emplace_back(Benchmark, i);
		for (auto& th : worker)
			th.join();
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;
		my_stack.Verify();
		cout << i << " threads   ";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms.\n\n";
	}
}

