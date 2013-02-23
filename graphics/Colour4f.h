/*=====================================================================
Colour4f.h
----------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include "../maths/SSE.h"
#include "../maths/mathstypes.h"
#include "../utils/platform.h"
#include <assert.h>
#include <string>


/*=====================================================================
Colour4f
--------

=====================================================================*/
SSE_CLASS_ALIGN Colour4f
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	typedef float RealType;


	INDIGO_STRONG_INLINE Colour4f() {}
	INDIGO_STRONG_INLINE explicit Colour4f(float x_, float y_, float z_, float w_) : v(_mm_set_ps(w_, z_, y_, x_)) {}
	INDIGO_STRONG_INLINE explicit Colour4f(__m128 v_) : v(v_) {}
	INDIGO_STRONG_INLINE explicit Colour4f(float f) : v(_mm_set1_ps(f)) {}

	INDIGO_STRONG_INLINE void set(float x_, float y_, float z_, float w_) { v = _mm_set_ps(w_, z_, y_, x_); }

	INDIGO_STRONG_INLINE Colour4f& operator = (const Colour4f& a);

	INDIGO_STRONG_INLINE float& operator [] (unsigned int index);
	INDIGO_STRONG_INLINE const float& operator [] (unsigned int index) const;

	INDIGO_STRONG_INLINE void operator += (const Colour4f& a);
	INDIGO_STRONG_INLINE void operator *= (const Colour4f& a);
	INDIGO_STRONG_INLINE void operator -= (const Colour4f& a);

	INDIGO_STRONG_INLINE void operator *= (float f);

	inline bool operator == (const Colour4f& a) const;

	INDIGO_STRONG_INLINE void addMult(const Colour4f& a, float x);
	INDIGO_STRONG_INLINE void clampInPlace(float lowerbound, float upperbound);
	INDIGO_STRONG_INLINE void lowerClampInPlace(float lowerbound);
	
	INDIGO_STRONG_INLINE float length() const;
	INDIGO_STRONG_INLINE float length2() const;
	INDIGO_STRONG_INLINE float getDist(const Colour4f& a) const;
	INDIGO_STRONG_INLINE float getDist2(const Colour4f& a) const;

	inline bool isUnitLength() const;

	const std::string toString() const;

	INDIGO_STRONG_INLINE bool isFinite() const;


	static void test();


	union
	{
		float x[4];
		__m128 v;
	};
};


// Some stand-alone functions that operate on Colour4fs


inline const std::string toString(const Colour4f& c)
{
	return c.toString();
}


INDIGO_STRONG_INLINE Colour4f operator + (const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_add_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE Colour4f operator - (const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_sub_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE Colour4f operator * (const Colour4f& a, float f)
{
	return Colour4f(_mm_mul_ps(a.v, _mm_load_ps1(&f)));
}


INDIGO_STRONG_INLINE Colour4f operator * (const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_mul_ps(a.v, b.v));
}



INDIGO_STRONG_INLINE float dot(const Colour4f& a, const Colour4f& b)
{
#if USE_SSE4 
	Colour4f res;
	res.v = _mm_dp_ps(a.v, b.v, 255);
	return res.x[0];
#else
	// Do the dot product horizontal add with scalar ops.
	// It's much faster this way.  This is the way the Embree dudes do it when SSE4 is not available.
	// See perf tests in Colour4f.cpp.
	Colour4f prod(_mm_mul_ps(a.v, b.v));

	return prod.x[0] + prod.x[1] + prod.x[2] + prod.x[3];
#endif
}


inline bool epsEqual(const Colour4f& a, const Colour4f& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x[0], b.x[0], eps) &&
		::epsEqual(a.x[1], b.x[1], eps) &&
		::epsEqual(a.x[2], b.x[2], eps) &&
		::epsEqual(a.x[3], b.x[3], eps);
}


INDIGO_STRONG_INLINE Colour4f min(const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_min_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE Colour4f max(const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_max_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE Colour4f clamp(const Colour4f& c, const Colour4f& lower, const Colour4f& upper)
{
	return min(max(c, lower), upper);
}


INDIGO_STRONG_INLINE Colour4f normalise(const Colour4f& a)
{
	return a * (1.0f / a.length());
}


INDIGO_STRONG_INLINE Colour4f normalise(const Colour4f& a, float& length_out)
{
	length_out = a.length();

	return a * (1.0f / length_out);
}


INDIGO_STRONG_INLINE Colour4f maskWToZero(const Colour4f& a)
{
	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	return Colour4f(_mm_and_ps(a.v, _mm_load_ps((const float*)mask)));
}


INDIGO_STRONG_INLINE const Colour4f removeComponentInDir(const Colour4f& v, const Colour4f& dir)
{
	return v - dir * dot(v, dir);
}


// Unary -
INDIGO_STRONG_INLINE const Colour4f operator - (const Colour4f& v)
{
	// Flip sign bits
	const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
    return Colour4f(_mm_xor_ps(v.v, mask));
}


Colour4f& Colour4f::operator = (const Colour4f& a)
{
	v = a.v;
	return *this;
}


float& Colour4f::operator [] (unsigned int index)
{
	assert(index < 4);
	return x[index];
}


const float& Colour4f::operator [] (unsigned int index) const
{
	assert(index < 4);
	return x[index];
}


void Colour4f::operator += (const Colour4f& a)
{
	v = _mm_add_ps(v, a.v);
}


void Colour4f::operator *= (const Colour4f& a)
{
	v = _mm_mul_ps(v, a.v);
}


void Colour4f::operator -= (const Colour4f& a)
{
	v = _mm_sub_ps(v, a.v);
}


void Colour4f::operator *= (float f)
{
	v = _mm_mul_ps(v, _mm_load_ps1(&f));
}


bool Colour4f::operator == (const Colour4f& a) const
{
	// NOTE: could speed this up with an SSE instruction, but does it need to be fast?
	// Exact floating point comparison should be rare.
	return
		x[0] == a.x[0] &&
		x[1] == a.x[1] &&
		x[2] == a.x[2] &&
		x[3] == a.x[3];
}


void Colour4f::addMult(const Colour4f& a, float x)
{
	v = _mm_add_ps(v, _mm_mul_ps(a.v, _mm_load_ps1(&x)));
}


void Colour4f::clampInPlace(float lowerbound, float upperbound)
{
	*this = max(min(*this, Colour4f(upperbound)), Colour4f(lowerbound));
}


void Colour4f::lowerClampInPlace(float lowerbound)
{
	*this = max(*this, Colour4f(lowerbound));
}


float Colour4f::length() const
{
	return std::sqrt(dot(*this, *this));
}


float Colour4f::length2() const
{
	return dot(*this, *this);
}


float Colour4f::getDist(const Colour4f& a) const
{
	return Colour4f(a - *this).length();
}


float Colour4f::getDist2(const Colour4f& a) const
{
	return Colour4f(a - *this).length2();
}


bool Colour4f::isUnitLength() const
{
	return ::epsEqual(1.0f, length());
}


bool Colour4f::isFinite() const
{
	return ::isFinite(x[0]) && ::isFinite(x[1]) && ::isFinite(x[2]) && ::isFinite(x[3]);
}
