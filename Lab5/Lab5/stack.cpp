#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

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

		while (true) {
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
			if (top == nullptr) {
				return -2;
			}

			NODE* cur = top;
			NODE* next = cur->next;

			int retVal = cur->key;
			if (CAS_TOP(cur, next)) {
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

//C_STACK mystack;
LF_STACK mystack;


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