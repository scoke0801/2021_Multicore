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

class SPNODE {
public:
	int key;
	//NODE* next;
	SPNODE*  next;
	mutex node_lock;
	bool marked;	// removed

	SPNODE() { next = nullptr; marked = false; }
	SPNODE(int key_value) { key = key_value; next = nullptr; marked = false; }
	~SPNODE() {}
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

		while (curr->key < x) {
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
		NODE* curr;

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
			curr->unlock();
			pred->unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;

			curr->unlock();
			pred->unlock();
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
			curr->unlock();
			pred->unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			curr->unlock();
			pred->unlock();
			delete curr;

			return true;
		}
	}

	bool contains(int x)
	{
		NODE* pred, * curr;

		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리

		head.lock();
		pred = &head;

		curr = pred->next;

		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
			curr->lock();
			pred->unlock();

			// lock, unlock의 순서가 중요
		}

		if (curr->key != x) {
			curr->unlock();
			pred->unlock();
			return false;
		}
		else {
			curr->unlock();
			pred->unlock();
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

class O_SET {
	NODE head, tail;
public:
	O_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}

	~O_SET() { init(); }
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

		// 가능하면 lock으로 잠기는 영역을 줄이는게 병렬성 측면에서 좋다.
		// 여기서부터 공유메모리

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
				pred->unlock();
				curr->unlock();
				return (curr->key == x); 
			}

			pred->unlock();
			curr->unlock();
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
	bool validate(NODE* pred, NODE* curr)
	{
		NODE* node = &head;
		while (node->key <= pred->key) {
			if (node == pred) {
				return pred->next == curr;
			}
			node = node->next;
		}
		return false;
	}
};


class L_SET {
	NODE head, tail;
public:
	L_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}

	~L_SET() { init(); }
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

class L_SET2 {
	shared_ptr <SPNODE> head;
	shared_ptr <SPNODE> tail;
public:
	L_SET2()
	{
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7fffffff);
		head->next = tail.get();
	}

	~L_SET2() { init(); }
	void init()
	{ 
		while (head->next != tail.get()) {
			SPNODE* p = head->next;
			head->next = p->next;
			delete p;
		}
	}

	bool add(int x)
	{
		shared_ptr<SPNODE> pred, curr;

		while (true) {
			pred = head;
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
//C_SET myset;
//F_SET myset;
//O_SET myset; 
L_SET myset;

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