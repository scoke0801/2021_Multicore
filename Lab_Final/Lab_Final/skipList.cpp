#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;


constexpr int NUM_LEVEL = 10;
class SKNODE {
public:
	int value;
	SKNODE* volatile next[NUM_LEVEL];
	int top_level;
	bool volatile fully_linked;
	bool volatile is_removed;

	std::recursive_mutex nlock;

	SKNODE() : value(0), top_level(0), fully_linked(false), is_removed(false)
	{ 
		for (int i = 0; i < NUM_LEVEL; ++i) {
			next[i] = nullptr;
		}
	}
	SKNODE(int x, int top) : value(x), top_level(top), fully_linked(false), is_removed(false)
	{ 
		for (int i = 0; i <= top; ++i) {
			next[i] = nullptr;
		}
	}
};

class CSK_SET {
	SKNODE head, tail;
	mutex c_lock;

public:
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
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	void Find(int x, SKNODE* pred[], SKNODE* curr[])
	{ 
		int curr_level = NUM_LEVEL - 1;
		pred[curr_level] = &head;

		while (true) {
			curr[curr_level] = pred[curr_level]->next[curr_level];
			while (curr[curr_level]->value < x) {
				pred[curr_level] = curr[curr_level];
				curr[curr_level] = curr[curr_level]->next[curr_level];
			}
			if (curr_level == 0) {
				return;
			}
			curr_level--;
			pred[curr_level] = pred[curr_level + 1];
		}
	}
	bool Add(int x)
	{ 
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];
		c_lock.lock();
		Find(x, preds, currs);

		if (currs[0]->value == x) {
			// 찾은 경우
			c_lock.unlock();
			return false;
		}
		else {
			int new_level = 0;
			for (int i = 0; i < NUM_LEVEL; ++i) {
				new_level == i;
				if (rand() % 2 == 0) {
					break;
				}
			}
			SKNODE* newNode = new SKNODE(x, new_level);
			for (int i = 0; i <= new_level; ++i) {
				preds[i]->next[i] = newNode;
				newNode->next[i] = currs[i];
			}
			c_lock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{ 
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];

		c_lock.lock();

		Find(x, preds, currs);

		if (currs[0]->value == x) {
			// 찾았다 삭제! 
			for (int i = 0; i <= currs[0]->top_level; ++i) {
				preds[i]->next[i] = currs[i]->next[i];
			}
			delete currs[0];
			c_lock.unlock();
			return true;
		}
		else {
			c_lock.unlock();
			return false;
		}
	}
	bool Contains(int x)
	{ 
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];

		c_lock.lock();
		
		Find(x, preds, currs);

		bool result = (currs[0]->value == x);
	
		c_lock.unlock();

		return result;
	}
	void Verify()
	{
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next[0];
		}
		cout << endl;
	}
};

class ZSK_SET {
	SKNODE head, tail; 

public:
	ZSK_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next)
			n = &tail;
	}
	~ZSK_SET()
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
		for (auto& n : head.next) {
			n = &tail;
		}
	}
	int Find(int x, SKNODE* pred[], SKNODE* curr[])
	{
		int cl = NUM_LEVEL - 1;
		int lf = -1;
		pred[cl] = &head;

		while (true) {
			curr[cl] = pred[cl]->next[cl];

			while (curr[cl]->value < x) {
				pred[cl] = curr[cl];
				curr[cl] = curr[cl]->next[cl];
			}
			if ((lf == -1) && (curr[cl]->value == x)) {
				lf = cl;
			}

			if (cl == 0) {
				return lf;
			}

			cl--;
			pred[cl] = pred[cl + 1];
		}

	}
	bool Add(int x)
	{
		SKNODE* target = nullptr;

		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];

		int nl;
		for (int i = 0; i < NUM_LEVEL; ++i) {
			nl = i;
			if (rand() % 2) {
				break;
			}
		}

		while (true) {
			int lf = Find(x, preds, currs);
			
			if (lf != -1) {
				SKNODE* nodeFound = currs[lf];
				if (false == nodeFound->is_removed) {
					while (false == nodeFound->fully_linked)
					{
					}
					return false;
				}
				continue;
			}
			
			
			// 유효성 검사
			bool valid = false;
			int ml = -1;
			for (int i = 0; valid && (i <= nl); ++i) {
				SKNODE* pred = preds[i];
				SKNODE* curr = currs[i];
				pred->nlock.lock();

				ml = i;

				valid = (false == pred->is_removed) && (false == curr->is_removed)
					&& (pred->next[i] == curr);
			}

			if (valid == false) {
				for (int i = 0; i <= ml; ++i) {
					preds[i]->nlock.unlock();
				}
				continue;
			}

			// 노드 연결
			SKNODE* newNode = new SKNODE(x, nl);

			for (int i = 0; i <= nl; ++i) {
				preds[i]->next[i] = newNode;
			}

			for (int i = 0; i <= nl; ++i) {
				newNode->next[i] = currs[i];
			}

			newNode->fully_linked = true;
			for (int i = 0; i <= ml; ++i) {
				preds[i]->nlock.unlock();
			}
			return true;
		}
	}
	bool Remove(int x)
	{
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];
		SKNODE* target;
		
		int lf = Find(x, preds, currs);

		if (lf == -1) {
			return false;
		}

		target = currs[lf];

		if( (lf == -1 )
			|| (true == target->is_removed)
			|| (false == target->fully_linked)
			|| (lf != target->top_level)) {
			return false;
		}

		target->nlock.lock();

		if(target->is_removed){
			target->nlock.unlock();
			return false;
		}

		target->is_removed = true;
		while (true) {
			// 유효성 검사
			int ml = -1;
			bool is_invalid = false;

			for (int i = 0; i <= target->top_level; ++i) {
				SKNODE* pred = preds[i]; 

				ml = i;

				pred->nlock.lock();

				if ((true == pred->is_removed) || (pred->next[i] != target)) {
					is_invalid = true;
					break;
				}
			}
			if (is_invalid) {
				for (int i = 0; i <= ml; ++i) {
					preds[i]->nlock.unlock();
				}
				Find(x, preds, currs);
				continue;
			}

			// 노드 연결 해제
			for (int i = 0; i <= target->top_level; ++i) {
				preds[i]->next[i] = target->next[i];
			}
			
			for (int i = 0; i <= ml; ++i) {
				preds[i]->nlock.unlock();
			}
			
			target->nlock.unlock();
			  
			return true;
		}

	}
	bool Contains(int x)
	{
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];
		
		int lf = Find(x, preds, currs);

		return (lf != -1) && (preds[lf]->is_removed == false) && (currs[lf]->is_removed == false);
	}
	void Verify()
	{
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->next[0];
		}
		cout << endl;
	}
};

//CSK_SET myset;
ZSK_SET myset;
//LFSK_SET myset;

void Benchmark(int num_threads)
{
	const int NUM_TEST = 4000000;
	const int RANGE = 1000;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 3) {
		case 0: myset.Add(x); break;
		case 1: myset.Remove(x); break;
		case 2: myset.Contains(x); break;
		}
	}
}

int main()
{
	for (int i = 1; i <= 16; i = i * 2) {
		vector <thread> worker;
		myset.Init();

		auto start_t = system_clock::now();

		for (int j = 0; j < i; ++j) {
			worker.emplace_back(Benchmark, i);
		}

		for (auto& th : worker) {
			th.join();
		}

		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		myset.Verify();

		cout << i << " threads   ";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms.\n";
	}
}

