#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace std;
using namespace chrono;

constexpr long long MAX_INT = 0x7FFFFFFFFFFFFFFF;
constexpr long long MIN_INT = 0x8000000000000000;

constexpr long long LSB_MASK = 0x7FFFFFFFFFFFFFFE; 
  
class NODE {
public:
	int key; 
	long long next; // 복합 포인터 

	NODE() {}
	NODE(int key_value) { key = key_value; }
	~NODE() {}

	void set_next(NODE* ptr, bool removed)
	{
		next = reinterpret_cast<long long>(ptr);
		if (removed) {
			next++;
			// LSB 조절
		}
	}
	NODE* get_next()
	{
		return reinterpret_cast<NODE*>(next & LSB_MASK);
	}
	NODE* get_next(bool *removed)
	{
		long long value = next;
		*removed = (1 ==(value & 0x1));
		return reinterpret_cast<NODE*>(value & LSB_MASK);
	} 

	bool CAS_NEXT(NODE* old_node, NODE* new_node, bool oldMark, bool newMark)
	{
		long long oldvalue = reinterpret_cast<long long>(old_node);
		if (oldMark) oldvalue = oldvalue | 0x01; // old_value++;
		else oldvalue = oldvalue & LSB_MASK;

		long long newvalue = reinterpret_cast<long long>(new_node); 
		if (newMark) newvalue = newvalue | 0x01; // new_value++;
		else newvalue = newvalue & LSB_MASK;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int64_t*>(&next), &oldvalue, newvalue);
	} 
	bool AttemptMark(NODE* node, bool mark)
	{
		long long oldvalue = reinterpret_cast<long long>(node);

		long long newvalue = reinterpret_cast<long long>(node);
		if (mark) newvalue = newvalue | 0x01; // new_value++;
		else newvalue = newvalue & LSB_MASK;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int64_t*>(&next), &oldvalue, newvalue); 
	}
}; 
  
class LF_SET {
	NODE head, tail;

public:
	LF_SET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.set_next(&tail, false);
	}

	~LF_SET() { init(); }
	void init()
	{
		while (head.get_next() != &tail) {
			NODE* p = head.get_next();
			head.set_next(p->get_next(), false);
			delete p;
		}
	}
	void FIND(int x, NODE*& pred, NODE*& curr)
	{
		while (true) {
		retry: 
			pred = &head;
			curr = pred->get_next();
			while (true) {
				// remove된 curr를 지운다...
				bool removed;
				NODE* succ = curr->get_next(&removed);
				while (true == removed) {
					bool ret = pred->CAS_NEXT(curr, succ, false, false);
					if (false == ret) {
						goto retry;
					}
					curr = succ;
					succ = curr->get_next(&removed);
				}

				if (curr->key >= x) {
					return;
				}

				pred = curr;
				curr = curr->get_next();
			}
		}
	}
	bool add(int x)
	{
		NODE* pred, * curr;
	 
		while (true) {
			FIND(x, pred, curr);

			if (curr->key == x) {
				return false;
			}
			else {
				NODE* node = new NODE(x);
				node->set_next(curr, false);

				if(pred->CAS_NEXT(curr, node, false, false)){
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
			FIND(x, pred, curr); 

			if (curr->key != x) {
				return false;
			}
			else {
				NODE* succ = curr->get_next();
				snip = curr->AttemptMark(succ, true);
				if (!snip) {
					continue;
				}
				if (pred->CAS_NEXT(curr, succ, false, false)) {
					return true;
				}
			}
		}
	}

	bool contains(int x)
	{
		NODE* pred, * curr;

		// 게으른 동기화를 통해 wait-free
		 
		bool marked[] = { false, };
		curr = &head;
		while (curr->key < x) {
			curr = curr->get_next();
			NODE* succ = curr->get_next(marked);
		}
		return (curr->key == x && !marked[0]);
	}

	void verify()
	{
		NODE* p = head.get_next();
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}
			cout << p->key << ", ";
			p = p->get_next();
		}
		cout << "\n";
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