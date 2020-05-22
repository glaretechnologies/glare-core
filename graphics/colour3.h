/*=====================================================================
colour3.h
---------
Copyright Glare Technologies Limited
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include <assert.h>


template <class Real>
class Colour3
{
public:
	Colour3(){}

	explicit Colour3(Real x)
	:	r(x), g(x), b(x)
	{}

	Colour3(Real r_, Real g_, Real b_)
	:	r(r_), g(g_), b(b_) 
	{}

	explicit Colour3(const Vec3<Real>& v)
	:	r(v.x),
		g(v.y),
		b(v.z)
	{}

	const Colour3<double> toColour3d() const { return Colour3<double>(r, g, b); }

	inline void set(Real r_, Real g_, Real b_)
	{
		r = r_;
		g = g_;
		b = b_;
	}

	inline Colour3 operator * (Real scale) const
	{
		return Colour3(r * scale, g * scale, b * scale);
	}

	inline Colour3 operator / (Real scale) const
	{
		const Real inv = 1 / scale;
		return Colour3(r * inv, g * inv, b * inv);
	}

	inline Colour3& operator *= (Real scale)
	{
		r *= scale;
		g *= scale;
		b *= scale;

		return *this;
	}

	inline Colour3& operator /= (Real scale)
	{
		const Real inv = 1 / scale;
		r *= inv;
		g *= inv;
		b *= inv;

		return *this;
	}

	inline Colour3 operator + (const Colour3& rhs) const
	{
		return Colour3(r + rhs.r, g + rhs.g, b + rhs.b);
	}
	
	inline Colour3 operator - (const Colour3& rhs) const
	{
		return Colour3(r - rhs.r, g - rhs.g, b - rhs.b);
	}

	inline void operator = (const Colour3& rhs)
	{
		r = rhs.r;
		g = rhs.g;
		b = rhs.b;
	}

	inline bool operator == (const Colour3& rhs) const
	{
		return r == rhs.r && g == rhs.g && b == rhs.b;
	}

	inline bool operator != (const Colour3& rhs) const
	{
		return r != rhs.r || g != rhs.g || b != rhs.b;
	}

	inline void operator += (const Colour3& rhs)
	{
		r += rhs.r;
		g += rhs.g;
		b += rhs.b;
	}

	inline Colour3& operator *= (const Colour3& rhs)
	{
		r *= rhs.r;
		g *= rhs.g;
		b *= rhs.b;
	
		return *this;
	}

	inline const Colour3 operator * (const Colour3& rhs) const
	{
		return Colour3(r * rhs.r, g * rhs.g, b * rhs.b);
	}

	inline void setToMult(const Colour3& other, Real factor)
	{
		r = other.r * factor;
		g = other.g * factor;
		b = other.b * factor;
	}

	inline void addMult(const Colour3& other, Real factor)
	{
		r += other.r * factor;
		g += other.g * factor;
		b += other.b * factor;
	}

	inline void addMult(const Colour3& other, const Colour3& factors)
	{
		r += other.r * factors.r;
		g += other.g * factors.g;
		b += other.b * factors.b;
	}

	inline void add(const Colour3& other)
	{
		r += other.r;
		g += other.g;
		b += other.b;
	}

	inline void clipComponents()
	{
		r = myClamp(r, 0, 1);
		g = myClamp(g, 0, 1);
		b = myClamp(b, 0, 1);
	}

	inline void positiveClipComponents(Real threshold = 1.0f)
	{
		r = myMin(r, threshold);
		g = myMin(g, threshold);
		b = myMin(b, threshold);
	}

	inline bool isZero() const
	{
		return r == 0.0f && g == 0.0f && b == 0.0f;
	}

	inline bool nonZero() const
	{
		return r != 0.0f || g != 0.0f || b != 0.0f;
	}

	// Assuming in linear sRGB space
	inline Real luminance() const
	{
		//return 0.3f*r + 0.587f*g + 0.114f*b;//approx
		// Note that the coefficients should add up to one.
		return (Real)0.2126*r + (Real)0.7152*g + (Real)0.0722*b;
	}
	
	inline Real averageVal() const
	{
		return (r + g + b) * 0.33333333333333333333333333f;
	}

	inline Real minVal() const
	{
		return myMin(r, g, b);
	}

	inline Real maxVal() const
	{
		return myMax(r, g, b);
	}

	const Real* toFloatArray() const { return (const Real*)this; }
	const Real* data() const { return (const Real*)this; }
	
	const Real length() const { return std::sqrt(r*r + g*g + b*b); }

	Real& operator [] (size_t index)
	{
		assert(index < 3);
		return *(&r + index);
	}
	
	Real operator [] (size_t index) const
	{
		assert(index < 3);
		return *(&r + index);
	}

	inline bool isFinite() const
	{
		return ::isFinite(r) && ::isFinite(g) && ::isFinite(b);
	}

	inline void clampInPlace(Real lowerbound, Real upperbound)
	{
		r = myClamp(r, lowerbound, upperbound);
		g = myClamp(g, lowerbound, upperbound);
		b = myClamp(b, lowerbound, upperbound);
	}

	inline void lowerClampInPlace(Real lowerbound)
	{
		r = myMax(r, lowerbound);
		g = myMax(g, lowerbound);
		b = myMax(b, lowerbound);
	}

	inline const Colour3<Real> clamp(Real lowerbound, Real upperbound) const
	{
		return Colour3<Real>(
			myClamp(r, lowerbound, upperbound),
			myClamp(g, lowerbound, upperbound),
			myClamp(b, lowerbound, upperbound)
		);
	}

	inline const Vec3<Real> toVec3() const
	{
		return Vec3<Real>(r, g, b);
	}

	const std::string toString() const;

	Real r,g,b;
};

typedef Colour3<float> Colour3f;
typedef Colour3<double> Colour3d;


// For sorting Colour3's
template <class Real>
inline bool operator < (const Colour3<Real>& a, const Colour3<Real>& b)
{
	if(a.r < b.r)
		return true;
	else if(a.r > b.r)
		return false;
	else // else a.r == b.r:
	{
		if(a.b < b.b)
			return true;
		else if(a.b > b.b)
			return false;
		else // else a.b == b.b:
			return a.g < b.g;
	}
}
