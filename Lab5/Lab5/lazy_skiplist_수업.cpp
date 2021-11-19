#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

constexpr long long LSB_MASK = 0xFFFFFFFE;

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

class SPNODE {
public:
	int value;
	shared_ptr <SPNODE> next;
	mutex node_lock;
	bool removed;
	SPNODE(int x) : value(x), next(nullptr), removed(false) {}
	SPNODE() : next(nullptr), removed(false) {}
	~SPNODE() {}
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

class C_SET {
	NODE head, tail;
	null_mutex c_lock;

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
		c_lock.lock();
		curr = head.next;
		while (curr->value < x) {
			curr = curr->next;
		}
		if (curr->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			c_lock.unlock();
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

class F_SET {
	NODE head, tail;

public:
	F_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~F_SET()
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
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->value < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (curr->value == x) {
			pred->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			pred->Unlock();
			curr->Unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->value < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (curr->value != x) {
			pred->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			curr->Unlock();
			pred->Unlock();
			delete curr;
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE* pred, * curr;
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->value < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}
		if (curr->value != x) {
			pred->Unlock();
			curr->Unlock();
			return false;
		}
		else {
			pred->Unlock();
			curr->Unlock();
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

class O_SET {
	NODE head, tail;

public:
	O_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~O_SET()
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
	bool Validate(NODE* pred, NODE* curr)
	{
		NODE* p = &head;
		while (p->value <= pred->value) {
			if (p == pred)
				return p->next == curr;
			p = p->next;
		}
		return false;
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value == x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = new_node;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value != x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					pred->next = curr->next;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Contains(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value != x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
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

class Z_SET {
	NODE head, tail;

public:
	Z_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~Z_SET()
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
	bool Validate(NODE* pred, NODE* curr)
	{
		return (pred->removed == false)
			&& (curr->removed == false)
			&& (pred->next == curr);
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value == x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = new_node;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value != x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					pred->next = curr->next;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Contains(int x)
	{
		NODE* curr;
		curr = head.next;
		while (curr->value < x) {
			curr = curr->next;
		}
		return (curr->value == x) && (curr->removed == false);
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

class SPZ_SET {
	shared_ptr<SPNODE> head, tail;

public:
	SPZ_SET()
	{
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7FFFFFFF);
		head->next = tail;
	}
	~SPZ_SET()
	{
		Init();
	}
	void Init()
	{
		head->next = tail;
	}
	bool Validate(const shared_ptr<SPNODE>& pred, const shared_ptr<SPNODE>& curr)
	{
		return (pred->removed == false)
			&& (curr->removed == false)
			&& (pred->next == curr);
	}
	bool Add(int x)
	{
		shared_ptr<SPNODE>  pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value == x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					shared_ptr<SPNODE> new_node = make_shared<SPNODE>(x);
					new_node->next = curr;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = new_node;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Remove(int x)
	{
		shared_ptr <SPNODE> pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->Lock(); curr->Lock();
			if (Validate(pred, curr) == true) {
				if (curr->value != x) {
					pred->Unlock();
					curr->Unlock();
					return false;
				}
				else {
					pred->next = curr->next;
					pred->Unlock();
					curr->Unlock();
					return true;
				}
			}
			pred->Unlock();
			curr->Unlock();
		}
	}
	bool Contains(int x)
	{
		shared_ptr <SPNODE> curr;
		curr = head->next;
		while (curr->value < x) {
			curr = curr->next;
		}
		return (curr->value == x) && (curr->removed == false);
	}

	void Verify()
	{
		shared_ptr<SPNODE> p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == tail) break;
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

class LFNODE {
public:
	int value;
	int next;
	LFNODE(int x) : value(x) {}
	LFNODE() {}
	~LFNODE() {}

	void set_next(LFNODE* p, bool removed)
	{
		next = reinterpret_cast<int>(p);
		if (true == removed)
			next++;
	}
	LFNODE* get_next()
	{
		return reinterpret_cast<LFNODE*>(next & LSB_MASK);
	}
	LFNODE* get_next(bool* removed)
	{
		int value = next;
		*removed = 1 == (value & 0x1);
		return reinterpret_cast<LFNODE*>(value & LSB_MASK);
	}
	bool CAS_NEXT(LFNODE* old_p, LFNODE* new_p, bool old_removed, bool new_removed)
	{
		int old_v = reinterpret_cast<int>(old_p);
		if (true == old_removed) old_v++;
		int new_v = reinterpret_cast<int>(new_p);
		if (true == new_removed) new_v++;
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&next), &old_v, new_v);
	}
	bool is_removed()
	{
		return 1 == (next & 0x1);
	}
};

class LF_SET {
	LFNODE head, tail;

public:
	LF_SET()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.set_next(&tail, false);
	}
	~LF_SET()
	{
		Init();
	}
	void Init()
	{
		while (head.get_next() != &tail) {
			LFNODE* p = head.get_next();
			head.set_next(p->get_next(), false);
			delete p;
		}
	}

	void Find(int x, LFNODE*& pred, LFNODE*& curr)
	{
		while (true) {
		retry:
			pred = &head;
			curr = pred->get_next();
			while (true) {
				// remove된 curr를 지운다.
				bool removed;
				LFNODE* succ = curr->get_next(&removed);
				while (true == removed) {
					bool ret = pred->CAS_NEXT(curr, succ, false, false);
					if (false == ret)
						goto retry;
					curr = succ;
					succ = curr->get_next(&removed);
				}
				if (curr->value >= x)
					return;
				pred = curr;
				curr = curr->get_next();
			}
		}
	}

	bool Add(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			Find(x, pred, curr);
			if (curr->value == x)
				return false;
			else {
				LFNODE* new_node = new LFNODE(x);
				new_node->set_next(curr, false);
				if (true == pred->CAS_NEXT(curr, new_node, false, false))
					return true;
			}
		}
	}
	bool Remove(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			Find(x, pred, curr);
			if (curr->value != x)
				return false;
			else {
				LFNODE* succ = curr->get_next();
				if (false == curr->CAS_NEXT(succ, succ, false, true))
					continue;
				pred->CAS_NEXT(curr, succ, false, false);
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		LFNODE* curr = head.get_next();
		while (curr->value < x) {
			curr = curr->get_next();
		}
		return (curr->value == x) && (curr->is_removed() == false);
	}

	void Verify()
	{
		LFNODE* p = head.get_next();
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->value << ", ";
			p = p->get_next();
		}
		cout << endl;
	}
};

constexpr int NUM_LEVEL = 10;
class SKNODE {
public:
	int value;
	SKNODE* volatile next[NUM_LEVEL];
	int top_level;			// - ~ NUM_LEVEL -1 
	volatile bool fully_linked;
	volatile bool is_removed;
	mutex nlock;
	SKNODE() : value(0), top_level(0), fully_linked(false), is_removed(false)
	{
		for (auto& n : next)
			n = nullptr;
	}
	SKNODE(int x, int top) : value(x), top_level(top), fully_linked(false), is_removed(false)
	{
		for (int i = 0; i <= top; ++i)
			next[i] = nullptr;
	}
};

class CSK_SET {
	SKNODE head, tail;
	null_mutex c_lock;

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
		for (auto& n : head.next)
			n = &tail;
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
			if (0 == curr_level)
				return;
			curr_level--;
			pred[curr_level] = pred[curr_level + 1];
		}
	}
	bool Add(int x)
	{
		SKNODE* pred[NUM_LEVEL], * curr[NUM_LEVEL];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value == x) {
			c_lock.unlock();
			return false;
		}
		else {
			int new_level = 0;
			for (int i = 0; i < NUM_LEVEL; ++i) {
				new_level = i;
				if (rand() % 2 == 0)
					break;
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
	bool Remove(int x)
	{
		SKNODE* pred[NUM_LEVEL], * curr[NUM_LEVEL];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			for (int i = 0; i <= curr[0]->top_level; ++i) {
				pred[i]->next[i] = curr[0]->next[i];
			}
			delete curr[0];
			c_lock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		SKNODE* pred[NUM_LEVEL], * curr[NUM_LEVEL];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			c_lock.unlock();
			return true;
		}
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
	std::recursive_mutex c_lock;
//	mutex c_lock;
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
		for (auto& n : head.next)
			n = &tail;
	}
	int Find(int x, SKNODE* pred[], SKNODE* curr[])
	{
		int level_found = -1;
		int curr_level = NUM_LEVEL - 1;
		pred[curr_level] = &head;
		while (true) {
			curr[curr_level] = pred[curr_level]->next[curr_level];
			while (curr[curr_level]->value < x) {
				pred[curr_level] = curr[curr_level];
				curr[curr_level] = curr[curr_level]->next[curr_level];
			}
			if ((level_found == -1) && (curr[curr_level]->value == x))
				level_found = curr_level;
			if (0 == curr_level)
				return level_found;
			curr_level--;
			pred[curr_level] = pred[curr_level + 1];
		}
	}
	bool Add(int x)
	{
		SKNODE* pred[NUM_LEVEL], * curr[NUM_LEVEL];
		c_lock.lock();
		
		Find(x, pred, curr);
		if (curr[0]->value == x) {
			c_lock.unlock();
			return false;
		}
		else {
			int new_level = 0;
			for (int i = 0; i < NUM_LEVEL; ++i) {
				new_level = i;
				if (rand() % 2 == 0)
					break;
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
	bool Remove(int x)
	{
		SKNODE* target;
		SKNODE* preds[NUM_LEVEL], * currs[NUM_LEVEL];

		int level_found = Find(x, preds, currs);
		if (-1 != level_found)
			target = currs[level_found];
		if ((-1 == level_found)
			|| (true == target->is_removed)
			|| (false == target->fully_linked)
			|| (level_found != target->top_level))
			return false;

		target->nlock.lock();
		if (true == target->is_removed) {
			target->nlock.unlock();
			return false;
		}
		target->is_removed = true;

		// 링크재조정
		while (true) {
			bool is_invalid = false;
			int max_lock_level = -1;
			for (int i = 0; i <= target->top_level; ++i) {
				preds[i]->nlock.lock();
				max_lock_level = i;
				if ((true == preds[i]->is_removed)
					|| (preds[i]->next[i] != target)) {
					is_invalid = true;
					break;
				}
			}
			if (true == is_invalid) {
				for (int i = 0; i <= max_lock_level; ++i)
					preds[i]->nlock.unlock();
				continue;
			}
			for (int i = 0; i <= target->top_level; ++i)
				preds[i]->next[i] = target->next[i];
			for (int i = 0; i <= max_lock_level; ++i)
				preds[i]->nlock.unlock();
			target->nlock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		SKNODE* pred[NUM_LEVEL], * curr[NUM_LEVEL];
		c_lock.lock();
		Find(x, pred, curr);
		if (curr[0]->value != x) {
			c_lock.unlock();
			return false;
		}
		else {
			c_lock.unlock();
			return true;
		}
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

void Benchmark(int num_threads)
{
	const int NUM_TEST = 4000000;
	const int RANGE = 2000;

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
	for (int i = 1; i <= 8; i = i * 2) {
		vector <thread> worker;
		myset.Init();
		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j)
			worker.emplace_back(Benchmark, i);
		for (auto& th : worker)
			th.join();
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;
		myset.Verify();
		cout << i << " threads   ";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count() << "ms.\n";
	}
}

