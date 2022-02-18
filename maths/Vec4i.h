/*=====================================================================
Vec4i.h
-------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "../maths/SSE.h"
#include "../utils/MemAlloc.h"
#include <string>


/*=====================================================================
Vec4i
-----
SSE 4-vector of 32 bit signed integers.
=====================================================================*/
class Vec4i
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	GLARE_STRONG_INLINE Vec4i() {}
	GLARE_STRONG_INLINE explicit Vec4i(int32 x_, int32 y_, int32 z_, int32 w_) : v(_mm_set_epi32(w_, z_, y_, x_)) {}
	GLARE_STRONG_INLINE Vec4i(__m128i v_) : v(v_) {}
	GLARE_STRONG_INLINE explicit Vec4i(int32 x) : v(_mm_set1_epi32(x)) {}

	const std::string toString() const;

	GLARE_STRONG_INLINE int32& operator [] (unsigned int index) { return x[index]; }
	GLARE_STRONG_INLINE const int32& operator [] (unsigned int index) const { return x[index]; }

	GLARE_STRONG_INLINE void operator += (const Vec4i& a);

	static void test();

	union
	{
		int32 x[4];
		__m128i v;
	};
};


void Vec4i::operator += (const Vec4i& a)
{
	v = _mm_add_epi32(v, a.v);
}


inline bool operator == (const Vec4i& a, const Vec4i& b)
{
	// NOTE: have to use _mm_cmpeq_epi32, not _mm_cmpeq_ps, since _mm_cmpeq_ps gives different results for NaN bit patterns.
	return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(a.v, b.v))) == 0xF;
}


inline bool operator != (const Vec4i& a, const Vec4i& b)
{
	return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(a.v, b.v))) != 0xF;
}


// Loads x into elem 0 of a Vec4i, and zeroes the other elements.
GLARE_STRONG_INLINE Vec4i loadIntToLowElem(int32 x)
{
	return _mm_cvtsi32_si128(x);
}


GLARE_STRONG_INLINE const Vec4i loadVec4i(const void* const data) // SSE 2
{
	assert(((uint64)data % 16) == 0); // Must be 16-byte aligned.
	return Vec4i(_mm_load_si128((__m128i const*)data));
}


GLARE_STRONG_INLINE void storeVec4i(const Vec4i& v, void* const mem)
{
	assert(((uint64)mem % 16) == 0); // Must be 16-byte aligned.
	_mm_store_si128((__m128i*)mem, v.v);
}


template <int index>
GLARE_STRONG_INLINE const Vec4i copyToAll(const Vec4i& a) { return _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a.v), _mm_castsi128_ps(a.v), _MM_SHUFFLE(index, index, index, index))); } // SSE 1

#if COMPILE_SSE4_CODE
template<int index>
GLARE_STRONG_INLINE int elem(const Vec4i& v) { return _mm_extract_epi32(v.v, index); } // SSE 4: pextrd
#endif // COMPILE_SSE4_CODE

#if COMPILE_SSE4_CODE
GLARE_STRONG_INLINE const Vec4i mulLo(const Vec4i& a, const Vec4i& b) { return _mm_mullo_epi32(a.v, b.v); } // // Returns lower 32 bits of 64-bit product.  SSE 4
#endif // COMPILE_SSE4_CODE
GLARE_STRONG_INLINE const Vec4i operator + (const Vec4i& a, const Vec4i& b) { return _mm_add_epi32(a.v, b.v); } // SSE 2
GLARE_STRONG_INLINE const Vec4i operator - (const Vec4i& a, const Vec4i& b) { return _mm_sub_epi32(a.v, b.v); } // SSE 2

GLARE_STRONG_INLINE const Vec4i operator ^ (const Vec4i& a, const Vec4i& b) { return _mm_xor_si128(a.v, b.v); } // SSE 2
GLARE_STRONG_INLINE const Vec4i operator & (const Vec4i& a, const Vec4i& b) { return _mm_and_si128(a.v, b.v); } // SSE 2

GLARE_STRONG_INLINE const Vec4i operator << (const Vec4i& a, const int32 bits) { return _mm_slli_epi32(a.v, bits); } // SSE 2
//GLARE_STRONG_INLINE const Vec4i operator >> (const Vec4i& a, const int32 bits) { return _mm_srai_epi32(a.v, bits); } // SSE 2 // Shift right while shifting in sign bits
GLARE_STRONG_INLINE const Vec4i operator >> (const Vec4i& a, const int32 bits) { return _mm_srli_epi32(a.v, bits); } // SSE 2 // Shift right while shifting in zeros

GLARE_STRONG_INLINE Vec4i operator < (const Vec4i& a, const Vec4i& b) { return Vec4i(_mm_cmplt_epi32(a.v, b.v)); } // SSE 2



#if COMPILE_SSE4_CODE
GLARE_STRONG_INLINE Vec4i min(const Vec4i& a, const Vec4i& b) { return Vec4i(_mm_min_epi32(a.v, b.v)); } // SSE 4
GLARE_STRONG_INLINE Vec4i max(const Vec4i& a, const Vec4i& b) { return Vec4i(_mm_max_epi32(a.v, b.v)); } // SSE 4

GLARE_STRONG_INLINE Vec4i clamp(const Vec4i& v, const Vec4i& a, const Vec4i& b) { return max(a, min(v, b)); }
#endif // COMPILE_SSE4_CODE


inline const std::string toString(const Vec4i& v) { return v.toString(); }
