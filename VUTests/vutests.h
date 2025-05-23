#pragma once

#include <stdlib.h>

template <typename T>
struct ArrayRef {
	T* data;
	size_t size;

	constexpr ArrayRef(): data(nullptr), size(0) {}
	constexpr ArrayRef(T* data_, size_t size_): data(data_), size(size_) {}
	template <size_t N>
	constexpr ArrayRef(T(&arr)[N]): data(arr), size(N) {}

	T* begin() const { return data; }
	T* end() const { return data + size; }
};

bool testClip(void);
bool testFlags(void);
bool testAdd(void);
bool testSub(void);
bool testMul(void);
bool testMAdd(void);
bool testMSub(void);
