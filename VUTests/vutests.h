#pragma once

#include <stdlib.h>
#include <tamtypes.h>

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

struct PrintCOP2Status {
	char str[19];
	constexpr static const char FLAGS[6] = {'Z','S','U','O','I','D'};
	constexpr PrintCOP2Status(u32 reg): str{} {
		for (u32 i = 0; i < 6; i++) {
			str[i * 2 + 0] = (reg >> (i + 6)) & 1 ? FLAGS[i] : '-';
			str[i * 2 + 1] = (reg >> (i + 6)) & 1 ? 'S'      : '-';
			str[i + 12]    = (reg >> (i + 0)) & 1 ? FLAGS[i] : '-';
		}
		str[18] = '\0';
	}
};

struct PrintCOP2MAC {
	char str[5];
	constexpr static const char FLAGS[4] = {'Z','S','U','O'};
	constexpr PrintCOP2MAC(u32 reg, u32 idx): str{} {
		for (u32 i = 0; i < 4; i++) {
			str[i] = (reg >> (i * 4 + idx)) & 1 ? FLAGS[i] : '-';
		}
		str[4] = '\0';
	}
};

struct PrintCOP1Flags {
	char str[13];
	constexpr static const char FLAGS[4] = {'U','O','D','I'};
	constexpr PrintCOP1Flags(u32 data): str{} {
		for (u32 i = 0; i < 4; i++) {
			str[i * 2 + 0] = (data >> (i + 0)) & 1 ? 'S'      : '-';
			str[i * 2 + 1] = (data >> (i + 0)) & 1 ? FLAGS[i] : '-';
			str[i + 8]     = (data >> (i + 4)) & 1 ? FLAGS[i] : '-';
		}
		str[12] = '\0';
	}
};

static inline u32 processFlagsCOP1(u32 flags) {
	return ((flags >> 3) & 0xf) | ((flags >> 10) & 0xf0);
}

bool testClip(void);
bool testFlags(void);
bool testAdd(void);
bool testSub(void);
bool testMul(void);
bool testMAdd(void);
bool testMSub(void);
bool testDiv(void);
bool testSqrt(void);
bool testRSqrt(void);
