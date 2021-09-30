#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace std;
using namespace chrono;

atomic <int> sum;

atomic <bool> flags[8];
atomic <int> labels[8];

mutex myl; 
void worket_bakery(int num_threads, int th_id);
 
void bakery_lock(int th_id, int th_count);
void bakery_unlock(int th_id);

int main()
{
	// Threads 1,2,4,8
	for (int i = 1; i <= 8; i *= 2) {
		sum = 0; 
		vector <thread> workers;
		//���� �ð�
		auto start_t = high_resolution_clock::now();

		for (int j = 0; j < i; ++j) {  
			workers.emplace_back(worket_bakery, i, j);
		}
		for (auto& th : workers) {
			th.join();
		}

		// ���� �ð�
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "number of threads = " << i;
		cout << "	exec_time = " << duration_cast<milliseconds>(du_t).count();
		cout << "	sum = " << sum << "\n";
	}
} 
void worket_bakery(int num_threads, int th_id)
{
	const int loop_count = 5000000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) {
		bakery_lock(th_id, num_threads);
		sum = sum + 2;
		bakery_unlock(th_id);
	}
}

void bakery_lock(int th_id, int th_count)

{
	// ��ȣǥ �߱�
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
		while (flags[i]){
			// ��ȣǥ�� ���� ������ ���
		}

		// ��ȣǥ�� ���� ���
		while (labels[i] != 0) {
			if (labels[i] > labels[th_id]) {
				// ������ ���ƿ� ��� 
				break;
			}
			else if (labels[i] < labels[th_id]) {
				// ������ ���ƿ��� ���� ���
				// ���
				continue;
			}
			else {
				// ���� ������ ���
				if (i < th_id) {
					// �� ���� ������id�� ���� ���� �켱�ؼ� �۾��ϵ��� 
					// ���
					continue;
				}
				else {
					break;
				}
			}
		}
	}
}

void bakery_unlock(int th_id) 
{ 
	labels[th_id] = 0;
}
