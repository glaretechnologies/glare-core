/*=====================================================================
SSE.h
-----
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Copyright Glare Technologies Limited 2012 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
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

#include <assert.h>
#ifndef _MSC_VER
#include <stdlib.h>
#include <inttypes.h>
#endif


typedef __m128 SSE4Vec; // A vector of 4 single precision floats.  16 byte aligned by default.
#ifdef USE_SSE2
typedef __m128i SSE4Int; // A vector of 4 32 bit integers.  16 byte aligned by default.
#endif


#ifdef COMPILER_MSVC
#define SSE_CLASS_ALIGN _MM_ALIGN16 class
#define SSE_ALIGN _MM_ALIGN16
#else
#define SSE_CLASS_ALIGN class __attribute__ ((aligned (16)))
#define SSE_ALIGN __attribute__ ((aligned (16)))
#endif

#ifdef COMPILER_MSVC
#define AVX_CLASS_ALIGN _CRT_ALIGN(32) class
#define AVX_ALIGN _CRT_ALIGN(32)
#else
#define AVX_CLASS_ALIGN class __attribute__ ((aligned (32)))
#define AVX_ALIGN __attribute__ ((aligned (32)))
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


	void* alignedMalloc(size_t size, size_t alignment); // throws bad_alloc on allocation failure.
	void alignedFree(void* mem);


	inline void* alignedSSEMalloc(size_t size)
	{
		return alignedMalloc(size, 16);
	}

	template <class T>
	inline void alignedSSEFree(T* t)
	{
		alignedFree((void*)t);
	}

	template <class T>
	inline void alignedSSEArrayMalloc(size_t numelems, T*& t_out)
	{
		const size_t memsize = sizeof(T) * numelems;
		t_out = static_cast<T*>(alignedSSEMalloc(memsize));
	}

	template <class T>
	inline void alignedArrayMalloc(size_t numelems, size_t alignment, T*& t_out)
	{
		const size_t memsize = sizeof(T) * numelems;
		t_out = static_cast<T*>(alignedMalloc(memsize, alignment));
	}

	template <class T>
	inline void alignedSSEArrayFree(T* t)
	{
		alignedSSEFree(t);
	}

	template <class T>
	inline void alignedArrayFree(T* t)
	{
		alignedFree(t);
	}
};


#define INDIGO_ALIGNED_NEW_DELETE \
	void* operator new(size_t size) { return SSE::alignedMalloc(size, 32); } \
	void operator delete(void* ptr) { SSE::alignedFree(ptr); }


// GLARE_ALIGNMENT - get alignment of type
#ifdef _MSC_VER
// alignof doesn't work in VS2012.
#define GLARE_ALIGNMENT(x) __alignof(x) 
#else
#define GLARE_ALIGNMENT(x) alignof(x)
#endif

#define assertSSEAligned(p) (assert(SSE::isSSEAligned((p))))


const SSE_ALIGN float zero_4vec[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
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

#define multByScalar(a, b) ( mult4Vec( (a), loadScalarCopy((b)) ) )

#define div4Vec(a, b) (_mm_div_ps((a), (b)))
//Divides the four single-precision, floating-point values of a and b. (result = a/b)

#define recip4Vec(a) (_mm_rcp_ps(a))
//Computes the approximations of reciprocals of the four single-precision, floating-point values of a.

#define lessThan4Vec(a, b) (_mm_cmplt_ps((a), (b)))
//Compares for less than. (all components).  Sets to 0xffffffff if true, 0x0 otherwise

#define lessThanLowWord(a, b) (_mm_cmple_ss((a), (b)))
//Compares for less than. (low word only)

#define MSBMask(a) (_mm_movemask_ps(a))
//Creates a 4-bit mask from the most significant bits of the four single-precision, floating-point values.

#define allTrue(a) (MSBMask(a) == 0x0000000F)
//is true IFF the four bits are set

#define noneTrue(a) (MSBMask(a) == 0x00000000)

/*
#define and4Vec(a) (MSBMask(a) == 0x0000000F)
//is true IFF the four bits are set

#define or4Vec(a) (MSBMask(a) != 0x00000000)
//is true IFF the four bits are set*/

#define and4Vec(a, b) (_mm_and_ps((a), (b)))
//Computes the bitwise AND of the four single-precision, floating-point values of a and b.

inline const SSE4Vec andNot4Vec(const SSE4Vec& a, const SSE4Vec& b)
{
	return _mm_andnot_ps(a, b);
}

#define or4Vec(a, b) (_mm_or_ps((a), (b)))
//Computes the bitwise OR of the four single-precision, floating-point values of a and b.

//#define allLessThan(a, b) (and4Vec(lessThan4Vec(a, b)))
#define allLessThan(a, b) (allTrue(lessThan4Vec(a, b)))
//are all the values in a less than the corresponding values in b

#define min4Vec(a, b) (_mm_min_ps((a), (b)))
//Computes the minima of the four single-precision, floating-point values of a and b.

#define max4Vec(a, b) (_mm_max_ps((a), (b)))
//Computes the maximums of the four single-precision, floating-point values of a and b.


#define SHUF_X 0
#define SHUF_Y 1
#define SHUF_Z 2
#define SHUF_W 3

//#define shuffle4Vec(a, b, x, y, z, w) (_mm_shuffle_ps(a, b, _MM_SHUFFLE(w, z, y, x)))
#define shuffle4Vec(a, b, x, y, z, w) (_mm_shuffle_ps(a, b, _MM_SHUFFLE(z, y, x, w)))
//Selects four specific single-precision, floating-point values from a and b, based on the mask i. The mask must be an immediate

#define indigoCopyToAll(a, index) (_mm_shuffle_ps(a, a, _MM_SHUFFLE(index, index, index, index)))


#define shiftLeftOneWord(a) (_mm_slli_si128((a)), 32)

//#define setToZero(a) (_mm_setzero_ps()

inline const SSE4Vec zeroVec()
{
	return _mm_setzero_ps();
}


inline const SSE4Vec oneVec()
{
	return _mm_load_ps(one_4vec);
}


//sets each float in result to a if the mask is 1'd, or b if the mask is zeroed
/*inline void setMasked(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& mask, SSE4Vec& result)
{
	result = or4Vec( and4Vec(a, mask), andNot(b, mask) );
}*/

inline const SSE4Vec setMasked(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& mask)
{
	//const SSE4Vec masked_a = and4Vec(a, mask);
	//const SSE4Vec masked_b = andNot(b, mask);

	//return or4Vec(masked_a, masked_b);
	return or4Vec( and4Vec(mask, a), andNot4Vec(mask, b) );
}

/*inline void setIfALessThanB4Vec(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& newvals, SSE4Vec& result)
{
	setMasked(newvals, result, lessThan4Vec(a, b), result);
}*/

inline const SSE4Vec setIfALessThanB4Vec(const SSE4Vec& a, const SSE4Vec& b, const SSE4Vec& newvals,
										 const SSE4Vec& oldvals)
{
	return setMasked(newvals, oldvals, lessThan4Vec(a, b));
}

//------------------------------------------------------------------------
//Integer stuff
//------------------------------------------------------------------------

#ifdef USE_SSE2
//------------------------------------------------------------------------
//initialisation
//------------------------------------------------------------------------
inline const SSE4Int set4Int(int x0, int x1, int x2, int x3)
{
	return _mm_set_epi32(x3, x2, x1, x0);
}

//Sets the 4 signed 32-bit integer values to i.
inline const SSE4Int setScalarCopy(int x)
{
	return _mm_set1_epi32(x);
}

//------------------------------------------------------------------------
//storing
//------------------------------------------------------------------------

inline void store4Int(const SSE4Int& v, int* dest)
{
	_mm_store_si128((SSE4Int*)dest, v);
}

//------------------------------------------------------------------------
//conversion
//------------------------------------------------------------------------
inline const SSE4Int toSSE4Int(const SSE4Vec& v)
{
	SSE_ALIGN int temp[4];
	store4Vec(v, (float*)temp);
	return set4Int(temp[0], temp[1], temp[2], temp[3]);
}

/*inline const SSE4Int load4Int(int* x)
{
	return _mm_load_si128(x);
}*/

//------------------------------------------------------------------------
//logical operators
//------------------------------------------------------------------------
#define and4Int(a, b) (_mm_and_si128((a), (b)))
//Computes the bitwise AND of the 128-bit value in a and the 128-bit value in b.

//Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the 128-bit value in a.
inline const SSE4Int andNot(const SSE4Int& a, const SSE4Int& b)
{
	return _mm_andnot_si128(a, b);
}

#define or4Int(a, b) (_mm_or_si128((a), (b)))
//Computes the bitwise OR of the 128-bit value in a and the 128-bit value in b.


inline const SSE4Int setMasked(const SSE4Int& a, const SSE4Int& b, const SSE4Int& mask)
{
	return or4Int( and4Int(mask, a), andNot(mask, b) );
}

#endif


//the returned SSE4Vec will have the value of the dot product in each of the components
inline const SSE4Vec dotSSEIn4Vec(const SSE4Vec& v1, const SSE4Vec& v2)
{
	SSE4Vec prod = mult4Vec(v1, v2);
	SSE4Vec result = shuffle4Vec(prod, prod, SHUF_X, SHUF_X, SHUF_X, SHUF_X);

	//SSE4Vec shuffled = shuffle4Vec(prod, prod, SHUF_Y, 0, 0, 0);
	//shuffled.x = prod.y
	//shuffled.y = prod.z
	result = add4Vec(result, shuffle4Vec(prod, prod, SHUF_Y, SHUF_Y, SHUF_Y, SHUF_Y));
	result = add4Vec(result, shuffle4Vec(prod, prod, SHUF_Z, SHUF_Z, SHUF_Z, SHUF_Z));

	return result;
}


inline void reciprocalSSE(const float* vec_in, float* reciprocal_out)
{
	assertSSEAligned(vec_in);
	assertSSEAligned(reciprocal_out);

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
	assertSSEAligned(a);
	assertSSEAligned(b);
	assertSSEAligned(vec_out);

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

	SSE_ALIGN float x[4];
	_mm_store_ps(x, v5);
	return x[0];

	// Although the code below looks faster, it seems to run slower (~11.5 vs 9.5 cycles on an I7 920)

	// v = [d, c, b, a]
	/*__m128 v_1 = _mm_movehl_ps(v, v); // [d, c, d, c]

	__m128 v_2 = _mm_max_ps(v, v_1); // [., ., m(b, d), m(a, c)]

	__m128 v_3 = _mm_shuffle_ps(v_2, v_2, _MM_SHUFFLE(1, 1, 1, 1)); // [., ., m(b, d), m(b, d)]

	__m128 v_4 = _mm_max_ps(v_2, v_3); // [., ., ., m(a, b, c, d)]

	SSE_ALIGN float x[4];
	_mm_store_ps(x, v_4);
	return x[0];*/
}


inline float horizontalMin(const __m128& v)
{
	// This code takes about 8.5 cycles on an I7 920.
	// v =																   [d, c, b, a]
	const __m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));    // [c, d, a, b]
	const __m128 v3 = _mm_min_ps(v, v2);                                // [m(c, d), m(c, d), m(a, b), m(a, b)]
	const __m128 v4 = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(0, 1, 2, 3));  // [m(a, b), m(a, b), [m(c, d), [m(c, d)]
	const __m128 v5 = _mm_min_ps(v4, v3);                               // [m(a, b, c, d), m(a, b, c, d), m(a, b, c, d), m(a, b, c, d)]

	SSE_ALIGN float x[4];
	_mm_store_ps(x, v5);
	return x[0];
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
