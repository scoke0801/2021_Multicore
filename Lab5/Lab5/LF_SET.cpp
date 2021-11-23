#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <set>

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


class LF_SET {
public:

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

	bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
			reinterpret_cast<long long*>(&old_node),
			reinterpret_cast<long long>(new_node));
	}
};

LF_QUEUE myqueue;
LF_SET myset;

// 세밀한 동기화를 할 때,
// 검색 및 수정 모두에서 잠금이 필요

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
	for (int i = 1; i <= 16; i = i * 2) {
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