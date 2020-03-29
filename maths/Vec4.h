/*=====================================================================
Vec4.h
------
Copyright Glare Technologies Limited 2019 - 
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include <assert.h>


/*=====================================================================
Vec4
----
Basic templated four-component vector class.

When dealing with floats, prefer to use the more optimised Vec4f.
=====================================================================*/
template <class T>
class Vec4
{
public:
	INDIGO_STRONG_INLINE Vec4()
	{}

	INDIGO_STRONG_INLINE explicit Vec4(T x_)
		: x(x_),
		y(x_),
		z(x_),
		w(x_)
	{}

	INDIGO_STRONG_INLINE Vec4(T x_, T y_, T z_, T w_)
		: x(x_),
		y(y_),
		z(z_),
		w(w_)
	{}

	INDIGO_STRONG_INLINE void set(T newx, T newy, T newz, T neww)
	{
		x = newx;
		y = newy;
		z = newz;
		w = neww;
	}

	INDIGO_STRONG_INLINE T& operator[] (unsigned int index)
	{
		assert(index < 4);
		return (&x)[index];
	}

	INDIGO_STRONG_INLINE const T& operator[] (unsigned int index) const
	{
		assert(index < 4);
		return (&x)[index];
	}

	INDIGO_STRONG_INLINE const Vec4 operator + (const Vec4& rhs) const
	{
		return Vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}


	INDIGO_STRONG_INLINE const Vec4 operator - (const Vec4& rhs) const
	{
		return Vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}

	INDIGO_STRONG_INLINE const Vec4 operator * (const Vec4& rhs) const
	{
		return Vec4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
	}

	INDIGO_STRONG_INLINE Vec4& operator += (const Vec4& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}

	INDIGO_STRONG_INLINE Vec4& operator -= (const Vec4& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}

	INDIGO_STRONG_INLINE Vec4& operator = (const Vec4& rhs)
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		w = rhs.w;
		return *this;
	}

	INDIGO_STRONG_INLINE bool operator == (const Vec4& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y) && (z == rhs.z) && (w == rhs.w);
	}

	INDIGO_STRONG_INLINE bool operator != (const Vec4& rhs) const
	{
		return (x != rhs.x) || (y != rhs.y) || (z != rhs.z) || (w != rhs.w);
	}

	// Unary -
	INDIGO_STRONG_INLINE const Vec4<T> operator - () const
	{
		return Vec4<T>(-x, -y, -z, -w);
	}

	INDIGO_STRONG_INLINE T length() const
	{
		return std::sqrt(length2());
	}

	INDIGO_STRONG_INLINE T length2() const
	{
		return x*x + y*y + z*z + w*w;
	}

	INDIGO_STRONG_INLINE void scale(T factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
		w *= factor;
	}

	INDIGO_STRONG_INLINE Vec4& operator *= (T factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
		w *= factor;
		return *this;
	}

	INDIGO_STRONG_INLINE Vec4& operator /= (T divisor)
	{
		assert(divisor != 0);
		*this *= (1 / divisor);
		return *this;
	}

	INDIGO_STRONG_INLINE const Vec4 operator * (T factor) const
	{
		return Vec4(x * factor, y * factor, z * factor, w * factor);
	}

	INDIGO_STRONG_INLINE const Vec4 operator / (T divisor) const
	{
		return *this * (1 / divisor);
	}

	INDIGO_STRONG_INLINE T getDist(const Vec4& other) const
	{
		return std::sqrt(getDist2(other));
	}

	INDIGO_STRONG_INLINE T getDist2(const Vec4& other) const
	{
		return length2(other - *this);
	}

	INDIGO_STRONG_INLINE const T* data() const { return &x; }

	INDIGO_STRONG_INLINE T dotProduct(const Vec4& rhs) const
	{
		return x*rhs.x + y*rhs.y + z*rhs.z + w*rhs.w;
	}

	INDIGO_STRONG_INLINE T dot(const Vec4& rhs) const
	{
		return dotProduct(rhs);
	}

	INDIGO_STRONG_INLINE bool isUnitLength() const
	{
		return ::epsEqual(length(), (T)1.0);
	}

	T x, y, z, w;
};


void doVec4Tests();


template <class T>
INDIGO_STRONG_INLINE Vec4<T> removeComponentInDir(const Vec4<T>& v, const Vec4<T>& unit_dir)
{
	assert(unit_dir.isUnitLength());
	return v - unit_dir * dot(v, unit_dir);
}


template <class T>
INDIGO_STRONG_INLINE const Vec4<T> normalise(const Vec4<T>& v)
{
	return v * (1 / v.length());
}


template <class T>
INDIGO_STRONG_INLINE T dotProduct(const Vec4<T>& v1, const Vec4<T>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
}


template <class T>
INDIGO_STRONG_INLINE T dot(const Vec4<T>& v1, const Vec4<T>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
}


template <class T>
INDIGO_STRONG_INLINE const Vec4<T> crossProduct(const Vec4<T>& v1, const Vec4<T>& v2)
{
	return Vec4<T>(
		(v1.y * v2.z) - (v1.z * v2.y),
		(v1.z * v2.x) - (v1.x * v2.z),
		(v1.x * v2.y) - (v1.y * v2.x),
		0
	);
}


// NOTE: we'll just define this in the relevant .cpp files like vec4.cpp, so it can go after epsEqual defined for Complex args.
/*template <class T>
INDIGO_STRONG_INLINE bool epsEqual(const Vec4<T>& a, const Vec4<T>& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps) && ::epsEqual(a.w, b.w, eps);
}
*/