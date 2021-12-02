#include <omp.h>

#include <iostream>
using namespace std;

// Open MP 실습 1, thread 숫자, thread id 얻어오기
int lab1() {
	int num_threads;

#pragma omp parallel
	{
		int thread_id = omp_get_thread_num();

		cout << "hello world from thread[" << thread_id << "]\n";

		num_threads = omp_get_num_threads();
	}

	cout << "number of thread :" << num_threads << "\n";
	
	return 0;
}

//Open MP를 써서 1억 만들기 
int lab2() {
	int sum = 0;
	int g_num_threads = 0;
#pragma omp parallel
	{
		int num_threads = omp_get_num_threads();
		for (int i = 0; i < 50000000 / num_threads; ++i) {
			// critical은 내부적으로 lock으로 구현, c++의 mutex lock보다도 성능이 떨어짐
#pragma omp critical
			sum += 2;
		}
		g_num_threads = num_threads;
	}

	cout << "number of thread :" << g_num_threads << "\n";
	cout << "Sum = " << sum << "\n";
	return 0;
}

// 실습 3
#include <tbb/parallel_for.h>
#include <iostream>

using namespace std;
using namespace tbb;

int lab3()
{
	atomic_int sum = 0;
	
	size_t n = 50000000;

	parallel_for(size_t(0), n, [&sum](int i) { sum += 2; });

	cout << "Sum = " << sum << "\n";
}

int main() {
	lab3();
}