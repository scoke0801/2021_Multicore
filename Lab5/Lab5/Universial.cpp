#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <set>

using namespace std;
using namespace chrono;


enum MethodType { INSERT, ERASE, CLEAR };

typedef int InputValue;
typedef int Response;
constexpr int INIT = -1;
constexpr int MAX_THREADS = 8;

bool CAS(NODE* volatile& next, NODE* old_node, NODE* new_node); 

class Consensus {
public:
	Consensus()
	{
		d_value = -1; // INIT은 절대로 사용되지 않는 값
	}
	int decide(int value) 
	{
		CAS(&d_value, INIT, value);
		return d_value;
	}
private:
	volatile int d_value;
};

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


constexpr int MAX_THREAD = 8;
class LFUniversal {
private:
	NODE* head[MAX_THREAD];
	NODE tail;
public:
	LFUniversal() { 
		tail.seq = 1;
		for (int i = 0; i < MAX_THREAD; ++i) head[i] = &tail;
	}
	Response apply(Invocation invoc, int thread_id) { 
		NODE prefer = NODE(invoc);
		while (prefer.seq == 0) {
			NODE* before = tail.max(head);
			NODE* after = before->decideNext->decide(&prefer);
			before->next = after; after->seq = before->seq + 1;
			head[thread_id] = after;
		}
		SeqObject_Set myObject;
		NODE* current = tail.next;
		while (current != &prefer) {
			myObject.apply(current->invoc);
			current = current->next;
		}
		return myObject.apply(current->invoc);
	}

	void init();
	void add(int value);
	bool remove(int value);
	bool contains(int value);
	bool verify(int num);
};

class WFUniversal{
private:
	NODE* announce[MAX_THREADS];
	NODE* head[MAX_THREADS];
	NODE tail;
public:
	WFUniversal() {
		tail.seq = 1;
		for (int i = 0; i < MAX_THREADS; ++i) {
			head[i] = &tail; announce[i] = &tail; 
		}
	}
	Response apply(Invocation invoc, int thread_id) { 
		announce[thread_id] = new NODE(invoc);
		head[thread_id] = tail.max(head);
		while (announce[thread_id]->seq == 0) {
			NODE* before = head[thread_id];
			NODE* help = announce[((before->seq + 1) % MAX_THREADS)];
			NODE* prefer;
			if (help->seq == 0) prefer = help;
			else prefer = announce[thread_id];
			NODE* after = before->decideNext->decide(prefer);
			before->next = after;
			after->seq = before->seq + 1;
			head[thread_id] = after;
		}
		SeqObject_Set myObject;
		NODE* current = tail.next;
		while (current != announce[thread_id]) {
			myObject.apply(current->invoc);
			current = current->next;
		}
		head[thread_id] = announce[thread_id];
		return myObject.apply(current->invoc);
	}
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
