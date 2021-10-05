#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

atomic <int> sum;

volatile int cas_sum;
volatile int LOCK = 0;

atomic <bool> flags[8];
atomic <int> labels[8];

mutex myl;

void worker(int num_threads);
void worker_mutex(int num_threads);
void worker_bakery(int num_threads, int th_id);
void worker_cas(int num_threads);

void bakery_lock(int th_id, int th_count);
void bakery_unlock(int th_id);

bool CAS(volatile int* addr, int expected, int new_val);
void CAS_LOCK();
void CAS_UNLOCK();

int main()
{
	// Threads 1,2,4,8
	for (int i = 1; i <= 8; i *= 2) {
		sum = 0; 
		cas_sum = 0;
		vector <thread> workers;
		//시작 시간
		auto start_t = high_resolution_clock::now();

		for (int j = 0; j < i; ++j) {
			//workers.emplace_back(worker, i);
			//workers.emplace_back(worker_mutex, i);
			//workers.emplace_back(worker_bakery, i, j);
			workers.emplace_back(worker_cas, i);
		}
		for (auto& th : workers) {
			th.join();
		}

		// 종료 시간
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "number of threads = " << i;
		cout << "	exec_time = " << duration_cast<milliseconds>(du_t).count();
		cout << "	sum = " << sum << "		cas_sum = " << cas_sum << "\n";
	}
} 
void worker(int num_threads)
{
	const int loop_count = 5000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) { 
		sum = sum + 2; 
	}
}

void worker_mutex(int num_threads)
{
	const int loop_count = 5000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) { 
		myl.lock();
		sum = sum + 2;
		myl.unlock();
	}
}

void worker_bakery(int num_threads, int th_id)
{
	const int loop_count = 5000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) {
		bakery_lock(th_id, num_threads);
		sum = sum + 2;
		bakery_unlock(th_id);
	}
}

void worker_cas(int num_threads)
{
	const int loop_count = 5000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) {
		CAS_LOCK();
		cas_sum = cas_sum + 2;
		CAS_UNLOCK();
	}
}

void bakery_lock(int th_id, int th_count) 
{
	// 번호표 발급
	flags[th_id] = true;

	int max_label = labels[0];
	for (int i = 1; i < th_count; ++i) {
		if (max_label < labels[i]) {
			max_label = labels[i];
		}
	}
	labels[th_id] = max_label + 1;
	flags[th_id] = false;

	for (int i = 0; i < th_count; ++i) {
		while (flags[i]) { 
			std::this_thread::yield();
		}
		while (labels[i] != 0 &&
			((labels[i] < labels[th_id]) ||
				(labels[i] == labels[th_id] && i < th_id))) {
			std::this_thread::yield();
		}
	} 
}

void bakery_unlock(int th_id) 
{ 
	labels[th_id] = 0;
}

bool CAS(volatile int* addr, int expected, int new_val)
{
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr), &expected, new_val);
}

void CAS_LOCK()
{
	while (false == CAS(&LOCK, 0, 1)) {

	} 
}


void CAS_UNLOCK()
{
	CAS(&LOCK, 1, 0);
}
