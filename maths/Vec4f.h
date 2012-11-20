/*=====================================================================
Vec4.h
------
Copyright Glare Technologies Limited 2012 - 
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
=====================================================================*/
#pragma once


#include "SSE.h"
#include "mathstypes.h"
#include <assert.h>
#include <string>
#include "../utils/platform.h"


/*=====================================================================
Vec4
----

=====================================================================*/
SSE_CLASS_ALIGN Vec4f
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	typedef float RealType;


	INDIGO_STRONG_INLINE Vec4f() {}
	INDIGO_STRONG_INLINE explicit Vec4f(float x_, float y_, float z_, float w_) : v(_mm_set_ps(w_, z_, y_, x_)) {}
	INDIGO_STRONG_INLINE Vec4f(__m128 v_) : v(v_) {}
	INDIGO_STRONG_INLINE explicit Vec4f(float f) : v(_mm_set1_ps(f)) {}

	INDIGO_STRONG_INLINE void set(float x_, float y_, float z_, float w_) { v = _mm_set_ps(w_, z_, y_, x_); }

	INDIGO_STRONG_INLINE Vec4f& operator = (const Vec4f& a);

	INDIGO_STRONG_INLINE float& operator [] (unsigned int index);
	INDIGO_STRONG_INLINE const float& operator [] (unsigned int index) const;

	INDIGO_STRONG_INLINE void operator += (const Vec4f& a);
	INDIGO_STRONG_INLINE void operator -= (const Vec4f& a);

	INDIGO_STRONG_INLINE void operator *= (float f);

	inline bool operator == (const Vec4f& a) const;

	
	INDIGO_STRONG_INLINE float length() const;
	INDIGO_STRONG_INLINE float length2() const;
	INDIGO_STRONG_INLINE float getDist(const Vec4f& a) const;
	INDIGO_STRONG_INLINE float getDist2(const Vec4f& a) const;

	inline bool isUnitLength() const;

	const std::string toString() const;


	static void test();


	union
	{
		float x[4];
		__m128 v;
	};
};


// Some stand-alone functions that operate on Vec4fs


// NOTE: This returns __m128 instead of 'const Vec4f', because VC9 won't inline the function if it returns 'const Vec4f'.
INDIGO_STRONG_INLINE __m128 operator + (const Vec4f& a, const Vec4f& b)
{
	return _mm_add_ps(a.v, b.v);
}


INDIGO_STRONG_INLINE __m128 operator - (const Vec4f& a, const Vec4f& b)
{
	return _mm_sub_ps(a.v, b.v);
}


INDIGO_STRONG_INLINE __m128 operator * (const Vec4f& a, float f)
{
	return _mm_mul_ps(a.v, _mm_load_ps1(&f));
}


INDIGO_STRONG_INLINE float dot(const Vec4f& a, const Vec4f& b)
{
#if USE_SSE4 
	Vec4f res;
	res.v = _mm_dp_ps(a.v, b.v, 255);
	return res.x[0];
#else
	// Do the dot product horizontal add with scalar ops.
	// It's much faster this way.  This is the way the Embree dudes do it when SSE4 is not available.
	// See perf tests in Vec4f.cpp.
	Vec4f prod(_mm_mul_ps(a.v, b.v));

	return prod.x[0] + prod.x[1] + prod.x[2] + prod.x[3];
#endif
}


INDIGO_STRONG_INLINE __m128 crossProduct(const Vec4f& a, const Vec4f& b)
{
	/*
		want (aybz - azby, azbx - axbz, axby - aybx, 0.0f)

		= [0, axby - aybx, azbx - axbz, aybz - azby]
	*/
	return
		_mm_sub_ps(
			_mm_mul_ps(
				_mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 0, 2, 1)), // [0, ax, az, ay]
				_mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 1, 0, 2)) // [0, by, bx, bz]
				), // [0, axby, azbx, aybz]
			_mm_mul_ps(
				_mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 1, 0, 2)), // [0, ay, ax, az]
				_mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 0, 2, 1)) // [0, bx, bz, by]
				) // [0, aybx, axbz, azby]
			)
		;
}


inline bool epsEqual(const Vec4f& a, const Vec4f& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x[0], b.x[0], eps) &&
		::epsEqual(a.x[1], b.x[1], eps) &&
		::epsEqual(a.x[2], b.x[2], eps) &&
		::epsEqual(a.x[3], b.x[3], eps);
}


INDIGO_STRONG_INLINE __m128 normalise(const Vec4f& a)
{
	return a * (1.0f / a.length());
}


INDIGO_STRONG_INLINE __m128 normalise(const Vec4f& a, float& length_out)
{
	length_out = a.length();

	return a * (1.0f / length_out);
}


INDIGO_STRONG_INLINE __m128 maskWToZero(const Vec4f& a)
{
	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	return _mm_and_ps(a.v, _mm_load_ps((const float*)mask));
}


INDIGO_STRONG_INLINE const Vec4f removeComponentInDir(const Vec4f& v, const Vec4f& dir)
{
	return v - dir * dot(v, dir);
}


// Unary -
INDIGO_STRONG_INLINE const Vec4f operator - (const Vec4f& v)
{
	// Flip sign bits
	const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    return _mm_xor_ps(v.v, mask);
}


Vec4f& Vec4f::operator = (const Vec4f& a)
{
	v = a.v;
	return *this;
}


// Disable a bogus VS 2010 Code analysis warning: 'warning C6385: Invalid data: accessing 'x', the readable size is '16' bytes, but '20' bytes might be read'
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:6385)
#endif


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


#ifdef _WIN32
#pragma warning(pop)
#endif


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
	v = _mm_mul_ps(v, _mm_load_ps1(&f));
}


bool Vec4f::operator == (const Vec4f& a) const
{
	// NOTE: could speed this up with an SSE instruction, but does it need to be fast?
	// Exact floating point comparison should be rare.
	return
		x[0] == a.x[0] &&
		x[1] == a.x[1] &&
		x[2] == a.x[2] &&
		x[3] == a.x[3];
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
	return Vec4f(a - *this).length();
}


float Vec4f::getDist2(const Vec4f& a) const
{
	return Vec4f(a - *this).length2();
}


bool Vec4f::isUnitLength() const
{
	return ::epsEqual(1.0f, length());
}
