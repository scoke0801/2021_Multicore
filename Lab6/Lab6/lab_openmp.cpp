#include <omp.h>

#include <iostream>
using namespace std;

// Open MP �ǽ� 1, thread ����, thread id ������
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

//Open MP�� �Ἥ 1�� ����� 
int lab2() {
	int sum = 0;
	int g_num_threads = 0;
#pragma omp parallel
	{
		int num_threads = omp_get_num_threads();
		for (int i = 0; i < 50000000 / num_threads; ++i) {
			// critical�� ���������� lock���� ����, c++�� mutex lock���ٵ� ������ ������
#pragma omp critical
			sum += 2;
		}
		g_num_threads = num_threads;
	}

	cout << "number of thread :" << g_num_threads << "\n";
	cout << "Sum = " << sum << "\n";
	return 0;
}

// �ǽ� 3
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