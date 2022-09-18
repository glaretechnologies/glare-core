/*=====================================================================
Vec4.h
------
Copyright Glare Technologies Limited 2020 - 
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
=====================================================================*/
#pragma once


#include "SSE.h"
#include "Vec4i.h"
#include "mathstypes.h"
#include "../utils/MemAlloc.h"
#include <assert.h>
#include <string>


/*=====================================================================
Vec4f
-----
Four component vector class that uses SSE 4-vectors.
=====================================================================*/
SSE_CLASS_ALIGN Vec4f
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	typedef float RealType;


	GLARE_STRONG_INLINE Vec4f() {}
	GLARE_STRONG_INLINE explicit Vec4f(float x_, float y_, float z_, float w_) : v(_mm_set_ps(w_, z_, y_, x_)) {}
	GLARE_STRONG_INLINE Vec4f(__m128 v_) : v(v_) {}
	GLARE_STRONG_INLINE explicit Vec4f(float f) : v(_mm_set1_ps(f)) {}

	GLARE_STRONG_INLINE void set(float x_, float y_, float z_, float w_) { v = _mm_set_ps(w_, z_, y_, x_); }

	GLARE_STRONG_INLINE Vec4f& operator = (const Vec4f& a);

	GLARE_STRONG_INLINE float& operator [] (unsigned int index);
	GLARE_STRONG_INLINE const float& operator [] (unsigned int index) const;

	GLARE_STRONG_INLINE void operator += (const Vec4f& a);
	GLARE_STRONG_INLINE void operator -= (const Vec4f& a);

	GLARE_STRONG_INLINE void operator *= (float f);

	inline bool operator == (const Vec4f& a) const;
	inline bool operator != (const Vec4f& a) const;
	
	GLARE_STRONG_INLINE float length() const;
	GLARE_STRONG_INLINE float length2() const;
	GLARE_STRONG_INLINE float getDist(const Vec4f& a) const;
	GLARE_STRONG_INLINE float getDist2(const Vec4f& a) const;

	GLARE_STRONG_INLINE float fastApproxRecipLength() const; // Uses rsqrt SSE instruction

	inline bool isUnitLength() const;

	inline bool isFinite() const;

	const std::string toString() const;
	const std::string toStringNSigFigs(int n) const;

	static void test();

	union
	{
		float x[4];
		__m128 v;
	};
};


// Some stand-alone functions that operate on Vec4fs


GLARE_STRONG_INLINE const Vec4f operator + (const Vec4f& a, const Vec4f& b)
{
	return _mm_add_ps(a.v, b.v);
}


GLARE_STRONG_INLINE const Vec4f operator - (const Vec4f& a, const Vec4f& b)
{
	return _mm_sub_ps(a.v, b.v);
}


GLARE_STRONG_INLINE const Vec4f operator * (const Vec4f& a, float f)
{
	return _mm_mul_ps(a.v, _mm_set1_ps(f));
}


GLARE_STRONG_INLINE const Vec4f operator * (const Vec4f& a, const Vec4f& b)
{
	return _mm_mul_ps(a.v, b.v);
}


GLARE_STRONG_INLINE const Vec4f operator / (const Vec4f& a, float f)
{
	return _mm_div_ps(a.v, _mm_set1_ps(f));
}


GLARE_STRONG_INLINE float dot(const Vec4f& a, const Vec4f& b)
{
#if COMPILE_SSE4_CODE
	return _mm_cvtss_f32(_mm_dp_ps(a.v, b.v, 255));
#else
	// Do the dot product horizontal add with scalar ops.
	// It's much faster this way.  This is the way the Embree dudes do it when SSE4 is not available.
	// See perf tests in Vec4f.cpp.
	Vec4f prod(_mm_mul_ps(a.v, b.v));

	return prod.x[0] + prod.x[1] + prod.x[2] + prod.x[3];
#endif
}


inline bool epsEqual(const Vec4f& a, const Vec4f& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x[0], b.x[0], eps) &&
		::epsEqual(a.x[1], b.x[1], eps) &&
		::epsEqual(a.x[2], b.x[2], eps) &&
		::epsEqual(a.x[3], b.x[3], eps);
}


inline bool approxEq(const Vec4f& a, const Vec4f& b, float eps = NICKMATHS_EPSILON)
{
	return Maths::approxEq(a.x[0], b.x[0], eps) &&
		Maths::approxEq(a.x[1], b.x[1], eps) &&
		Maths::approxEq(a.x[2], b.x[2], eps) &&
		Maths::approxEq(a.x[3], b.x[3], eps);
}


GLARE_STRONG_INLINE const Vec4f normalise(const Vec4f& a)
{
	return a / a.length();
}


GLARE_STRONG_INLINE const Vec4f normalise(const Vec4f& a, float& length_out)
{
	length_out = a.length();

	return a / length_out;
}


GLARE_STRONG_INLINE const Vec4f removeComponentInDir(const Vec4f& v, const Vec4f& unit_dir)
{
	assert(unit_dir.isUnitLength());
	return v - unit_dir * dot(v, unit_dir);
}


// Unary -
GLARE_STRONG_INLINE const Vec4f operator - (const Vec4f& v)
{
	// Flip sign bits
	const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	return _mm_xor_ps(v.v, mask);
}


inline bool isZero(const Vec4f& v)
{
	__m128 zero = _mm_setzero_ps(); // Latency 1
	__m128 mask = _mm_cmpneq_ps(zero, v.v); // latency 3
	int res = _mm_movemask_ps(mask);
	return res == 0;
}


Vec4f& Vec4f::operator = (const Vec4f& a)
{
	v = a.v;
	return *this;
}


float& Vec4f::operator [] (unsigned int index)
{
	assert(index < 4);
	return x[index];
}


const float& Vec4f::operator [] (unsigned int index) const
{
	assert(index < 4);
	return x[index];
}


void Vec4f::operator += (const Vec4f& a)
{
	v = _mm_add_ps(v, a.v);
}


void Vec4f::operator -= (const Vec4f& a)
{
	v = _mm_sub_ps(v, a.v);
}


void Vec4f::operator *= (float f)
{
	v = _mm_mul_ps(v, _mm_set1_ps(f));
}


bool Vec4f::operator == (const Vec4f& a) const
{
	return _mm_movemask_ps(_mm_cmpeq_ps(v, a.v)) == 0xF;
}


bool Vec4f::operator != (const Vec4f& a) const
{
	return _mm_movemask_ps(_mm_cmpneq_ps(v, a.v)) != 0;
}


float Vec4f::length() const
{
	return std::sqrt(dot(*this, *this));
}


float Vec4f::length2() const
{
	return dot(*this, *this);
}


float Vec4f::getDist(const Vec4f& a) const
{
	return (a - *this).length();
}


float Vec4f::getDist2(const Vec4f& a) const
{
	return (a - *this).length2();
}


float Vec4f::fastApproxRecipLength() const // Uses rsqrt SSE instruction
{
	return _mm_cvtss_f32(_mm_rsqrt_ps(_mm_dp_ps(v, v, 255)));
}


bool Vec4f::isUnitLength() const
{
	return ::epsEqual(1.0f, length());
}


GLARE_STRONG_INLINE const Vec4f min(const Vec4f& a, const Vec4f& b)
{
	return _mm_min_ps(a.v, b.v);
}


GLARE_STRONG_INLINE const Vec4f max(const Vec4f& a, const Vec4f& b)
{
	return _mm_max_ps(a.v, b.v);
}


GLARE_STRONG_INLINE const Vec4f clamp(const Vec4f& v, const Vec4f& lowerbound, const Vec4f& upperbound)
{
	return max(min(v, upperbound), lowerbound);
}


#if COMPILE_SSE4_CODE
GLARE_STRONG_INLINE const Vec4f floor(const Vec4f& v)
{
	return Vec4f(_mm_floor_ps(v.v)); // NOTE: _mm_floor_ps (roundps) is SSE4
}
#endif


GLARE_STRONG_INLINE const Vec4i toVec4i(const Vec4f& v)
{
	return Vec4i(_mm_cvttps_epi32(v.v)); // _mm_cvttps_epi32 (CVTTPS2DQ) is SSE 2
}


template <int index>
GLARE_STRONG_INLINE const Vec4f copyToAll(const Vec4f& a) { return _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(index, index, index, index)); } // SSE 1


// Copy the elements of a vector to other elements.
// For example, to reverse the elements: v = swizzle<3, 2, 1, 0>(v);
// Note that the _MM_SHUFFLE macro takes indices in reverse order than usual.
template <int index0, int index1, int index2, int index3>
GLARE_STRONG_INLINE const Vec4f swizzle(const Vec4f& a) { return _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(index3, index2, index1, index0)); } // SSE 1

template <int index0, int index1, int index2, int index3>
GLARE_STRONG_INLINE const Vec4f shuffle(const Vec4f& a, const Vec4f& b) { return _mm_shuffle_ps(a.v, b.v, _MM_SHUFFLE(index3, index2, index1, index0)); } // SSE 1

template<int index>
GLARE_STRONG_INLINE float elem(const Vec4f& v) { return _mm_cvtss_f32(swizzle<index, index, index, index>(v).v); } // SSE 1

template<>
GLARE_STRONG_INLINE float elem<0>(const Vec4f& v) { return _mm_cvtss_f32(v.v); } // Specialise for getting the zeroth element.


// From Embree
// Returns [a0, b0, a1, b1]
GLARE_STRONG_INLINE Vec4f unpacklo(const Vec4f& a, const Vec4f& b) { return _mm_unpacklo_ps(a.v, b.v); } // SSE 1

// Returns [a2, b2, a3, b3]
GLARE_STRONG_INLINE Vec4f unpackhi(const Vec4f& a, const Vec4f& b) { return _mm_unpackhi_ps(a.v, b.v); } // SSE 1

GLARE_STRONG_INLINE void transpose(const Vec4f& r0, const Vec4f& r1, const Vec4f& r2, const Vec4f& r3, Vec4f& c0, Vec4f& c1, Vec4f& c2, Vec4f& c3)
{
	Vec4f l02 = unpacklo(r0,r2); // (r0x, r2x, r0y, r2y)
	Vec4f h02 = unpackhi(r0,r2); // (r0z, r2z, r0w, r2w)
	Vec4f l13 = unpacklo(r1,r3); // (r1x, r3x, r1y, r3y)
	Vec4f h13 = unpackhi(r1,r3); // (r1z, r3z, r1w, r3w)
	c0 = unpacklo(l02,l13); // (l02.x, l13.x, l02.y, l13.y) = (r0x, r1x, r2x, r3x)
	c1 = unpackhi(l02,l13);
	c2 = unpacklo(h02,h13);
	c3 = unpackhi(h02,h13);
}


#if COMPILE_SSE4_CODE
GLARE_STRONG_INLINE Vec4i floorToVec4i(const Vec4f& v) 
{
	//NOTE: round_ps is SSE4: http://msdn.microsoft.com/en-us/library/bb514047(v=vs.90).aspx
	return Vec4i(_mm_cvtps_epi32(_mm_round_ps(v.v, _MM_FROUND_FLOOR)));
}
#endif


GLARE_STRONG_INLINE Vec4i truncateToVec4i(const Vec4f& v)
{
	return Vec4i(_mm_cvttps_epi32(v.v));
}


GLARE_STRONG_INLINE const Vec4f abs(const Vec4f& a)
{
	// Zero the sign bits.
	// _mm_castsi128_ps, _mm_set1_epi32 are SSE2.
	const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
	return _mm_and_ps(a.v, mask);
}


inline const std::string toString(const Vec4f& v) { return v.toString(); }


GLARE_STRONG_INLINE const Vec4f loadVec4f(const float* const data)
{
	assert(((uint64)data % 16) == 0); // Must be 16-byte aligned.
	return Vec4f(_mm_load_ps(data));
}


GLARE_STRONG_INLINE const Vec4f loadUnalignedVec4f(const float* const data)
{
	return Vec4f(_mm_loadu_ps(data));
}


GLARE_STRONG_INLINE void storeVec4f(const Vec4f& v, float* const mem)
{
	assert(((uint64)mem % 16) == 0); // Must be 16-byte aligned.
	_mm_store_ps(mem, v.v);
}


GLARE_STRONG_INLINE void storeVec4fUnaligned(const Vec4f& v, float* const mem)
{
	_mm_storeu_ps(mem, v.v);
}


GLARE_STRONG_INLINE const Vec4f mul(const Vec4f& a, const Vec4f& b)
{
	return Vec4f(_mm_mul_ps(a.v, b.v));
}


GLARE_STRONG_INLINE const Vec4f div(const Vec4f& a, const Vec4f& b)
{
	return Vec4f(_mm_div_ps(a.v, b.v));
}


GLARE_STRONG_INLINE const Vec4f sqrt(const Vec4f& a)
{
	return Vec4f(_mm_sqrt_ps(a.v));
}


GLARE_STRONG_INLINE Vec4f parallelEq(const Vec4f& a, const Vec4f& b)
{
	return _mm_cmpeq_ps(a.v, b.v);
}


GLARE_STRONG_INLINE Vec4f parallelOr(const Vec4f& a, const Vec4f& b)
{
	return _mm_or_ps(a.v, b.v);
}


GLARE_STRONG_INLINE Vec4f parallelAnd(const Vec4f& a, const Vec4f& b)
{
	return _mm_and_ps(a.v, b.v);
}


GLARE_STRONG_INLINE Vec4f parallelLessEqual(const Vec4f& a, const Vec4f& b)
{
	return _mm_cmple_ps(a.v, b.v);
}


GLARE_STRONG_INLINE Vec4f parallelGreaterEqual(const Vec4f& a, const Vec4f& b)
{
	return _mm_cmpge_ps(a.v, b.v);
}


GLARE_STRONG_INLINE bool allTrue(const Vec4f& vec)
{
	return _mm_movemask_ps(vec.v) == 0xF; // All elements are true (e.g. 0xFFFFFFFF) iff the four lower bits of the result mask are set.
}


GLARE_STRONG_INLINE float horizontalSum(const Vec4f& a)
{
	// suppose a = (a3, a2, a1, a0)
	// 
	// hadd(a, a) = 
	// (a3 + a2, a1 + a0, a3 + a2, a1 + a0)
	// 
	// hadd(hadd(a, a), hadd(a, a)) = 
	// ((a3 + a2) + (a1 + a0), (a3 + a2) + (a1 + a0), (a3 + a2) + (a1 + a0), (a3 + a2) + (a1 + a0))
#if COMPILE_SSE4_CODE
	const Vec4f temp(_mm_hadd_ps(a.v, a.v));
	const Vec4f res(_mm_hadd_ps(temp.v, temp.v));
	return res.x[0];
#else
	return a.x[0] + a.x[1] + a.x[2] + a.x[3];
#endif
}


// If mask element has higher bit set, return a element, else return b element, so something like:
// mask ? a : b
// Note that _mm_blendv_ps returns its second arg if mask is true, so we'll swap the order of the args.
#if COMPILE_SSE4_CODE 
GLARE_STRONG_INLINE Vec4i select(const Vec4i& a, const Vec4i& b, const Vec4f& mask)
{
	return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(b.v), _mm_castsi128_ps(a.v), mask.v)); 
}

// Same as above but with Vec4i mask.
GLARE_STRONG_INLINE Vec4i select(const Vec4i& a, const Vec4i& b, const Vec4i& mask)
{
	return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(b.v), _mm_castsi128_ps(a.v), _mm_castsi128_ps(mask.v)));
}


// If mask element has higher bit set, return a element, else return b element.
GLARE_STRONG_INLINE Vec4f select(const Vec4f& a, const Vec4f& b, const Vec4f& mask)
{
	return Vec4f(_mm_blendv_ps(b.v, a.v, mask.v));
}
#endif


GLARE_STRONG_INLINE Vec4f bitcastToVec4f(const Vec4i& v)
{
	return Vec4f(_mm_castsi128_ps(v.v));
}


GLARE_STRONG_INLINE Vec4i bitcastToVec4i(const Vec4f& v)
{
	return Vec4i(_mm_castps_si128(v.v));
}


// Cast each element to float
GLARE_STRONG_INLINE Vec4f toVec4f(const Vec4i& v)
{
	return Vec4f(_mm_cvtepi32_ps(v.v));
}


// Treat each element as an uint32, and cast each element to float.
#if COMPILE_SSE4_CODE 
GLARE_STRONG_INLINE Vec4f UIntToVec4f(const Vec4i& v)
{
	// If sign bit is set, convert to float, then add 2^32.
	// this brings -2^31 to 2^31, and -1 to 2^32 - 1.
	// otherwise just use converted to float value directly.
	const Vec4f fv = toVec4f(v);
	const Vec4f offset_fv = fv + Vec4f(4294967296.f);

	return select(
		offset_fv, // a
		fv, // b
		bitcastToVec4f(v) // mask (uses high bit, which is sign bit)
	);
}
#endif


GLARE_STRONG_INLINE const Vec4f maskWToZero(const Vec4f& a)
{
	return _mm_and_ps(a.v, bitcastToVec4f(Vec4i(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0)).v);
}


GLARE_STRONG_INLINE Vec4f setWToOne(const Vec4f& a)
{
	const Vec4f one = _mm_set_ss(1.f); // [1, 0, 0, 0]
	const Vec4f v1 = shuffle<2, 2, 0, 0>(a, one); // [z, z, 1, 1]
	return shuffle<0, 1, 0, 2>(a, v1); // [x, y, z, 1]

	//return select(a, Vec4f(1.f), bitcastToVec4f(Vec4i(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0)).v); // // If mask element has higher bit set, return a element, else return b element.
}


GLARE_STRONG_INLINE Vec4f setW(const Vec4f& a, float new_w)
{
	const Vec4f wvec = _mm_set_ss(new_w); // [new_w, 0, 0, 0]
	const Vec4f v1 = shuffle<2, 2, 0, 0>(a, wvec); // [z, z, new_w, new_w]
	return shuffle<0, 1, 0, 2>(a, v1); // [x, y, z, new_w]
}


GLARE_STRONG_INLINE const Vec4f crossProduct(const Vec4f& a, const Vec4f& b)
{
	// w component of result = a.w*b.w - a.w*b.w = 0
	return mul(swizzle<1, 2, 0, 3>(a), swizzle<2, 0, 1, 3>(b)) - mul(swizzle<2, 0, 1, 3>(a), swizzle<1, 2, 0, 3>(b));
}


// This is bascically the vectorised form of isFinite() from mathstypes.h
bool Vec4f::isFinite() const
{
	Vec4f anded = _mm_and_ps(v, bitcastToVec4f(Vec4i(0x7f800000)).v); // c & 0x7f800000

	Vec4i res = _mm_cmplt_epi32(bitcastToVec4i(anded).v, Vec4i(0x7f800000).v); // (c & 0x7f800000) < 0x7f800000

	return _mm_movemask_ps(bitcastToVec4f(res).v) == 0xF; // Return true if the less-than is true for all components.
}


void doAssertIsUnitLength(const Vec4f& v);

#ifdef NDEBUG
#define assertIsUnitLength(v) ((void)0)
#else
#define assertIsUnitLength(v) doAssertIsUnitLength(v)
#endif
