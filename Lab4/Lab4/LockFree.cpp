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

class AtomicMarkableReference
{
public:
	AtomicMarkableReference();
	AtomicMarkableReference(NODE* curr, bool mark) {

	}
};
class NODE {
public:
	int key;
	//AtomicMarkableReference next;
	NODE* next;
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
	bool CAS(int old_v, int new_v)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&next), &old_v, new_v);
	}
	bool CAS32(NODE* old_node, NODE* new_node,
		bool oldMark, bool newMark) 
	{
		int oldvalue = reinterpret_cast<int>(old_node);
		if (oldMark) oldvalue = oldvalue | 0x01;
		else oldvalue = oldvalue & 0xFFFFFFFE;

		int newvalue = reinterpret_cast<int>(new_node);

		if (newMark) newvalue = newvalue | 0x01;
		else newvalue = newvalue & 0xFFFFFFFE;
		return CAS(oldvalue, newvalue);
	}

	bool CAS64(NODE* old_node, NODE* new_node, bool oldMark, bool newMark)
	{
		long oldvalue = reinterpret_cast<long>(old_node);
		if (oldMark) oldvalue = oldvalue | 0x01;
		else oldvalue = oldvalue & 0xFFFFFFFFFFFFFFFE;

		long newvalue = reinterpret_cast<long>(new_node); 
		if (newMark) newvalue = newvalue | 0x01;
		else newvalue = newvalue & 0xFFFFFFFFFFFFFFFE;

		return CAS(oldvalue, newvalue);
	}

	NODE* GetReference()
	{
		return nullptr;
	}

	NODE* Get(bool* mark)
	{
		return nullptr;
	}

	bool AttemptMark(NODE* nextnode, bool mark)
	{
		return false;
	}

	Window AtomicMarkableReference()
	{

	}

	//CompareAndSet(oldmark, newmark, *oldnode, *newnode)
	//{
	//
	//}
}; 

class Window {
public:
	NODE* pred, * curr;

	Window(NODE* myPred, NODE* myCurr) {
		pred = myPred; 
		curr = myCurr;
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
			Window* window = Find(&head, x);
			pred = window->pred, curr = window->curr;

			if (curr->key == x) {
				return false;
			}
			else {
				NODE* node = new NODE(x);
				node->next = new AtomicMarkableReference(curr, false);
				if (pred->next->CAS64(curr, node, false, false)) {
					return true;
				}
			}
		}
	}

	bool remove(int x)
	{
		NODE* pred, * curr;
		
		bool snip;

		while (true) {
			Window* window = Find(&head, x);
			pred = window->pred;
			curr = window->curr;

			if (curr->key != x) {
				return false;
			}
			else {
				NODE* succ = curr->next->GetReference();
				snip = curr->next->AttemptMark(succ, true);
				if (!snip) {
					continue;
				}
				pred->next->CAS64(curr, succ, false, false);
				return true;
			}
		}
	}

	bool contains(int x)
	{
		NODE* pred, * curr;

		// 게으른 동기화를 통해 wait-free

		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리
		bool marked[] = { false, };
		NODE* curr = &head;
		while (curr->key < x) {
			curr = curr->next->GetReference();
			NODE* succ = curr->next->Get(marked);
		}
		return (curr->key == x && !marked[0]);
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

	Window* Find(NODE* head, int key) {
		NODE* pred = nullptr;
		NODE* curr = nullptr;
		NODE* succ = nullptr;

		bool marked[] = { false, };
		bool snip = false;
		while (true)
		{
			pred = head;
			curr = pred->next->GetReference();

			while (true) {
				succ = curr->next->CAS64(curr, succ, false, false);
				if (!snip) {
					continue;
				}
				curr = succ;
				succ = curr->next->Get(marked);
			}
			if (curr->key >= key) {
				return new Window(pred, curr);
			}
			pred = curr;
			curr = succ;
		}
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