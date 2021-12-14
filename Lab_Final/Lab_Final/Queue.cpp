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

class C_QUEUE {
	NODE* head, * tail;
	mutex c_lock;
public:
	C_QUEUE() {
		tail = head = new NODE(0);
	}
	~C_QUEUE() { 
	}
	void init() {
		// 보초 노드 남기기
		while (head->next != nullptr) {
			NODE* temp = head->next;
			head->next = head->next->next;
			delete temp;
		}
		tail = head;
	}

public:
	void Enq(int key) {
		NODE* e = new NODE(key);
		if (e == nullptr) {
			return;
		}
		c_lock.lock();

		tail->next = e;
		tail = e;
		
		c_lock.unlock(); 
	}
	int Deq() { 
		c_lock.lock();
		if (head->next == nullptr) {
			c_lock.unlock();
			return -1;
		}
		int key = head->next->key;
		NODE* temp = head;
		head = head->next;
		delete temp;

		c_lock.unlock();
		return key;
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
};

class LF_QUEUE {
	NODE* volatile head;
	NODE* volatile tail;

public:
	LF_QUEUE() {
		tail = head = new NODE(0);
	}
	~LF_QUEUE() {
		init();
		delete head;
	}
	void init() {
		// 보초 노드 남기기
		while (head->next != nullptr) {
			NODE* temp = head->next;
			head->next = head->next->next;
			delete temp;
		}
		tail = head;
	}

public:
	void Enq(int key) {
		NODE* e = new NODE(key);
		if (e == nullptr) {
			return;
		} 

		while (true) {
			NODE* last = tail;
			NODE* next = last->next;

			if (last != tail) {
				continue;
			}

			if (next == nullptr) {
				if (CAS(last->next, nullptr, e)) {
					CAS(tail, last, e);
					return;
				}
			}
			else {
				// 전진을 보조
				CAS(tail, last, next);
			}
		}
	}
	int Deq() { 
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = head->next;

			if (first != head) {
				continue;
			}
			if(first == last){
				if (next == nullptr) {
					return -1;
					// empty
				}
				CAS(tail, last, next);
				// enq를 보조
				continue;
			}
			int key = next->key;
			if (false == CAS(head, first, next)) {
				continue;
			}
			//elete first;
			return key;
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
	
	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};

struct STPTR {
	NODE* volatile ptr;
	int volatile stamp;

	STPTR() { 
		ptr = nullptr; stamp = 0; 
	}

	STPTR(NODE* p, int v) {
		ptr = p; stamp = v; 
	}
};

class SPLF_QUEUE {
	STPTR head;
	STPTR tail;

public:
	SPLF_QUEUE() {
		tail.ptr = head.ptr = new NODE(0);
		tail.stamp = head.stamp = 0;
	}
	~SPLF_QUEUE() {
		init();
		delete head.ptr;
	}
	void init() {
		// 보초 노드 남기기
		while (head.ptr->next != nullptr) {
			NODE* temp = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			delete temp;
		}
		tail = head;
	}

public:
	void Enq(int key) {
		NODE* e = new NODE(key);
		if (e == nullptr) {
			return;
		}

		while (true) {
			STPTR last = tail;
			NODE* next = last.ptr->next;

			if (last.ptr != tail.ptr) {
				continue;
			}

			if (next == nullptr) {
				if (CAS(last.ptr->next, nullptr, e)) {
					STAMP_CAS(tail, last.ptr, e, last.stamp, last.stamp + 1);
					return;
				}
			}
			else {
				// 전진을 보조
				STAMP_CAS(tail, last.ptr, next, last.stamp, last.stamp + 1);
			}
		}
	}
	int Deq() {
		while (true) {
			STPTR first = head;
			STPTR last = tail;
			NODE* next = first.ptr->next;

			if (first.ptr != head.ptr) {
				continue;
			}
			if (first.ptr == last.ptr) {
				if (next == nullptr) {
					// empty
					return -1;
				}
				STAMP_CAS(tail, last.ptr, next, last.stamp, last.stamp + 1);
				continue;
			}

			int key = first.ptr->key;
			if (false == STAMP_CAS(head, first.ptr, next, first.stamp, first.stamp + 1)) {
				continue;
			}
			// delete first.ptr;
			return key;
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

	bool STAMP_CAS(STPTR volatile& next, NODE* old_p, NODE* new_p, int old_st, int new_st)
	{
		STPTR old_v{ old_p, old_st };
		STPTR new_v{ new_p, new_st };
		 
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_v),
			*reinterpret_cast<long long*>(&new_v));
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