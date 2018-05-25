// Copyright (c) 2012- PPSSPP Project / Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <cstdlib>

#include "Common/CommonTypes.h"

// Android
#if defined(__ANDROID__)
#include <sys/endian.h>

#if _BYTE_ORDER == _LITTLE_ENDIAN && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif _BYTE_ORDER == _BIG_ENDIAN && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// GCC 4.6+
#elif __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)

#if __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// LLVM/clang
#elif __clang__

#if __LITTLE_ENDIAN__ && !defined(COMMON_LITTLE_ENDIAN)
#define COMMON_LITTLE_ENDIAN 1
#elif __BIG_ENDIAN__ && !defined(COMMON_BIG_ENDIAN)
#define COMMON_BIG_ENDIAN 1
#endif

// MSVC
#elif defined(_MSC_VER) && !defined(COMMON_BIG_ENDIAN) && !defined(COMMON_LITTLE_ENDIAN)

#define COMMON_LITTLE_ENDIAN 1

#endif

// Worst case, default to little endian.
#if !COMMON_BIG_ENDIAN && !COMMON_LITTLE_ENDIAN
#define COMMON_LITTLE_ENDIAN 1
#endif

#ifdef _MSC_VER
inline unsigned long long bswap64(unsigned long long x) { return _byteswap_uint64(x); }
inline unsigned int bswap32(unsigned int x) { return _byteswap_ulong(x); }
inline unsigned short bswap16(unsigned short x) { return _byteswap_ushort(x); }
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/endian.h>
#ifdef __OpenBSD__
#define bswap16 swap16
#define bswap32 swap32
#define bswap64 swap64
#endif
#elif defined(__GNUC__)
#define bswap16 __builtin_bswap16
#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64
#else
// TODO: speedup
inline unsigned short bswap16(unsigned short x) { return (x << 8) | (x >> 8); }
inline unsigned int bswap32(unsigned int x) { return (x >> 24) | ((x & 0xFF0000) >> 8) | ((x & 0xFF00) << 8) | (x << 24); }
inline unsigned long long bswap64(unsigned long long x) { return ((unsigned long long)bswap32(x) << 32) | bswap32(x >> 32); }
#endif

template <typename T> struct swap_t {
	static_assert(sizeof(T) > 1, "swap_t used with a 1-Byte type");

private:
	T swapped;

	static T swap(T val) {
		switch (sizeof(T)) {
		case 2: *(u16 *)&val = bswap16(*(u16 *)&val); break;
		case 4: *(u32 *)&val = bswap32(*(u32 *)&val); break;
		case 8: *(u64 *)&val = bswap64(*(u64 *)&val); break;
		default: break;
		}
		return val;
	}

public:
	swap_t() {}
	swap_t(T val) : swapped(swap(val)) {}

	swap_t &operator=(T val) { return *this = swap_t(val); }

	swap_t &operator<<=(T val) { return *this = *this << val; }
	swap_t &operator>>=(T val) { return *this = *this >> val; }
	swap_t &operator&=(T val) { return *this = *this & val; }
	swap_t &operator|=(T val) { return *this = *this | val; }
	swap_t &operator^=(T val) { return *this = *this ^ val; }
	swap_t &operator++() { return *this += 1; }
	swap_t &operator--() { return *this -= 1; }

	T operator++(int) {
		T old = *this;
		*this += 1;
		return old;
	}

	T operator--(int) {
		T old = *this;
		*this -= 1;
		return old;
	}

	template <typename S> swap_t &operator+=(S val) { return *this = *this + val; }
	template <typename S> swap_t &operator-=(S val) { return *this = *this - val; }
	template <typename S> swap_t &operator*=(S val) { return *this = *this * val; }
	template <typename S> swap_t &operator/=(S val) { return *this = *this / val; }
	template <typename S> swap_t &operator%=(S val) { return *this = *this % val; }

	operator T() const { return swap(swapped); }
};

#if COMMON_LITTLE_ENDIAN
template <typename T> using LEndian = T;
template <typename T> using BEndian = swap_t<T>;
#else
template <typename T> using LEndian = swap_t<T>;
template <typename T> using BEndian = T;
#endif

typedef LEndian<u16> u16_le;
typedef LEndian<u32> u32_le;
typedef LEndian<u64> u64_le;

typedef LEndian<s16> s16_le;
typedef LEndian<s32> s32_le;
typedef LEndian<s64> s64_le;

typedef LEndian<float> float_le;
typedef LEndian<double> double_le;

typedef BEndian<u16> u16_be;
typedef BEndian<u32> u32_be;
typedef BEndian<u64> u64_be;

typedef BEndian<s16> s16_be;
typedef BEndian<s32> s32_be;
typedef BEndian<s64> s64_be;

typedef BEndian<float> float_be;
typedef BEndian<double> double_be;
