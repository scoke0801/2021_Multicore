#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

constexpr int LSB_MASK = 0xFFFFFFFE;

class NODE {
public:
	int value;
	NODE* volatile next;
	mutex node_lock;
	bool removed;
	NODE(int x) : value(x), next(nullptr), removed(false) {}
	NODE() : next(nullptr), removed(false) {}
	~NODE() {}
	void Lock()
	{
		node_lock.lock();
	}
	void Unlock()
	{
		node_lock.unlock();
	}
};
 
class null_mutex
{
public:
	void lock() {}
	void unlock() {}
};

#include <shared_mutex>

class C_SET {
	NODE head, tail;
	shared_mutex c_lock;

public:
	C_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~C_SET()
	{
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		c_lock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == x) {
			c_lock.unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			c_lock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		c_lock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			delete curr;
			c_lock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE* curr;
		c_lock.lock_shared();
		curr = head.next;
		while (curr->value < x) {
			curr = curr->next;
		}
		if (curr->value != x) {
			c_lock.unlock_shared();
			return false;
		}
		else {
			c_lock.unlock_shared();
			return true;
		}
	}
	void Verify()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

//#include <tbb/task.h>
//using namespace std;
//using namespace tbb;
//
//class FibTask : public task {
//public:
//	const long n;
//	long* const sum;
//	FibTask(long n_, long* sum_) : n(n_), sum(sum_)
//	{}
//	task* execute() { // Overrides virtual function task::execute 
//		if (n < CutOff) {
//			*sum = SerialFib(n);
//		}
//		else {
//			long x, y;
//			FibTask& a = *new(allocate_child()) FibTask(n - 1, &x);
//			FibTask& b = *new(allocate_child()) FibTask(n - 2, &y);
//			// Set ref_count to "two children plus one for the wait". 
//			set_ref_count(3);
//			// Start b running. 
//			spawn(b);
//			// Start a running and wait for all children (a and b). 
//			spawn_and_wait_for_all(a);
//			// Do the sum 
//			*sum = x + y;
//		}
//		return NULL;
//	}
//};
//
//long SerialFib(long n) {
//	if (n < 2)
//		return n;
//	else
//		return SerialFib(n - 1) + SerialFib(n - 2);
//}
//
//long ParallelFib(long n) {
//	long sum;
//	FibTask& a = *new(task::allocate_root()) FibTask(n, &sum);
//	task::spawn_root_and_wait(a);
//	return sum;
//}
C_SET myset;

void Benchmark(int num_threads)
{
	const int NUM_TEST = 400000;
	const int RANGE = 4000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 100) {
		case 0:case 1:case 2: myset.Add(x); break;
		case 3:case 4:case 5: myset.Remove(x); break;
		dafault: myset.Contains(x); break;
		}
	}
}

int main()
{
	/*auto start_t = high_resolution_clock::now();

	volatile int sum = SerialFib(44);

	auto end_t = high_resolution_clock::now();
	auto exec_t = end_t - start_t;

	cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms.\n";*/


	//for (int i = 1; i <= 8; i = i * 2) {
	//	vector <thread> worker;
	//	myset.Init();

	//	auto start_t = system_clock::now();

	//	for (int j = 0; j < i; ++j) {
	//		worker.emplace_back(Benchmark, i);
	//	}

	//	for (auto& th : worker) {
	//		th.join();
	//	}

	//	auto end_t = system_clock::now();
	//	auto exec_t = end_t - start_t;

	//	myset.Verify();

	//	cout << i << " threads   ";
	//	cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms.\n";
	//}
}

