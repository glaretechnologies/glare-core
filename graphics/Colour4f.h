/*=====================================================================
Colour4f.h
----------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include "../maths/Vec4i.h"
#include "../maths/Vec4f.h"
#include "../maths/mathstypes.h"
#include "../utils/Platform.h"
#include <assert.h>
#include <string>


/*=====================================================================
Colour4f
--------

=====================================================================*/
SSE_CLASS_ALIGN Colour4f
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	typedef float RealType;


	INDIGO_STRONG_INLINE Colour4f() {}
	INDIGO_STRONG_INLINE explicit Colour4f(float x_, float y_, float z_, float w_) : v(_mm_set_ps(w_, z_, y_, x_)) {}
	INDIGO_STRONG_INLINE explicit Colour4f(__m128 v_) : v(v_) {}
	INDIGO_STRONG_INLINE explicit Colour4f(float f) : v(_mm_set1_ps(f)) {}

	INDIGO_STRONG_INLINE void set(float x_, float y_, float z_, float w_) { v = _mm_set_ps(w_, z_, y_, x_); }

	INDIGO_STRONG_INLINE Colour4f& operator = (const Colour4f& a);

	INDIGO_STRONG_INLINE float& operator [] (size_t index);
	INDIGO_STRONG_INLINE const float& operator [] (size_t index) const;

	INDIGO_STRONG_INLINE void operator += (const Colour4f& a);
	INDIGO_STRONG_INLINE void operator *= (const Colour4f& a);
	INDIGO_STRONG_INLINE void operator -= (const Colour4f& a);

	INDIGO_STRONG_INLINE void operator *= (float f);

	inline bool operator == (const Colour4f& a) const;
	inline bool operator != (const Colour4f& a) const;

	INDIGO_STRONG_INLINE void addMult(const Colour4f& a, float x);
	INDIGO_STRONG_INLINE void clampInPlace(float lowerbound, float upperbound);
	INDIGO_STRONG_INLINE void lowerClampInPlace(float lowerbound);
	
	INDIGO_STRONG_INLINE float length() const;
	INDIGO_STRONG_INLINE float length2() const;
	INDIGO_STRONG_INLINE float getDist(const Colour4f& a) const;
	INDIGO_STRONG_INLINE float getDist2(const Colour4f& a) const;

	inline bool isUnitLength() const;

	const std::string toString() const;

	static Colour4f fromHTMLHexString(const std::string& s); // From something like "92ac88".  Returns red on failure.

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
	return Colour4f(_mm_mul_ps(a.v, _mm_set1_ps(f)));
}


INDIGO_STRONG_INLINE Colour4f operator * (const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_mul_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE Colour4f operator / (const Colour4f& a, const Colour4f& b)
{
	return Colour4f(_mm_div_ps(a.v, b.v));
}


INDIGO_STRONG_INLINE float dot(const Colour4f& a, const Colour4f& b)
{
#if 1 // USE_SSE4 
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
	return Colour4f(_mm_and_ps(a.v, bitcastToVec4f(Vec4i(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0)).v));
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


float& Colour4f::operator [] (size_t index)
{
	assert(index < 4);
	return x[index];
}


const float& Colour4f::operator [] (size_t index) const
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
	v = _mm_mul_ps(v, _mm_set1_ps(f));
}


bool Colour4f::operator == (const Colour4f& a) const
{
	return _mm_movemask_ps(_mm_cmpeq_ps(v, a.v)) == 0xF;
}


bool Colour4f::operator != (const Colour4f& a) const
{
	return _mm_movemask_ps(_mm_cmpneq_ps(v, a.v)) != 0;
}


void Colour4f::addMult(const Colour4f& a, float factor)
{
	v = _mm_add_ps(v, _mm_mul_ps(a.v, _mm_set1_ps(factor)));
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


// This is bascically the vectorised form of isFinite() from mathstypes.h
bool Colour4f::isFinite() const
{
	Vec4f anded = _mm_and_ps(v, bitcastToVec4f(Vec4i(0x7f800000)).v); // c & 0x7f800000

	Vec4i res = _mm_cmplt_epi32(bitcastToVec4i(anded).v, Vec4i(0x7f800000).v); // (c & 0x7f800000) < 0x7f800000

	return _mm_movemask_ps(bitcastToVec4f(res).v) == 0xF; // Return true if the less-than is true for all components.
}


// If mask element has higher bit set, return a element, else return b element.
INDIGO_STRONG_INLINE Colour4f select(const Colour4f& a, const Colour4f& b, const Colour4f& mask)
{
	return Colour4f(_mm_blendv_ps(b.v, a.v, mask.v));
}


INDIGO_STRONG_INLINE const Vec4i toVec4i(const Colour4f& v)
{
	return Vec4i(_mm_cvttps_epi32(v.v)); // _mm_cvttps_epi32 (CVTTPS2DQ) is SSE 2
}


// SSE2
INDIGO_STRONG_INLINE Colour4f toColour4f(const Vec4i& v)
{
	return Colour4f(_mm_cvtepi32_ps(v.v));
}


INDIGO_STRONG_INLINE Colour4f bitcastToColour4f(const Vec4i& v)
{
	return Colour4f(_mm_castsi128_ps(v.v));
}


INDIGO_STRONG_INLINE const Colour4f sqrt(const Colour4f& a)
{
	return Colour4f(_mm_sqrt_ps(a.v));
}


INDIGO_STRONG_INLINE const Colour4f abs(const Colour4f& a)
{
	// Zero the sign bits.
	// _mm_castsi128_ps, _mm_set1_epi32 are SSE2.
	const __m128 mask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
	return Colour4f(_mm_and_ps(a.v, mask));
}


INDIGO_STRONG_INLINE Colour4f loadColour4f(const float* const data)
{
	assert(((uint64)data % 16) == 0); // Must be 16-byte aligned.
	return Colour4f(_mm_load_ps(data));
}


INDIGO_STRONG_INLINE Colour4f loadUnalignedColour4f(const float* const data)
{
	return Colour4f(_mm_loadu_ps(data));
}


INDIGO_STRONG_INLINE void storeColour4f(const Colour4f& v, float* const mem)
{
	assert(((uint64)mem % 16) == 0); // Must be 16-byte aligned.
	_mm_store_ps(mem, v.v);
}


INDIGO_STRONG_INLINE void storeColour4fUnaligned(const Colour4f& v, float* const mem)
{
	_mm_storeu_ps(mem, v.v);
}
