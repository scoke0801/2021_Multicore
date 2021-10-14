#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace std;
using namespace chrono;

bool CAS(volatile int* addr, int expected, int new_val)
{
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr), &expected, new_val);
}

class NODE {
public:
	int key;
	//NODE* next;
	NODE* volatile next;
	mutex node_lock;
	bool marked;	// removed

	NODE() { next = nullptr; marked = false; }
	NODE(int key_value) { key = key_value; next = nullptr; marked = false; }
	~NODE() {}
	void lock()
	{
		node_lock.lock();
	}
	void unlock()
	{
		node_lock.unlock();
	}
}; 
// 싱글스레드를 테스트하기 위해
// 아래의 null_mutex객체를 이용함.
class null_mutex
{
public:
	void lock() {}
	void unlock() {}
};
class LF_SET {
	NODE head, tail;
public:
	LF_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}

	~LF_SET() { init(); }
	void init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool add(int x)
	{
		NODE* pred, * curr;

		while (true) {
			pred = &head;
			curr = pred->next;

			while (curr->key < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->lock();
			curr->lock();

			if (validate(pred, curr)) {
				if (curr->key == x) {
					pred->unlock();
					curr->unlock();
					return false;
				}
				else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = new_node;
					// 위의 노드 생성순서가 바뀐다면
					// 에러가 발생

					pred->unlock();
					curr->unlock();
					return true;
				}
			}

			pred->unlock();
			curr->unlock();
		}
	}

	bool remove(int x)
	{
		NODE* pred, * curr;

		while (true) {
			pred = &head;

			curr = pred->next;

			while (curr->key < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->lock();
			curr->lock();
			if (validate(pred, curr)) {
				if (curr->key != x) {
					pred->unlock();
					curr->unlock();
					return false;
				}
				else {
					curr->marked = true;
					pred->next = curr->next;
					pred->unlock();
					curr->unlock();
					//delete curr;

					return true;
				}
			}
			pred->unlock();
			curr->unlock();
		}
	}

	bool contains(int x)
	{
		NODE* pred, * curr;

		// 게으른 동기화를 통해 wait-free

		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리


		pred = &head;
		curr = pred->next;

		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}
		return curr->key == x && !curr->marked;
	}

	void verify()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}
	bool validate(NODE* pred, NODE* curr)
	{
		return !pred->marked && !curr->marked &&
			(pred->next == curr);
	}
};

LF_SET myset;

// 세밀한 동기화를 할 때,
// 검색 및 수정 모두에서 잠금이 필요

void Benchmark(int num_threads)
{
	const int NUM_TEST = 4000000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 3) {
		case 0:
			myset.add(x);
			break;
		case 1:
			myset.remove(x);
			break;
		case 2:
			myset.contains(x);
			break;
		}
	}
}

int main()
{
	for (int i = 1; i <= 8; i = i * 2) {
		myset.init();
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

		myset.verify();
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
	}
}