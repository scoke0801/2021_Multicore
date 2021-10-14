#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
using namespace std;
using namespace chrono;

#define ADD_VALUE 2
#define DST_VALUE 100'000'000
#define TASK_COUNT DST_VALUE / ADD_VALUE
#define THREAD_NUM 8

#define TASK_COUNT_PER_THREAD TASK_COUNT / THREAD_NUM

volatile int sum = 0;
volatile bool flag[THREAD_NUM];
volatile int label[THREAD_NUM];

bool b_lockKeepCondition(int t_id)
{
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		if (t_id == i) continue;			

		if (flag[i] && (label[i] < label[t_id] || (label[i] == label[t_id] && i < t_id)))
			return true;
	}
	return false;
}

void b_lock(const int t_id)		// 빵집 알고리즘을 사용한 락
{
	int other = 1 - t_id;
	flag[t_id] = true;

	int max = 0;
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		if (label[i] > max)
			max = label[i];
	}
	label[t_id] = max + 1;
	while (b_lockKeepCondition(t_id));
}

void b_unlock(const int t_id)
{
	flag[t_id] = false;
}

void workerWithBakery(int t_id)
{	
	for (int i = 0; i < TASK_COUNT_PER_THREAD; ++i)
	{
		b_lock(t_id);
		sum += ADD_VALUE;
		b_unlock(t_id);
	}
}

void TestMultiThreadWithBakery()
{
	std::vector<thread> mythreads;
	for (int i = 0; i < THREAD_NUM; ++i)
	{
		mythreads.emplace_back(workerWithBakery, i);
	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		mythreads[i].join();
	}
}

int main()
{
	memset((void*)flag, 0, sizeof(bool) * THREAD_NUM);
	memset((void*)label, 0, sizeof(int) * THREAD_NUM);

	auto start_t = high_resolution_clock::now();

	TestMultiThreadWithBakery();

	auto end_t = high_resolution_clock::now();
	auto exec_time = end_t - start_t;
	cout << "Exec Time : " << duration_cast<milliseconds>(exec_time).count() << "ms\n";
	cout << "Sum = " << sum << endl;
}