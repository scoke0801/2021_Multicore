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
	NODE* volatile next; // 복합 포인터 

	NODE() {}
	NODE(int key_value) { key = key_value; next = nullptr; }
	~NODE() {}
};

struct STPTR { 
	NODE* volatile ptr;
	int volatile stamp;
};

class null_mutex {
public:
	void lock() {}
	void unlock() {}
};
class C_QUEUE {
public:
	NODE* head;
	NODE* tail;
	mutex c_lock;

	C_QUEUE()
	{
		head = tail = new NODE(0);
	}

	~C_QUEUE() { init(); }
	void init()
	{
		while (head != tail) {
			NODE* p = head;
			head = head->next;
			delete p;
		}
	}
	void Enq(int x)
	{
		NODE* e = new NODE(x);
		if (e == nullptr) {
			return;
		}

		c_lock.lock();
		
		tail->next = e;
		tail = e;

		c_lock.unlock();
	}

	int Deq()
	{  
		c_lock.lock();
		
		if (head == tail) {
			c_lock.unlock();
			return -1;
		}

		NODE* delNode = head;
		int retVal = delNode->key;
		head = head->next;
		delete delNode;

		c_lock.unlock();
		
		return retVal;
	}

	void Verify()
	{
		NODE* p = head;
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


class LF_QUEUE {
public:
	NODE* volatile head;
	NODE* volatile tail;
	 
	LF_QUEUE()
	{
		head = tail = new NODE(0);
	}

	~LF_QUEUE() { init(); }
	void init()
	{
		while (head != tail) {
			NODE* p = head;
			head = head->next;
			delete p;
		}
	}
	void Enq(int x)
	{
		NODE* e = new NODE(x);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;
			if (last != tail) continue;
			if (nullptr == next) {
				if (CAS((last->next), nullptr, e)) {
					CAS(tail, last, e);
					return;
				}
			}
			else CAS(tail, last, next);
		}
	}

	int Deq()
	{
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first != head) continue;
			if (nullptr == next) return -1;
			if (first == last) {
				CAS(tail, last, next);
				continue;
			}
			int value = next->key;
			if (false == CAS(head, first, next)) continue;
			delete first;
			return value;
		}
	}

	void Verify()
	{
		NODE* p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == nullptr) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}

	bool CAS(NODE * volatile& next, NODE *old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};  

class SPLF_QUEUE {
public:
	STPTR head;
	STPTR tail;

	SPLF_QUEUE()
	{
		head.ptr = tail.ptr = new NODE(0);
		head.stamp = tail.stamp = 0;
	}

	~SPLF_QUEUE() { init(); delete head.ptr; }
	void init()
	{
		while (head.ptr != tail.ptr) {
			NODE* p = head.ptr;
			head.ptr = head.ptr->next;
			delete p;
		}
	}
	void Enq(int x)
	{
		NODE* e = new NODE(x);
		while (true) {
			STPTR last = tail;
			NODE* next = last.ptr->next;
			if (last.ptr != tail.ptr || (last.stamp != tail.stamp)) continue;
			if (nullptr == next) {
				if (CAS(last.ptr->next, nullptr, e)) {
					STAMP_CAS(&tail, last.ptr, e, last.stamp, last.stamp + 1);
					return;
				}
			}
			else STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp + 1);
		}
	}

	int Deq()
	{
		while (true) { 
			STPTR first = head;
			STPTR last = tail;
			NODE* next = first.ptr->next;
			if (first.ptr != head.ptr || (first.stamp != head.stamp)) continue;
			if (nullptr == next) return -1;
			if (first.ptr == last.ptr) {
				STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp + 1);
				continue;
			}
			int value = next->key;
			if (false == STAMP_CAS(&head, first.ptr, next, first.stamp, first.stamp +1)) continue;
			delete first.ptr;
			return value;
		}
	}

	void Verify()
	{
		NODE* p = head.ptr->next;
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

	bool STAMP_CAS(STPTR* next, NODE* old_p, NODE* new_p, int old_st, int new_st)
	{
		// 변수를 따로 만들어서 사용함에 주의
		STPTR old_v{ old_p, old_st };
		STPTR new_v{ new_p, new_st };
		long long new_value = *(reinterpret_cast<long long*>(&new_p));
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_llong*>(next),
			reinterpret_cast<long long*>(&old_v),
			new_value);
	}
};

//C_QUEUE myqueue;
//LF_QUEUE myqueue;
SPLF_QUEUE myqueue; 

void Benchmark(int num_threads)
{
	const int NUM_TEST = 10000000; 

	for (int i = 0; i < NUM_TEST / num_threads; ++i) { 
		if (rand() % 2 || i < 32 / num_threads) {
			myqueue.Enq(i);
		}
		else {
			myqueue.Deq();
		} 
	}
}

int main()
{
	for (int i = 1; i <= 8; i = i * 2) {
		myqueue.init();
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

		myqueue.Verify();
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
	}
}