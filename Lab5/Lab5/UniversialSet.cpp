#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <set>

using namespace std;
using namespace chrono;

enum MethodType { INSERT, ERASE, CLEAR, CONTAIN, VERIFY  };
 
constexpr int MAX_THREADS = 8;
 
typedef int InputValue;
typedef int Response;
thread_local int thread_id;

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
		else if (CONTAIN == invoc.type) m_set.find(invoc.v);
		else if (VERIFY == invoc.type) {
			int count = 0;
			for (auto data : m_set) {
				if (count++ >= 20) {
					break;
				} 
				cout << data << ", ";   
			}
			cout << "\n"; 
		}
		return res;
	}
}; 

class NODE;
class Consensus {
private:
	atomic<long long>* d_value;
public:
	Consensus() : d_value(0) {}
	NODE* decide(NODE* value) {
		long long new_value = reinterpret_cast<long long>(value);
		long long old_value = 0;

		if (atomic_compare_exchange_strong(reinterpret_cast<atomic_int64_t*>(&d_value), &old_value, new_value)) {
			return value;
		}
		return reinterpret_cast<NODE*>(old_value);
	}
};

class NODE
{
public:
	NODE() { init(); }
	~NODE() { }
	NODE(const Invocation& input_invoc)
	{
		invoc = input_invoc;
		init();
	} 
	void init()
	{
		decideNext = Consensus();
		next = nullptr;
		seq = 0;
	}
public:
	Invocation invoc;
	Consensus decideNext;
	NODE* next;
	volatile int seq;
};
 
class LFUniversal {
private:
	NODE* head[MAX_THREADS];
	NODE* tail;
public:
	LFUniversal() {
		tail = new NODE();
		tail->seq = 1;
		for (int i = 0; i < MAX_THREADS; ++i) head[i] = tail;
	}
	void init()
	{
		NODE* p = tail->next;
		if (p == nullptr)
			return;

		while (p->next)
		{
			NODE* tmp = p;
			p = p->next;
			delete tmp;
		}
		delete p;

		for (int i = 0; i < MAX_THREADS; ++i)
			head[i] = tail;

		tail->init();
	}
	Response apply(Invocation invoc) {
		NODE* prefer = new NODE(invoc);
		// prefer가 성공적으로 head에 추가 되었는지 검사
		while (prefer->seq == 0) { 
			NODE* before = max(); 
			// Log의 head를 찾지만 다른 스레드와 겹쳐져서 잘 못 찾을 수도 있음

			NODE* after = before->decideNext.decide( prefer);
			// before의 합의는 항상 유일하다!

			before->next = after;
			after->seq = before->seq + 1;
			// 여러 스레드가 같은 작업을 반복 할 수 있지만, 상관없다.

			head[thread_id] = after;
			// 자신이 본 제일 앞의 head는 after니까 업데이트 시켜준다
			// 이러지 않으면 동일한 합의 객체를 두 번 호출할 수 있다.
			// 운좋게 after가 prefer가 되면 성공이다
		}
		SeqObject_Set myObject;
		NODE* current = tail->next;
		while (current != prefer) {
			myObject.apply(current->invoc);
			current = current->next;  
		}
		return myObject.apply(current->invoc);
	}

	NODE* max() {
		NODE* p = head[0];
		for (const auto ptr : head)
		{
			if (p->seq < ptr->seq)
			{
				p = ptr;
			}
		}

		return p;

		for (int i = 0; i < MAX_THREADS; ++i) {
			if (head[i]->seq > p->seq) {
				p = head[i];
			}
		}
		return p;
	}
};

class LFUniversalSet{
	LFUniversal m_set;

public: 
	void init() {
		m_set.init();
	}
	void add(int key) {
		m_set.apply({ INSERT, key} );
	}

	void remove(int key) {
		m_set.apply({ ERASE, key } );
	}
	void contains(int key) {
		m_set.apply({ CONTAIN, key } );
	}
	void verify() {
		m_set.apply({ VERIFY, 0 });
	} 
};
class MutexSet {
	set<int> m_set;
	mutex c_lock;
public:
	void init() {
		m_set.clear();
	}
	void add(int key) {
		c_lock.lock();
		m_set.insert(key);
		c_lock.unlock();
	}

	void remove(int key) {
		c_lock.lock();
		m_set.erase(key);
		c_lock.unlock();
	}
	void contains(int key) {
		c_lock.lock();
		m_set.find(key);
		c_lock.unlock();
	}
	void verify() {
		int count = 0;
		for (auto data : m_set) {
			if (count++ >= 20) {
				break;
			}
			cout << data << ", ";
		}
		cout << "\n";
	}
};
//MutexSet		myset;
LFUniversalSet myset;

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