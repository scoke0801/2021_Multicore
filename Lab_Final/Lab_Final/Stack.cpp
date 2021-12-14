#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

constexpr int MAX_THREADS = 8;

using namespace std;
using namespace chrono;

class NODE {
public:
	int key;

	// stack은 volatile을 사용하지 않아도 됨.. (일단)
	//NODE* volatile next; // 복합 포인터 
	NODE* next; // 복합 포인터 

	NODE() {}
	NODE(int key_value) { key = key_value; next = nullptr; }
	~NODE() {}
};

class C_STACK {
public:
	NODE* top;
	mutex c_lock;

	C_STACK() : top(nullptr)
	{
	}

	~C_STACK() { init(); }
	void init()
	{ 
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	void push(int x)
	{ 
		NODE* e = new NODE(x);
		
		c_lock.lock();

		e->next = top;
		top = e;

		c_lock.unlock();
	}

	int pop()
	{ 
		c_lock.lock();

		if (top == nullptr) {
			c_lock.unlock();
			return -1;
		}

		NODE* ptr = top;
		int key = top->key;
		top = top->next;
		c_lock.unlock();
		delete ptr;
		return key;
	}

	void verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};

class LF_STACK {
public:
	NODE* top; 
	LF_STACK() : top(nullptr)
	{
	}

	~LF_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);

		while (true) {
			NODE* first = top; 

			if (first != top) {
				return;
			}

			e->next = first;
			if (CAS(top, first, e)) {	
				return;
			}
		}
	}

	int pop()
	{ 
		while (true) {
			NODE* first = top;
			NODE* next = first->next;

			if (first != top) {
				continue;
			}

			if (first == nullptr) {
				return -1;
			}

			int key = first->key;
			if (false == CAS(top, first, next)) {
				continue;
			}

			// delete first;...
			return key;
		}
	}

	void verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};

class BackOff {
	int minDelay, maxDelay;
	int limit;

public:
	BackOff(int min, int max) : minDelay(min), maxDelay(max), limit(min) {}
	void InterruptedException() {
		int delay = 0;
		if (limit == 0) {
			limit = 1;
		}
		else {
			delay = rand() % limit;
		}
		limit *= 2;
		if (limit >= maxDelay) {
			limit = maxDelay;
		}
		this_thread::sleep_for(microseconds(delay));
	}
	//void InterruptedException2() {
	//	int delay = 0;
	//	if (limit != 0)
	//		delay = rand() % limit;
	//	limit *= 2;
	//	if (limit > maxDelay)
	//		limit = maxDelay;
	//	int start, current;
	//	_asm RDTSC;
	//	_asm mov start, eax;
	//	do {
	//		_asm RDTSC;
	//		_asm mov current, eax;
	//	} while (current - start < delay);
	//}
	//void InterruptedException3() {
	//	int delay = 0;
	//	if (0 != limit) delay = rand() % limit;
	//	if (0 == delay) return;
	//	limit += limit;
	//	if (limit > maxDelay) limit = maxDelay;
	//	_asm mov eax, delay;
	//myloop:
	//	_asm dec eax;
	//	_asm jnz myloop;
	//	// Jump Non Zero, zero가 아니라면 점프를 해라
	//	// dec, Decrease
	//}
};

class LFBO_STACK {
public:
	NODE* top;
	LFBO_STACK() : top(nullptr)
	{
	}

	~LFBO_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);

		BackOff bo{ 1,100 };
		while (true) {
			NODE* first = top;

			if (first != top) {
				return;
			}
			e->next = first;
			if (CAS(top, first, e)) {
				return;
			}
			else {
				bo.InterruptedException();
			}
		}
	}

	int pop()
	{
		BackOff bo{ 1,100 };
		while (true) {
			NODE* first = top;
			NODE* next = first->next;

			if (first != top) {
				continue;
			}

			if (first == nullptr) {
				return -1;
			}

			int key = first->key;
			if (false == CAS(top, first, next)) {
				bo.InterruptedException();
				continue;
			} 
			// delete first;...
			return key;
		}
	}

	void verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};
 
constexpr unsigned int EMPTY = 0;
constexpr unsigned int WAITING = 0x4000'0000;
constexpr unsigned int BUSY = 0x8000'0000;

class LockFreeExchanger {
	atomic <unsigned int> slot;

public:
	LockFreeExchanger() { }
	~LockFreeExchanger() {	}

	int exchange(int value, bool* time_out, bool* busy) {
		
		while (true) {
			unsigned int curr_slot = slot;
			unsigned int slot_value = curr_slot & 0x3FFF'FFFF;
			unsigned int slot_state = curr_slot & 0xC000'0000;

			switch (slot_state) {
			case EMPTY: {
				unsigned int new_slot = WAITING & value;
				if (atomic_compare_exchange_strong(&slot, &curr_slot, new_slot)) {
					for (int i = 0; i < 10; ++i) {
						if ((slot & 0xC000'0000) == WAITING) {
							*time_out = false;
							*busy = false;
							slot = EMPTY;
							return (int)(slot & 0x3FFF'FFFF);
						}
					}
					if (false == atomic_compare_exchange_strong(&slot, &curr_slot, EMPTY)) {
						// 시간초과
						*time_out = true;
						*busy = false;
						return -1;
					}
					else {
						// 다른 쓰레드에서 처리
						*time_out = false;
						*busy = false;
						slot = EMPTY;
						return (int)(slot & 0x3FFF'FFFF);
					}
				}
				else {
					continue;
				}
				break;
			}
			case WAITING: {
				unsigned int new_slot = BUSY & value;
				if (atomic_compare_exchange_strong(&slot, &curr_slot, new_slot)) {
					*time_out = false;
					*busy = false;
					return (int)(slot & 0x3FFF'FFFF);
				}
				else {
					// 너무 바뻐요
					*time_out = false;
					*busy = true;
					return -1;
				}
				break;
			}
			case BUSY:
				*time_out = false;
				*busy = true;
				return -1;
				break;
			}
		}
	}
};

class EliminationArray {
	int range;
	int _num_threads;

	LockFreeExchanger exchanger[MAX_THREADS / 2];
public:
	EliminationArray() { range = 1; _num_threads = 1; }
	EliminationArray(int num_threads) :_num_threads(num_threads) { range = 1; }
	~EliminationArray() {}
	int Visit(int value, bool* time_out) {
		int slot = rand() % range;
		bool busy;
		int ret = exchanger[slot].exchange(value, time_out, &busy);

		if ((true == *time_out) && (range > 1)) { range--; }
		if ((true == busy) && (range <= _num_threads / 2)) range++;
		return ret; 
	}

	void set_threads_num(int num) { _num_threads = num; }
};
class LFEL_STACK {
public:
	NODE* top;
	LFEL_STACK() : top(nullptr)
	{
	}

	~LFEL_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);
		  
		EliminationArray arr;
		while (true) {
			NODE* first = top;

			if (first != top) {
				return;
			}
			e->next = first;
			if (CAS(top, first, e)) {
				return;
			}
			else {
				bool time_out;
				int ret = arr.Visit(x, &time_out);
				
				if (false == time_out && ret != -1) {
					return;
				}
			}
		}
	}

	int pop()
	{
		EliminationArray arr;
		while (true) {
			NODE* first = top;
			NODE* next = first->next;

			if (first != top) {
				continue;
			}

			if (first == nullptr) {
				return -1;
			}

			int key = first->key;
			if (false == CAS(top, first, next)) {
				bool time_out;
				int ret = arr.Visit(-1, &time_out);
				if (false == time_out && ret != -1) {
					return ret;
				}
				continue;
			}
			// delete first;...
			return key;
		}
	}

	void verify()
	{
		NODE* p = top;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};
//C_STACK mystack;
//LF_STACK mystack;
//LFBO_STACK mystack; 
LFEL_STACK mystack;

void Benchmark(int num_threads)
{
	const int NUM_TEST = 10000000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if (rand() % 2 || i < 1000 / num_threads) {
			mystack.push(i);
		}
		else {
			mystack.pop();
		}
	}
}

int main()
{
	for (int i = 1; i <= 8; i = i * 2) {
		mystack.init();
		vector<thread> worker;

		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i);
		}

		for (auto& th : worker) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		mystack.verify();
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
	}
}