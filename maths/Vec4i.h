/*=====================================================================
Vec4i.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-10 21:02:10 +0000
=====================================================================*/
#pragma once


#include "../maths/SSE.h"
#include "../utils/Platform.h"
#include <string>


/*=====================================================================
Vec4i
-------------------

=====================================================================*/
class Vec4i
{
public:
	INDIGO_STRONG_INLINE Vec4i() {}
	INDIGO_STRONG_INLINE explicit Vec4i(int32 x_, int32 y_, int32 z_, int32 w_) : v(_mm_set_epi32(w_, z_, y_, x_)) {}
	INDIGO_STRONG_INLINE Vec4i(__m128i v_) : v(v_) {}
	INDIGO_STRONG_INLINE explicit Vec4i(int32 f) : v(_mm_set1_epi32(f)) {}

	const std::string toString() const;

	INDIGO_STRONG_INLINE int32& operator [] (unsigned int index) { return x[index]; }
	INDIGO_STRONG_INLINE const int32& operator [] (unsigned int index) const { return x[index]; }

	static void test();

	union
	{
		int32 x[4];
		__m128i v;
	};

};


inline bool operator == (const Vec4i& a, const Vec4i& b)
{
	return a.x[0] == b.x[0] && a.x[1] == b.x[1] && a.x[2] == b.x[2] && a.x[3] == b.x[3];
}


template <int index>
INDIGO_STRONG_INLINE const Vec4i copyToAll(const Vec4i& a) { return _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a.v), _mm_castsi128_ps(a.v), _MM_SHUFFLE(index, index, index, index))); } // SSE 1

template<int index>
INDIGO_STRONG_INLINE int elem(const Vec4i& v) { return _mm_extract_epi32(v.v, index); } // SSE 4: pextrd


//INDIGO_STRONG_INLINE const Vec4i operator * (const Vec4i& a, const Vec4i& b) { return _mm_mullo_epi32(a.v, b.v); } // SSE 4
INDIGO_STRONG_INLINE const Vec4i operator + (const Vec4i& a, const Vec4i& b) { return _mm_add_epi32(a.v, b.v); } // SSE 2

INDIGO_STRONG_INLINE const Vec4i operator ^ (const Vec4i& a, const Vec4i& b) { return _mm_xor_si128(a.v, b.v); } // SSE 2
INDIGO_STRONG_INLINE const Vec4i operator & (const Vec4i& a, const Vec4i& b) { return _mm_and_si128(a.v, b.v); } // SSE 2

INDIGO_STRONG_INLINE const Vec4i operator << (const Vec4i& a, const int32 bits) { return _mm_slli_epi32(a.v, bits); } // SSE 2
INDIGO_STRONG_INLINE const Vec4i operator >> (const Vec4i& a, const int32 bits) { return _mm_srai_epi32(a.v, bits); } // SSE 2


#if COMPILE_SSE4_CODE
// Note: _mm_min_epi32 and _mm_max_epi32 are SSE 4!
INDIGO_STRONG_INLINE Vec4i min(const Vec4i& a, const Vec4i& b) { return Vec4i(_mm_min_epi32(a.v, b.v)); } // SSE 4
INDIGO_STRONG_INLINE Vec4i max(const Vec4i& a, const Vec4i& b) { return Vec4i(_mm_max_epi32(a.v, b.v)); } // SSE 4

INDIGO_STRONG_INLINE Vec4i clamp(const Vec4i& v, const Vec4i& a, const Vec4i& b) { return max(a, min(v, b)); }
#endif // COMPILE_SSE4_CODE

