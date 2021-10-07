#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace chrono;

class NODE {
public:
	int key;
	NODE* next;
	mutex node_lock;

	NODE() { next = nullptr; }
	
	NODE(int key_value)
	{
		key = key_value;
		next = nullptr;
	} 
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
// coarsed
class C_SET {
	NODE head, tail;
	mutex c_lock;

	//null_mutex c_lock;

public:
	C_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}

	~C_SET() { init(); }

	void init()
	{
		NODE* p;
		while (head.next != &tail) {
			p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool add(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		
		c_lock.lock();
		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리

		curr = pred->next;
		
		while (curr->key < x){
			pred = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
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

	bool remove(int x)
	{
		NODE* pred, * curr;
		pred = &head;

		c_lock.lock(); 

		curr = pred->next;

		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->key != x) {
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

	bool contains(int x)
	{
		NODE *curr;
		
		c_lock.lock();
		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리
		curr = head.next;

		while (curr->key < x) { 
			curr = curr->next;
		}

		if (curr->key != x) {
			c_lock.unlock();
			return false;
		}
		else { 
			c_lock.unlock();
			return true;
		}
	}

	void verify()
	{
		NODE *p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}
}; 
 
class F_SET {
	NODE head, tail; 
public:
	F_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}

	~F_SET() { init(); }
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

		head.lock();
		pred = &head;
		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리

		curr = pred->next;
		curr->lock();
		while (curr->key < x) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->key == x) {
			pred->unlock();
			curr->unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = pred;

			pred->unlock();
			curr->unlock();
			return true;
		}
	}

	bool remove(int x)
	{
		NODE* pred, * curr;
		
		head.lock();
		pred = &head; 

		curr = pred->next;
		curr->lock();

		while (curr->key < x) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->key != x) {
			pred->unlock();
			curr->unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			curr->unlock();
			delete curr;

			pred->unlock();
			return true;
		}
	}

	bool contains(int x)
	{
		NODE* curr;

		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리
		curr = head.next;

		while (curr->key < x) {
			curr = curr->next;
		}

		if (curr->key != x) {
			curr->unlock();
			return false;
		}
		else {
			curr->unlock();
			return true;
		}
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
};

C_SET myset;
//F_SET myset;
// 
// 세밀한 동깋를 할 때,
// 검색 및 수정 모두에서 잠금이 필요

void Benchmark(int num_threads)
{
	const int NUM_TEST = 4000000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i){
		int x = rand() % RANGE;
		switch(rand() % 3){
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

		for(auto & th : worker) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		myset.verify();
		cout << i << " threads	";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << " ms\n"; 
	}
}