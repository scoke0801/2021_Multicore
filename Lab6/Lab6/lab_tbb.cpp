

// 실습 3
#include <tbb/parallel_for.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace tbb;
using namespace chrono;

atomic_int sum = 0;
// volatile int sum = 0; // 오류 생김!

// 1억 만들기, 기존 처럼 쓰레드를 이용해서
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

// 1억 만들기, tbb를 이용해서
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
// 단어 카운터, 싱글 쓰레드 기준
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

// 단어 카운터, tbb 사용
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

int main() {
//	lab3();
//	lab4();
	
	lab5();

	return 0;
}