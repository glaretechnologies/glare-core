/*=====================================================================
vec2.h
------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include <string>


/*=================================================================
2 component vector class
------------------------

=================================================================*/
template <class Real>
class Vec2
{
public:
	inline Vec2()
	{}

	inline explicit Vec2(Real v)
	:	x(v),
		y(v)
	{}

	inline Vec2(Real x_, Real y_)
	:	x(x_),
		y(y_)
	{}

	inline void set(Real newx, Real newy)
	{
		x = newx;
		y = newy;
	}

	const std::string toString() const;

	inline const Vec2 operator + (const Vec2& rhs) const
	{
		return Vec2(x + rhs.x, y + rhs.y);
	}

	inline const Vec2 operator - (const Vec2& rhs) const
	{	
		return Vec2(x - rhs.x, y - rhs.y);
	}

	inline Vec2& operator += (const Vec2& rhs)
	{		
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	inline Vec2& operator -= (const Vec2& rhs)
	{	
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	inline bool operator == (const Vec2& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y);
	}

	inline bool operator != (const Vec2& rhs) const
	{
		return (x != rhs.x) || (y != rhs.y);
	}

	// For sorting Vec2's
	inline bool operator < (const Vec2& rhs) const
	{
		if(x < rhs.x)
			return true;
		else if(x > rhs.x)
			return false;
		else // else if x == rhs.x:
		{
			return y < rhs.y;
		}
	}

	inline Real& operator[] (unsigned int index)
	{
		assert(index < 3);
		return ((Real*)(&x))[index];
	}

	inline const Real& operator[] (unsigned int index) const
	{
		assert(index < 3);
		return ((Real*)(&x))[index];
	}

	inline void normalise()
	{
		const Real inverselength = Real(1.0) / length();

		x *= inverselength;
		y *= inverselength;
	}

	inline Real normalise_ret_length()
	{
		const Real len = length();

		const Real inverselength = Real(1.0) / len;

		x *= inverselength;
		y *= inverselength;

		return len;
	}

	inline Real length() const
	{
		return (Real)sqrt(x*x + y*y);
	}

	inline Real length2() const
	{
		return x*x + y*y;
	}

	inline void setLength(Real newlength)
	{
		const Real current_len = length();

		if(!current_len)
			return;

		scale(newlength / current_len);
	}

	inline Real getDist2(const Vec2& other) const
	{
		const Vec2 dif = other - *this;
		return dif.length2();
	}

	inline Real getDist(const Vec2& other) const
	{
		const Vec2 dif = other - *this;
		return dif.length();
	}

	inline void scale(Real factor)
	{
		x *= factor;
		y *= factor;
	}

	inline Vec2& operator *= (Real factor)
	{
		x *= factor;
		y *= factor;
		return *this;
	}

	inline Vec2& operator /= (Real divisor)
	{
		*this *= Real(1.0) / divisor;
		return *this;
	}

	inline const Vec2 operator * (Real factor) const
	{
		return Vec2(x * factor, y * factor);
	}

	inline const Vec2 operator / (Real divisor) const
	{
		const Real inverse_d = Real(1.0) / divisor;

		return Vec2(x * inverse_d, y * inverse_d);
	}

	inline void zero()
	{
		x = 0.0;
		y = 0.0;
	}

	void print() const;

	const static Vec2 zerovector;	//(0,0,)
	const static Vec2 i;			//(1,0,)
	const static Vec2 j;			//(0,1,0)

	Real x,y;

	Real dotProduct(const Vec2& rhs) const
	{
		return x*rhs.x + y*rhs.y;
	}

	Real dot(const Vec2& rhs) const
	{
		return dotProduct(rhs);
	}

	inline const Real* data() const { return (Real*)this; }

	inline void sub(const Vec2& other)
	{
		x -= other.x;
		y -= other.y;
	}

	inline void subMult(const Vec2& other, Real factor)
	{
		x -= other.x * factor;
		y -= other.y * factor;
	}

	inline void removeComponentInDir(const Vec2& unitdir)
	{
		subMult(unitdir, this->dot(unitdir));
	}

	// Returns true if all components c satisfy c >= minval && c < maxval, i.e. c e [minval, maxval)
	inline bool inHalfClosedInterval(Real minval, Real maxval) const
	{
		return Maths::inHalfClosedInterval(x, minval, maxval) && Maths::inHalfClosedInterval(y, minval, maxval);
	}

	inline const Vec2 clamp(const Vec2& lo, const Vec2& up) const
	{
		return Vec2(
			myClamp(x, lo.x, up.x),
			myClamp(y, lo.y, up.y)
			);
	}

	inline const Vec2 min(const Vec2& other) const
	{
		return Vec2(
			myMin(x, other.x),
			myMin(y, other.y)
			);
	}

	inline const Vec2 max(const Vec2& other) const
	{
		return Vec2(
			myMax(x, other.x),
			myMax(y, other.y)
			);
	}
};




template <class Real>
inline Real dotProduct(const Vec2<Real>& v1, const Vec2<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y);
}

template <class Real>
inline Real dot(const Vec2<Real>& v1, const Vec2<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y);
}

template <class Real>
inline Real angleBetweenNormalized(const Vec2<Real>& v1, const Vec2<Real>& v2)
{
	const Real dp = dotProduct(v1, v2);

	return (Real)acos(dp);
}

template <class Real>
inline const Vec2<Real> normalise(const Vec2<Real>& v)
{
	return v / v.length();
}


// Unary -
template <class Real> 
inline const Vec2<Real> operator - (const Vec2<Real>& v)
{
	return Vec2<Real>(-v.x, -v.y);
}


template <class Real>
inline Vec2<Real> operator * (Real a, const Vec2<Real>& v)
{
	return Vec2<Real>(a * v.x, a * v.y);
}


template <class T>
inline Vec2<T> div(const Vec2<T>& a, const Vec2<T>& b)
{
	return Vec2<T>(a.x / b.x, a.y / b.y);
}


template <class T>
inline const Vec2<T> min(const Vec2<T>& a, const Vec2<T>& b)
{
	return Vec2<T>(
		myMin(a.x, b.x),
		myMin(a.y, b.y)
		);
}


template <class T>
inline const Vec2<T> max(const Vec2<T>& a, const Vec2<T>& b)
{
	return Vec2<T>(
		myMax(a.x, b.x),
		myMax(a.y, b.y)
	);
}


template <class Real>
inline bool epsEqual(const Vec2<Real>& a, const Vec2<Real>& b)
{
	return ::epsEqual(a.x, b.x) && ::epsEqual(a.y, b.y);
}

template <class Real>
inline bool epsEqualWithEps(const Vec2<Real>& a, const Vec2<Real>& b, float eps)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps);
}


template <class Real>
inline void writeToStream(const Vec2<Real>& v, OutStream& stream)
{
	stream.writeData(&v.x, sizeof(Real) * 2);
}

template <class Real>
inline Vec2<Real> readVec2FromStream(InStream& stream)
{
	Vec2<Real> v;
	stream.readData(&v.x, sizeof(Real) * 2);
	return v;
}


template <class Real>
inline const std::string toString(const Vec2<Real>& v)
{
	return v.toString();
}


template <class Real>
inline const Vec2<int> toVec2i(const Vec2<Real>& v)
{
	return Vec2<int>((int)v.x, (int)v.y);
}

inline const Vec2<float> toVec2f(const Vec2<double>& v)
{
	return Vec2<float>((float)v.x, (float)v.y);
}

inline const Vec2<double> toVec2d(const Vec2<float>& v)
{
	return Vec2<double>((double)v.x, (double)v.y);
}


template <class Real>
inline Vec2<Real> closestPointOnLine(const Vec2<Real>& p, const Vec2<Real>& p_a, const Vec2<Real>& p_b)
{
	const Vec2<Real> v = p_b - p_a;
	Real t = dot(p - p_a, v) / v.length2();
	return p_a + v * t;
}


template <class Real>
inline Vec2<Real> closestPointOnLineSegment(const Vec2<Real>& p, const Vec2<Real>& p_a, const Vec2<Real>& p_b)
{
	const Vec2<Real> v = p_b - p_a;
	Real t = dot(p - p_a, v) / v.length2();
	t = myClamp<Real>(t, 0, 1);
	return p_a + v * t;
}


template <class Real>
inline Real pointLineSegmentDist(const Vec2<Real>& p, const Vec2<Real>& a, const Vec2<Real>& b)
{
	const Vec2<Real> p_closest = closestPointOnLineSegment(p, a, b);
	return (p - p_closest).length();
}


typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<int> Vec2i;
