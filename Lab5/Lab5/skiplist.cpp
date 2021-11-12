#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

constexpr int MAX_THREADS = 16;

using namespace std;
using namespace chrono;


class NODE {
public:
	int key;

	// stack은 volatile을 사용하지 않아도 됨.. (일단)
	//NODE* volatile next; // 복합 포인터 
	NODE* next; // 복합 포인터 

	NODE() {}
	NODE(int key_value) { key = key_value; next = nullptr; }
	~NODE() {}
};

class null_mutex {
public:
	void lock() {}
	void unlock() {}
};

constexpr int MAX_LEVEL = 10;

class SKNODE
{
public:
	int value;
	SKNODE* next[MAX_LEVEL];
	int top_level;
	SKNODE()
	{
		value = 0;
		for (auto i = 0; i <= MAX_LEVEL; ++i)
			next[i] = nullptr;
		top_level = 0;
	}
	SKNODE(int x, int top)
	{
		value = x;
		
		for (int i = 0; i <= top; ++i) {
			next[i] = nullptr;
		}
		top_level = top;
	}
};


class CSK_SET {
public:
	SKNODE head, tail;
	mutex c_lock;

	CSK_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next)
			n = &tail;
	}

	~CSK_SET()
	{
		Init();
	}
	void Init()
	{
		while (head.next[0] != &tail) {
			SKNODE* p = head.next[0];
			head.next[0] = p->next[0];
			delete p;
		}
		for (auto& n : head.next)
			n = &tail;
	}
	bool add(int x)
	{
		SKNODE* pred[MAX_LEVEL], * curr[MAX_LEVEL];

		c_lock.lock();
		Find(x, pred, curr);

		if (curr[0]->value == x) {
			c_lock.unlock();
			return false;
		}
		else {
			int new_level = 0;
			for (int i = 0; i < MAX_LEVEL; ++i) {

				new_level = i;
				if ((rand() % 2) == 0) {
					break;
				}
			}
			SKNODE* new_node = new SKNODE(x, new_level);
			for (int i = 0; i <= new_level; ++i) {
				new_node->next[i] = curr[i];
				pred[i]->next[i] = new_node;
			}
			c_lock.unlock();
			return true;
		}
	}

	bool remove(int x)
	{
		SKNODE* pred[MAX_LEVEL], * curr[MAX_LEVEL];
		 
		c_lock.lock();

		Find(x, pred, curr);

		if (curr[0]->value == x) {
			SKNODE* delNode = curr[0];

			for (int i = 0; i < MAX_LEVEL; ++i) {
				if (pred[i]->next[i] == delNode) {
					pred[i]->next[i] = delNode->next[i];
				}
			}

			delete delNode;

			c_lock.unlock();
			return true;
		}
		else { 
			c_lock.unlock();
			return true;
		}
	}

	bool contains(int x)
	{
		SKNODE* pred[MAX_LEVEL], * curr[MAX_LEVEL];

		c_lock.lock();

		Find(x, pred, curr);

		bool result = curr[0]->value == x;
		c_lock.unlock();

		return result;
	}
	void verify()
	{
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next[0];
		}
		cout << endl;
	}

	void Find(int value, SKNODE* pred[], SKNODE* curr[])
	{ 
		int curr_level = MAX_LEVEL - 1;
		pred[curr_level] = &head;
		while (true) {
			curr[curr_level] = pred[curr_level]->next[curr_level];
			while (curr[curr_level]->value < value) {
				pred[curr_level] = curr[curr_level];
				curr[curr_level] = curr[curr_level]->next[curr_level];
			}
			if (0 == curr_level)
				return;
			curr_level--;
			pred[curr_level] = pred[curr_level + 1];
		} 
	} 
};

CSK_SET myset;

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
		myset.Init();
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