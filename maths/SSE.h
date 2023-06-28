/*=====================================================================
SSE.h
-----
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"

#if defined(_M_X64) || defined(__x86_64__)

#include <xmmintrin.h> // SSE header file
#include <emmintrin.h> // SSE 2 header file
#ifdef __SSE3__
#include <tmmintrin.h> // SSSE 3 (Supplemental Streaming SIMD Extensions 3) header file
#endif
#ifdef __SSE4_1__
#include <smmintrin.h> // SSE 4.1 header file
#endif

#ifdef __SSE4_1__
#define COMPILE_SSE4_CODE 1
#endif

#else // Else if not x64, assume ARM64.

#include "../libs/sse2neon.h" // Use SSE2Neon to convert SSE intrinsics to Neon intrinsics.

#define COMPILE_SSE4_CODE 1

#endif

#include <assert.h>
#ifndef _MSC_VER
#include <stdlib.h>
#include <inttypes.h>
#endif


namespace SSE
{
	template <class T>
	inline bool isAlignedTo(T* ptr, uintptr_t alignment)
	{
		return (uintptr_t(ptr) % alignment) == 0;
	}

	template <class T>
	inline bool isSSEAligned(T* ptr)
	{
		return isAlignedTo(ptr, 16);
	}
}


typedef __m128 SSE4Vec; // A vector of 4 single precision floats.  16 byte aligned by default.


const SSE_ALIGN float one_4vec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };


#define load4Vec(v) (_mm_load_ps(v))
//Loads four single-precision, floating-point values. The address must be 16-byte aligned.

#define store4Vec(v, loc) (_mm_store_ps(loc, v))
//Stores four single-precision, floating-point values. The address must be 16-byte aligned.

#define storeLowWord(v, loc) (_mm_store_ss(loc, v))
//Stores the lower single-precision, floating-point value.

#define loadScalarCopy(s) (_mm_load_ps1(s))
//Loads a single single-precision, floating-point value, copying it into all four words.
//Composite op: MOVSS + shuffling

#define loadScalarLow(s) (_mm_load_ss(s))
//Loads an single-precision, floating-point value into the low word and clears the upper three words.

#define add4Vec(a, b) (_mm_add_ps((a), (b)))
//Adds the four single-precision, floating-point values of a and b.

#define sub4Vec(a, b) (_mm_sub_ps((a), (b)))
//Subtracts the four single-precision, floating-point values of a and b. (r0 := a0 - b0 etc..)

#define mult4Vec(a, b) (_mm_mul_ps((a), (b)))
//Multiplies the four single-precision, floating-point values of a and b.

#define div4Vec(a, b) (_mm_div_ps((a), (b)))
//Divides the four single-precision, floating-point values of a and b. (result = a/b)

#define lessThan4Vec(a, b) (_mm_cmplt_ps((a), (b)))
//Compares for less than. (all components).  Sets to 0xffffffff if true, 0x0 otherwise

#define MSBMask(a) (_mm_movemask_ps(a))
//Creates a 4-bit mask from the most significant bits of the four single-precision, floating-point values.

#define noneTrue(a) (MSBMask(a) == 0x00000000)

#define and4Vec(a, b) (_mm_and_ps((a), (b)))
//Computes the bitwise AND of the four single-precision, floating-point values of a and b.

inline SSE4Vec andNot4Vec(const SSE4Vec& a, const SSE4Vec& b)
{
	return _mm_andnot_ps(a, b);
}

#define or4Vec(a, b) (_mm_or_ps((a), (b)))
//Computes the bitwise OR of the four single-precision, floating-point values of a and b.

#define min4Vec(a, b) (_mm_min_ps((a), (b)))
//Computes the minima of the four single-precision, floating-point values of a and b.

#define max4Vec(a, b) (_mm_max_ps((a), (b)))
//Computes the maximums of the four single-precision, floating-point values of a and b.

#define indigoCopyToAll(a, index) (_mm_shuffle_ps(a, a, _MM_SHUFFLE(index, index, index, index)))

#define shiftLeftOneWord(a) (_mm_slli_si128((a)), 32)


inline SSE4Vec zeroVec()
{
	return _mm_setzero_ps();
}


inline SSE4Vec oneVec()
{
	return _mm_load_ps(one_4vec);
}


inline void reciprocalSSE(const float* vec_in, float* reciprocal_out)
{
	//assertSSEAligned(vec_in);
	//assertSSEAligned(reciprocal_out);

	_mm_store_ps(
		reciprocal_out,
		_mm_div_ps(
			_mm_load_ps(one_4vec),
			_mm_load_ps(vec_in)
			)
		);
}


inline void addScaledVec4SSE(const float* a, const float* b, float scale, float* vec_out)
{
	//assertSSEAligned(a);
	//assertSSEAligned(b);
	//assertSSEAligned(vec_out);

	_mm_store_ps(
		vec_out,
		_mm_add_ps(
			_mm_load_ps(a),
			_mm_mul_ps(
				_mm_load_ps(b),
				_mm_set1_ps(scale)
				)
			)
		);
}


inline float horizontalMax(const __m128& v)
{
	// This code takes about 8.5 cycles on an I7 920.
	// v =																   [d, c, b, a]
	const __m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));    // [c, d, a, b]
	const __m128 v3 = _mm_max_ps(v, v2);                                // [m(c, d), m(c, d), m(a, b), m(a, b)]
	const __m128 v4 = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(0, 1, 2, 3));  // [m(a, b), m(a, b), [m(c, d), [m(c, d)]
	const __m128 v5 = _mm_max_ps(v4, v3);                               // [m(a, b, c, d), m(a, b, c, d), m(a, b, c, d), m(a, b, c, d)]
	return _mm_cvtss_f32(v5);
}


inline float horizontalMin(const __m128& v)
{
	// This code takes about 8.5 cycles on an I7 920.
	// v =																   [d, c, b, a]
	const __m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));    // [c, d, a, b]
	const __m128 v3 = _mm_min_ps(v, v2);                                // [m(c, d), m(c, d), m(a, b), m(a, b)]
	const __m128 v4 = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(0, 1, 2, 3));  // [m(a, b), m(a, b), [m(c, d), [m(c, d)]
	const __m128 v5 = _mm_min_ps(v4, v3);                               // [m(a, b, c, d), m(a, b, c, d), m(a, b, c, d), m(a, b, c, d)]
	return _mm_cvtss_f32(v5);
}


typedef union {
	__m128 v;
	float f[4];
	unsigned int i[4];
} UnionVec4;


///////////////////////////////////////////////////////////////////////
// From http://jrfonseca.blogspot.co.uk/2008/09/fast-sse2-pow-tables-or-polynomials.html ?

#define EXP_POLY_DEGREE 3

#define POLY0(x, c0) _mm_set1_ps(c0)
#define POLY1(x, c0, c1) _mm_add_ps(_mm_mul_ps(POLY0(x, c1), x), _mm_set1_ps(c0))
#define POLY2(x, c0, c1, c2) _mm_add_ps(_mm_mul_ps(POLY1(x, c1, c2), x), _mm_set1_ps(c0))
#define POLY3(x, c0, c1, c2, c3) _mm_add_ps(_mm_mul_ps(POLY2(x, c1, c2, c3), x), _mm_set1_ps(c0))
#define POLY4(x, c0, c1, c2, c3, c4) _mm_add_ps(_mm_mul_ps(POLY3(x, c1, c2, c3, c4), x), _mm_set1_ps(c0))
#define POLY5(x, c0, c1, c2, c3, c4, c5) _mm_add_ps(_mm_mul_ps(POLY4(x, c1, c2, c3, c4, c5), x), _mm_set1_ps(c0))

static inline __m128 exp2f4(__m128 x)
{
   __m128i ipart;
   __m128 fpart, expipart, expfpart;

   x = _mm_min_ps(x, _mm_set1_ps( 129.00000f));
   x = _mm_max_ps(x, _mm_set1_ps(-126.99999f));

   /* ipart = int(x - 0.5) */
   ipart = _mm_cvtps_epi32(_mm_sub_ps(x, _mm_set1_ps(0.5f)));

   /* fpart = x - ipart */
   fpart = _mm_sub_ps(x, _mm_cvtepi32_ps(ipart));

   /* expipart = (float) (1 << ipart) */
   expipart = _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(ipart, _mm_set1_epi32(127)), 23));

   /* minimax polynomial fit of 2**x, in range [-0.5, 0.5[ */
#if EXP_POLY_DEGREE == 5
   expfpart = POLY5(fpart, 9.9999994e-1f, 6.9315308e-1f, 2.4015361e-1f, 5.5826318e-2f, 8.9893397e-3f, 1.8775767e-3f);
#elif EXP_POLY_DEGREE == 4
   expfpart = POLY4(fpart, 1.0000026f, 6.9300383e-1f, 2.4144275e-1f, 5.2011464e-2f, 1.3534167e-2f);
#elif EXP_POLY_DEGREE == 3
   expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
#elif EXP_POLY_DEGREE == 2
   expfpart = POLY2(fpart, 1.0017247f, 6.5763628e-1f, 3.3718944e-1f);
#else
#error
#endif

   return _mm_mul_ps(expipart, expfpart);
}

#define LOG_POLY_DEGREE 5

static inline __m128 log2f4(__m128 x)
{
   __m128i exp = _mm_set1_epi32(0x7F800000);
   __m128i mant = _mm_set1_epi32(0x007FFFFF);

   __m128 one = _mm_set1_ps( 1.0f);

   __m128i i = _mm_castps_si128(x);

   __m128 e = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_srli_epi32(_mm_and_si128(i, exp), 23), _mm_set1_epi32(127)));

   __m128 m = _mm_or_ps(_mm_castsi128_ps(_mm_and_si128(i, mant)), one);

   __m128 p;

   /* Minimax polynomial fit of log2(x)/(x - 1), for x in range [1, 2[ */
#if LOG_POLY_DEGREE == 6
   p = POLY5( m, 3.1157899f, -3.3241990f, 2.5988452f, -1.2315303f,  3.1821337e-1f, -3.4436006e-2f);
#elif LOG_POLY_DEGREE == 5
   p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);
#elif LOG_POLY_DEGREE == 4
   p = POLY3(m, 2.61761038894603480148f, -1.75647175389045657003f, 0.688243882994381274313f, -0.107254423828329604454f);
#elif LOG_POLY_DEGREE == 3
   p = POLY2(m, 2.28330284476918490682f, -1.04913055217340124191f, 0.204446009836232697516f);
#else
#error
#endif

   /* This effectively increases the polynomial degree by one, but ensures that log2(1) == 0*/
   p = _mm_mul_ps(p, _mm_sub_ps(m, one));

   return _mm_add_ps(p, e);
}

static inline __m128 powf4(__m128 x, __m128 y)
{
   return exp2f4(_mm_mul_ps(log2f4(x), y));
}


///////////////////////////////////////////////////////////////////////


void SSETest();
