/*=====================================================================
BitUtils.h
----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <cstring>
#include <assert.h>
#ifdef _WIN32
#include <intrin.h>
#endif


// See http://www.forwardscattering.org/post/27
// NOTE: put in some more suitable header file?
template <class Dest, class Src> 
GLARE_STRONG_INLINE Dest bitCast(const Src& x)
{
	static_assert(sizeof(Src) == sizeof(Dest), "sizeof(Src) == sizeof(Dest)");
	Dest d;
	std::memcpy(&d, &x, sizeof(Dest));
	return d;
}


/*=====================================================================
BitUtils
-------------------
Bit manipulation / counting functions.
=====================================================================*/
namespace BitUtils
{

	// Return the zero-based index of the least-significant bit in x that is set to 1.  If no bits are set to 1 (e.g. if x=0), then returns 0.
	inline uint32 lowestSetBitIndex(uint32 x);
	inline uint32 lowestSetBitIndex(uint64 x);

	// Return the zero-based index of the least-significant bit in x that is set to 0.  If no bits are set to 0 (e.g. if x=2^32-1), then returns 0.
	inline uint32 lowestZeroBitIndex(uint32 x);
	inline uint32 lowestZeroBitIndex(uint64 x);


	inline uint32 highestSetBitIndex(uint64 x);


	template <class T> 
	inline bool isBitSet(const T x, const T bitflag);

	// Sets the bit to 1, in place.
	template <class T> 
	inline void setBit(T& x, const T bitflag);

	// Sets the bit to 0, in place.
	template <class T> 
	inline void zeroBit(T& x, const T bitflag);

	// Sets the bit to 1 or 0 depending on should_set, in place.
	template <class T> 
	inline void setOrZeroBit(T& x, const T bitflag, bool should_set);


	void test();


	//==================================== Implementation ====================================

	uint32 lowestSetBitIndex(uint32 x)
	{
		if(x == 0)
			return 0;

#ifdef _WIN32
		// _BitScanForward: Search the mask data from least significant bit (LSB) to the most significant bit (MSB) for a set bit (1).
		// http://msdn.microsoft.com/en-us/library/wfd9z0bb(v=vs.80).aspx
		unsigned long pos;
		_BitScanForward(&pos, x);
		return pos;
#else
		// __builtin_ctz: Returns the number of trailing 0-bits in x, starting at the least significant bit position. If x is 0, the result is undefined.  (http://gcc.gnu.org/onlinedocs/gcc-4.4.5/gcc/Other-Builtins.html)
		return __builtin_ctz(x);
#endif
	}


	uint32 lowestSetBitIndex(uint64 x)
	{
		if(x == 0)
			return 0;

#ifdef _WIN32
		// _BitScanForward: Search the mask data from least significant bit (LSB) to the most significant bit (MSB) for a set bit (1).
		// https://docs.microsoft.com/en-us/cpp/intrinsics/bitscanforward-bitscanforward64?view=msvc-170
		unsigned long pos;
		_BitScanForward64(&pos, x);
		return pos;
#else
		// __builtin_ctz: Returns the number of trailing 0-bits in x, starting at the least significant bit position. If x is 0, the result is undefined.  (http://gcc.gnu.org/onlinedocs/gcc-4.4.5/gcc/Other-Builtins.html)
		return __builtin_ctz(x);
#endif
	}


	uint32 lowestZeroBitIndex(uint32 x)
	{
		return lowestSetBitIndex(~x); // Negate bits of x.
	}


	uint32 lowestZeroBitIndex(uint64 x)
	{
		return lowestSetBitIndex(~x); // Negate bits of x.
	}


	// Undefined if x == 0.  Index is from right (least significant bit)
	inline uint32 highestSetBitIndex(uint64 x)
	{
		assert(x != 0);
#ifdef _WIN32
		unsigned long pos;
#ifdef _WIN64
		// _BitScanReverse64: Search the mask data from most significant bit (MSB) to least significant bit (LSB) for a set bit (1).
		_BitScanReverse64(&pos, x);
		return pos;
#else
		uint32 parts[2];
		std::memcpy(parts, &x, sizeof(uint64));
		if(parts[1] != 0) // If there is a bit set in the most signficant 4 bytes (NOTE: this is only correct for little-endian systems)
		{
			_BitScanReverse(&pos, parts[1]);
			return pos + 32;
		}
		else
		{
			_BitScanReverse(&pos, parts[0]);
			return pos;
		}
#endif // _WIN64
#else
		return 63 - __builtin_clzl(x); // Returns the number of leading 0-bits in x, starting at the most significant bit position. If x is 0, the result is undefined.
#endif
	}


	template <class T>
	bool isBitSet(const T x, const T bitflag)
	{
		return (x & bitflag) != 0;
	}


	template <class T> 
	void setBit(T& x, const T bitflag)
	{
		x = x | bitflag;
	}


	template <class T> 
	void zeroBit(T& x, const T bitflag)
	{
		x = x & ~bitflag; // AND with the bitwise negation of the bitflag.
	}


	// Sets the bit to 1 or 0 depending on should_set, in place.
	template <class T> 
	void setOrZeroBit(T& x, const T bitflag, bool should_set)
	{
		if(should_set)
			setBit(x, bitflag);
		else
			zeroBit(x, bitflag);
	}


} // end namespace BitUtils
