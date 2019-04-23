#include <cassert>
#include <iostream>
#include <vector>
#include <chrono>

//uncomment this to use std::allocator
//#define USE_STD_ALLOCATOR

#ifdef USE_STD_ALLOCATOR

template<typename V>
using Vector = std::vector<V>;

#else
#include "inblock_allocator.hpp"

struct holder {
	static inblock_allocator_heap heap;
};

inblock_allocator_heap holder::heap;

template<typename V>
using Vector = std::vector<V, inblock_allocator<V, holder>>;
#endif

using Vec = Vector<int>;
using Matrix = Vector<Vec>;

int ugly_dot(Vec a, Vec b)
{
	assert(a.size() == b.size());
	int res = 0;
	for (size_t i = 0; i < a.size(); ++i) res += a[i] * b[i];
	return res;
}

Matrix ugly_mult_matrix(Matrix a, Matrix b)
{
	Matrix c;
	for (auto&& i : a) {
		Vec tmp;
		for (auto&& j : b) tmp.push_back(ugly_dot(i, j));
		c.push_back(tmp);
	}
	return c;
}

#define SIZE 200

#define memsize (SIZE * SIZE * sizeof (int) * 4 * 10)

int main()
{
#ifndef USE_STD_ALLOCATOR
	std::vector<uint8_t> mem;
	mem.resize(memsize);

	holder::heap(mem.data(), memsize);
#endif
	auto start = std::chrono::system_clock::now();

	Matrix a;
	a.resize(SIZE);
	for (size_t i = 0; i < SIZE; ++i) a[i].resize(SIZE);
	Matrix b = a;

	srand(0x1337);
	for (size_t i = 0; i < SIZE; ++i)
		for (size_t j = 0; j < SIZE; ++j) {
			a[i][j] = rand() % 3;
			b[i][j] = rand() % 3;
		}

	a = ugly_mult_matrix(a, b);
	a = ugly_mult_matrix(a, b);
	a = ugly_mult_matrix(a, b);

	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	std::cout << elapsed_seconds.count() << std::endl;

	/*for (size_t i = 0; i < SIZE; ++i) {
		for (size_t j = 0; j < SIZE; ++j) std::cout << a[i][j] << ' ';
		std::cout << std::endl;
	}*/

	a.clear();
	b.clear();
	return 0;
}
