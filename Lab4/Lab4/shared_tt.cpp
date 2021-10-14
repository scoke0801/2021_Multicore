#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace std;
using namespace chrono;
 
class SPNODE {
public:
	int key;
	//NODE* next;
	shared_ptr<SPNODE> next;
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

// �̱۽����带 �׽�Ʈ�ϱ� ����
// �Ʒ��� null_mutex��ü�� �̿���.
class null_mutex
{
public:
	void lock() {}
	void unlock() {}
}; 
class SPZLIST {
	shared_ptr <SPNODE> head;
	shared_ptr <SPNODE> tail;
public:
	SPZLIST()
	{
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7fffffff);
		head->next = tail;
	}

	~SPZLIST() { init(); }
	void init()
	{
		head->next = tail;
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
					shared_ptr<SPNODE> new_node = make_shared<SPNODE>(x);
					new_node->next = curr;
					//atomic_thread_fence(memory_order_seq_cst);
					pred->next = new_node;
					// ���� ��� ���������� �ٲ�ٸ�
					// ������ �߻�

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
		shared_ptr<SPNODE> pred, curr;

		// ������ ����ȭ�� ���� wait-free

		// �����ϸ� lock���� ���� ������ ���̴°� ���ļ� ���鿡�� ����.
		// ���⼭���� �����޸�
		 
		pred = head;
		curr = pred->next;

		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}
		return curr->key == x && !curr->marked;
	}

	void verify()
	{
		shared_ptr<SPNODE> p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == tail) {
				break;
			}
			cout << p->key << ", ";
			p = p->next;
		}
		cout << "\n";
	}
	bool validate(const shared_ptr<SPNODE>& pred, const shared_ptr<SPNODE>& curr)
	{
		return !pred->marked && !curr->marked &&
			(pred->next == curr);
	}
};

SPZLIST myset;

// ������ ����ȭ�� �� ��,
// �˻� �� ���� ��ο��� ����� �ʿ�

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