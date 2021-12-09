

// �ǽ� 3
#include <tbb/parallel_for.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace tbb;
using namespace chrono;

atomic_int sum = 0;
// volatile int sum = 0; // ���� ����!

// 1�� �����, ���� ó�� �����带 �̿��ؼ�
int lab3()
{
	size_t n = 50000000;
	sum = 0;
	auto start_t = chrono::system_clock::now();

	vector <thread> ttt;
	for (int i = 0; i < 8; ++i) {
		ttt.emplace_back([&]() { for (int j = 0; j < 50000000 / 8; ++j)  sum += 2;});
	}

	for (auto& th : ttt) {
		th.join();
	} 

	auto end_t = chrono::system_clock::now();
	auto mill = chrono::duration_cast<chrono::milliseconds>(end_t - start_t);

	cout << "thread calc ";
	cout << "Sum = " << sum << "\n";
	cout << "time : " << mill.count() << "\n";
	return 0;
}

// 1�� �����, tbb�� �̿��ؼ�
int lab4()
{
	size_t n = 50000000;
	sum = 0;
	auto start_t = chrono::system_clock::now();

	parallel_for(size_t(0), n, [&](int i) { sum += 2; });

	auto end_t = chrono::system_clock::now();
	auto mill = chrono::duration_cast<chrono::milliseconds>(end_t - start_t);

	cout << "Parallel for ";
	cout << "Sum = " << sum << "\n";
	cout << "time : " << mill.count() << "\n";
	return 0;
}

#include <tbb/concurrent_unordered_map.h>
#include <unordered_map>
#include <tbb/concurrent_hash_map.h>
using StringTable = concurrent_hash_map<string, atomic_int>;

const size_t N = 50'000'000;
string Data[N];

int fill_data() {
	char ch;
	for (auto& s : Data) {
		int length = rand() % 5;
		for (int i = 0; i < length; ++i) {
			ch = ('A' + rand() % 26);
			s = s + ch;
		}
	}
}
// �ܾ� ī����, �̱� ������ ����
int counter_single_thread()
{
	unordered_map<string, int> table;
	
	for (auto& d : Data) {
		table[d]++;
	}

	//for (auto& t : table) {
	//	cout << t.first << " : " << t.second << ", ";
	//}
	//cout << "\n";

	return table["BCD"];
}

// �ܾ� ī����, tbb ���
int counter_tbb()
{
	concurrent_unordered_map <string, atomic_int> table;

	parallel_for(size_t(0), N, [&table](int i) {table[Data[i]]++; });

	//for (auto& t : table) {
	//	cout << t.first << " : " << t.second << ", ";
	//}
	//cout << "\n";
	return table["BCD"];
}

int counter_tbb_hash_map()
{
	concurrent_hash_map <string, atomic_int> table;

	parallel_for(size_t(0), N, [&table](int i) { 
		concurrent_hash_map <string, atomic_int>::accessor acc;
		table.insert(acc, Data[i]);
		acc->second++;
		});

	//for (auto& t : table) {
	//	cout << t.first << " : " << t.second << ", ";
	//}
	//cout << "\n";

	concurrent_hash_map <string, atomic_int>::const_accessor c_acc;
	table.find(c_acc, "BCD");
	return c_acc->second;
}
int lab5() {
	fill_data();

	auto start_t = system_clock::now();
	int result = counter_single_thread();
	auto end_t = system_clock::now();
	auto time_elapsed = duration_cast<milliseconds>(end_t - start_t);
	cout << "single thread result : " << result << " ";
	cout << "time : " << time_elapsed.count() << "\n";

	start_t = system_clock::now();
	result = counter_tbb();
	end_t = system_clock::now();
	time_elapsed = duration_cast<milliseconds>(end_t - start_t);
	cout << "tbb result : " << result << " ";
	cout << "time : " << time_elapsed.count() << "\n";


	start_t = system_clock::now();
	result = counter_tbb_hash_map();
	end_t = system_clock::now();
	time_elapsed = duration_cast<milliseconds>(end_t - start_t);
	cout << "tbb_hash_map result : " << result << " ";
	cout << "time : " << time_elapsed.count() << "\n";

	return 0;
}

#include <tbb/task_group.h>

// task_group lab
constexpr int FIB_MAX = 45;
int serial_fibo(int n) {
	if (n < 2) {
		return n;
	}
	return serial_fibo(n - 2) + serial_fibo(n - 1);
}

int parallel_fibo(int n)
{
	if (n < 30) {
		return serial_fibo(n);
	}

	int x, y;

	task_group g;
	g.run([&]() { x = parallel_fibo(n - 2); });
	g.run([&]() { y = parallel_fibo(n - 1); });

	g.wait();

	return x + y;
}

int lab6() { 
	auto start_t = system_clock::now();
	volatile int result = serial_fibo(FIB_MAX);
	auto end_t = system_clock::now();
	auto time_elapsed = duration_cast<milliseconds>(end_t - start_t);
	cout << "single fibo : " << result << " ";
	cout << "time : " << time_elapsed.count() << "\n";

	start_t = system_clock::now();
	result = parallel_fibo(FIB_MAX);
	end_t = system_clock::now();
	time_elapsed = duration_cast<milliseconds>(end_t - start_t);
	cout << "TBB fibo : " << result << " ";
	cout << "time : " << time_elapsed.count() << "\n";

	return 0;
}

int main() {
//	lab3();
//	lab4();
//	lab5();
	lab6();

	return 0;
}