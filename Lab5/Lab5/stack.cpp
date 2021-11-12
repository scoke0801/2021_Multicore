#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

constexpr int MAX_THREADS = 16;

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

class null_mutex {
public:
	void lock() {}
	void unlock() {}
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
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x); 
		if (e == nullptr) {
			return;
		}

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
			return -2;
		}

		NODE* p = top;
		int retVal = p->key; 
		top = top->next;
		delete p;

		c_lock.unlock();
		
		return retVal;
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
	NODE* volatile top; 

	LF_STACK() : top(nullptr)
	{
	}

	~LF_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);
		if (e == nullptr) {
			return;
		}

		// 현재 자료구조를 atomic하게 파악
		// NODE* ptr = top;
		
		// 변경될 자료구조를 준비
		// e->next = ptr;

		// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
		// if(ptr != top) continue; ... 실패가 빈번한 환경에서는 옆의 코드가 좋을 수 도 있다.
		// CAS_TOP(ptr, e);
		
		// 실패하면 처음부터....
		// 위의 내용을 while(true)로 감싸지

		while (true) {
			// 변수 사용을 조심해서 할 것.
			// top을 직접적으로 사용하지 않고 변수에 담아서 사용해야
			// 해당 값이 결정된 상태로 CAS를 진행할 수 있음
			NODE* curr = top;
			e->next = curr;
			if (CAS_TOP(curr, e)) {
				return;
			}
		} 
	}

	int pop()
	{ 
		while (true) {
			// 변수 사용을 조심해서 할 것
			NODE* cur = top;

			if (cur == nullptr) {
				return -2;
			}

			NODE* next = cur->next;
			int retVal = cur->key;

			if (cur != top) {
				continue;
			}

			if (CAS_TOP(cur, next)) {
				// delete cur ... aba 문제 발생으로 인하여 주석처리
				return retVal;
			} 
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

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong volatile*>(&top),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr));
	}
};


// BackOff stack
class BackOff {
	int minDelay, maxDelay;
	int limit;

public:
	BackOff(int min, int max) : minDelay(min), maxDelay(max), limit(min) {}
	void InterruptedException() {
		int delay = 0;
		if (limit != 0) {
			delay = rand() % limit;
		}
		limit *= 2;
		if (limit > maxDelay) {
			limit = maxDelay;
		}
		this_thread::sleep_for(chrono::microseconds(delay));
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
	NODE* volatile top;
	
	LFBO_STACK() : top(nullptr)
	{
	}

	~LFBO_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);

		if (e == nullptr) {
			return;
		}
		BackOff bo{ 1,100 };

		// 현재 자료구조를 atomic하게 파악
		// NODE* ptr = top;

		// 변경될 자료구조를 준비
		// e->next = ptr;

		// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
		// if(ptr != top) continue; ... 실패가 빈번한 환경에서는 옆의 코드가 좋을 수 도 있다.
		// CAS_TOP(ptr, e);

		// 실패하면 처음부터....
		// 위의 내용을 while(true)로 감싸지

		while (true) {
			// 변수 사용을 조심해서 할 것.
			// top을 직접적으로 사용하지 않고 변수에 담아서 사용해야
			// 해당 값이 결정된 상태로 CAS를 진행할 수 있음
			NODE* curr = top;
			e->next = curr;
			if (CAS_TOP(curr, e)) {
				return;
			}
			else {
				bo.InterruptedException();
			}
		}
	}

	int pop()
	{
		BackOff bo{ 1, 100 };
		while (true) {
			// 변수 사용을 조심해서 할 것
			NODE* cur = top;

			if (cur == nullptr) {
				return -2;
			}

			NODE* next = cur->next;
			int retVal = cur->key;

			if (cur != top) {
				continue;
			}

			if (CAS_TOP(cur, next)) {
				// delete cur ... aba 문제 발생으로 인하여 주석처리
				return retVal;
			}
			else {
				bo.InterruptedException();
			}
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

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong volatile*>(&top),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr));
	}
};

constexpr int EMPTY = 0; // 00
constexpr int WAITING = 0x40000000; // 01
constexpr int BUSY = 0x80000000; // 10

class LockFreeExchanger {
	atomic <unsigned int> slot;

public:
	LockFreeExchanger() { }
	~LockFreeExchanger() {	}

	int exchange(int value, bool* time_out, bool* busy) {
		while (true) {
			unsigned int curr_slot = slot;
			unsigned int slot_state = curr_slot & 0xC0000000; // 앞의 두 비트만 빼내기
			unsigned int slot_value = curr_slot & 0x3FFFFFFF; // 나머지 비트 값 알아오기

			switch (slot_state) {
			case EMPTY:
				unsigned int new_slot = WAITING & value;
				if (atomic_compare_exchange_strong(&slot, &curr_slot, new_slot))
				{
					for (int i = 0; i < 10; ++i) {
						if (slot & 0xC0000000 == BUSY) {

							int ret_value = slot & 0x3FFFFFFF;

							slot = EMPTY;

							return ret_value;
						}
					}
					// 여기까지 오면 timeout
					if (atomic_compare_exchange_strong(&slot, &new_slot, EMPTY)) {
						*time_out = true;
						*busy = false;
						return 0;
					}
					else {
						// 다른 스레드가 와서 busy로 만든 경우는 여기로옴
						int ret_value = slot & 0x3FFFFFFF;

						slot = EMPTY;

						return ret_value;
					}
				}
				else {
					// 애초에 cas가 실패한경우
					continue;
				}
				break;
			case WAITING:
				unsigned int new_value = BUSY | value;
				if (atomic_compare_exchange_strong(&slot, &curr_slot, new_value)) {
					*time_out = false;
					*busy = false;
					return slot_value;
				}
				else {
					// 실패를 했으면?
					// 경합이 너무 심한 상태...
					*time_out = false;
					*busy = true;
					return 0;
				}
				break;
			case BUSY:
				*time_out = false;
				*busy = true;
				return 0;
				break;
			default:
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
	EliminationArray(int num_threads) :_num_threads(num_threads) { range = 1; }
	~EliminationArray() {}
	int Visit(int value, bool* time_out) {
		int ret = -2;
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
public:
	NODE* volatile top;

	EliminationArray _earray;

	LFEL_STACK() : top(nullptr)
	{
	}

	~LFEL_STACK() { init(); }
	void init()
	{
		while (top != nullptr) {
			NODE* p = top;
			top = top->next;
			delete p;
		}
	}
	void push(int x)
	{
		NODE* e = new NODE(x);

		if (e == nullptr) {
			return;
		}

		// 현재 자료구조를 atomic하게 파악
		// NODE* ptr = top;

		// 변경될 자료구조를 준비
		// e->next = ptr;

		// CAS로 충돌을 확인하면서 자료구조를 atomic하게 변경
		// if(ptr != top) continue; ... 실패가 빈번한 환경에서는 옆의 코드가 좋을 수 도 있다.
		// CAS_TOP(ptr, e);

		// 실패하면 처음부터....
		// 위의 내용을 while(true)로 감싸지

		while (true) {
			// 변수 사용을 조심해서 할 것.
			// top을 직접적으로 사용하지 않고 변수에 담아서 사용해야
			// 해당 값이 결정된 상태로 CAS를 진행할 수 있음
			NODE* curr = top;
			e->next = curr;
			if (CAS_TOP(curr, e)) {
				return;
			}
			else {
				bool time_out; 
				int ret = _earray.Visit(x, &time_out);
			}
		}
	}

	int pop()
	{
		BackOff bo{ 1, 100 };
		while (true) {
			// 변수 사용을 조심해서 할 것
			NODE* cur = top;

			if (cur == nullptr) {
				return -2;
			}

			NODE* next = cur->next;
			int retVal = cur->key;

			if (cur != top) {
				continue;
			}

			if (CAS_TOP(cur, next)) {
				// delete cur ... aba 문제 발생으로 인하여 주석처리
				return retVal;
			}
			else {
				bo.InterruptedException();
			}
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

	bool CAS_TOP(NODE* old_ptr, NODE* new_ptr) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong volatile*>(&top),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr));
	}
};

//C_STACK mystack;
//LF_STACK mystack;
LFBO_STACK mystack;

// 세밀한 동기화를 할 때,
// 검색 및 수정 모두에서 잠금이 필요

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