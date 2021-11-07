#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <set>

using namespace std;
using namespace chrono;

 
constexpr int MAX_THREADS = 8;

class SeqObject {
public:
	Response apply(Invocation invoc);
};

enum MethodType { INSERT, ERASE, CLEAR };
typedef int InputValue;
typedef int Response;
class Invocation {
public:
	MethodType type;
	InputValue v;
};
class SeqObject_Set {
	set <int> m_set;
public:
	Response apply(Invocation invoc)
	{
		int res = -1;
		if (INSERT == invoc.type) m_set.insert(invoc.v);
		else if (ERASE == invoc.type) res = m_set.erase(invoc.v);
		else if (CLEAR == invoc.type) m_set.clear();
		return res;
	}
};
class Consensus {
private:
	NODE* d_value;
public:
	Consensus() {
		d_value = nullptr; 
	}
	NODE* decide(NODE* value) {
		CAS(d_value, nullptr, value);
		return d_value;
	}
};

class NODE
{
public:
	Invocation invoc;
	Consensus decideNext;
	NODE* next;
	volatile int seq;
	NODE() { seq = 0; next = nullptr; }
	~NODE() { }
	NODE(const Invocation& input_invoc)
	{
		invoc = input_invoc;
		next = nullptr;
		seq = 0;
	}
};

bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node);

bool CAS(volatile int* addr, int expected, int new_val);

class LFUniversal {
private:
	NODE* head[MAX_THREADS];
	NODE tail;
public:
	LFUniversal() {
		tail.seq = 1;
		for (int i = 0; i < MAX_THREADS; ++i) head[i] = &tail;
	}
	Response apply(Invocation invoc, int thread_id) {
		int i = thread_id;
		NODE prefer = NODE(invoc);
		while (prefer.seq == 0) {
			NODE* before = tail.max(head);
			NODE* after = before->decideNext->decide(&prefer);
			before->next = after; after->seq = before->seq + 1;
			head[i] = after;
		}
		SeqObject myObject;
		NODE* current = tail.next;
		while (current != &prefer) {
			myObject.apply(current->invoc);
			current = current->next;
		}
		return myObject.apply(current->invoc);
	}

	void add(int x) {
	}
	void remove(int x) {

	}
	void contains(int x) {

	}
	void 
};

LFUniversal myset;

// 세밀한 동기화를 할 때,
// 검색 및 수정 모두에서 잠금이 필요

void Benchmark(int num_threads)
{
	const int NUM_TEST = 4000;
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

		// to clear...
		//
		// 

		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i);
		}

		for (auto& th : worker) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		myset.verify(20);
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << " ms\n";
	}
}

bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node)
{
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int64_t*>(&next),
		reinterpret_cast<long long*>(&old_node),
		reinterpret_cast<long long>(new_node));
}

bool CAS(volatile int* addr, int expected, int new_val)
{
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr), &expected, new_val);
}
