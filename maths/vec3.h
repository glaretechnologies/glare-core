/*=====================================================================
vec3.h
------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include "Vec4f.h"
#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include "../utils/Platform.h"
#include <assert.h>
#include <string>


#define VEC3_INLINE GLARE_STRONG_INLINE


template <class Real>
class Vec3
{
public:

	typedef Real RealType;

	VEC3_INLINE Vec3()
	{}

	VEC3_INLINE explicit Vec3(Real x_)
	:	x(x_),
		y(x_),
		z(x_)
	{}

	VEC3_INLINE Vec3(Real x_, Real y_, Real z_)
	:	x(x_),
		y(y_),
		z(z_)
	{}

	VEC3_INLINE Vec3(const Real* f)
	:	x(f[0]),
		y(f[1]),
		z(f[2])
	{}

	VEC3_INLINE Vec3(const Vec3& v, Real scale)
	:	x(v.x * scale),
		y(v.y * scale),
		z(v.z * scale)
	{}

	VEC3_INLINE explicit Vec3(const Vec4f& v)
	:	x(v.x[0]),
		y(v.x[1]),
		z(v.x[2])
	{}


	VEC3_INLINE void set(Real newx, Real newy, Real newz)
	{
		x = newx;
		y = newy;
		z = newz;
	}

	VEC3_INLINE Real& operator[] (unsigned int index)
	{
		assert(index < 3);
		return (&x)[index];
	}

	VEC3_INLINE const Real& operator[] (unsigned int index) const
	{
		assert(index < 3);
		return (&x)[index];
	}

	VEC3_INLINE const Vec3 operator + (const Vec3& rhs) const
	{
		return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
	}


	VEC3_INLINE const Vec3 operator - (const Vec3& rhs) const
	{
		return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	VEC3_INLINE const Vec3 operator * (const Vec3& rhs) const
	{
		return Vec3(x * rhs.x, y * rhs.y, z * rhs.z);
	}

	VEC3_INLINE Vec3& operator += (const Vec3& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	VEC3_INLINE Vec3& operator -= (const Vec3& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}

	VEC3_INLINE bool operator == (const Vec3& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y) && (z == rhs.z);
	}

	VEC3_INLINE bool operator != (const Vec3& rhs) const
	{
		return (x != rhs.x) || (y != rhs.y) || (z != rhs.z);
	}

	//for sorting Vec3's
	/*inline bool operator < (const Vec3& rhs) const
	{
		if(x < rhs.x)
			return true;
		else if(x > rhs.x)
			return false;
		else	//else x == rhs.x
		{
			if(y < rhs.y)
				return true;
			else if(y > rhs.y)
				return false;
			else
			{
				return z < rhs.z;
			}
		}
	}*/

	VEC3_INLINE void normalise()
	{
		//assert(length() != 0);
		const Real inverselength = Real(1.0) / length();

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;
	}


	/*inline void fastNormalise()
	{
		const Real inverselength = RSqrt( length2() );

		//if(!inverselength)
		//	return;

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;
	}*/

	VEC3_INLINE Real normalise_ret_length()
	{
		assert(length() != 0);
		const Real len = length();
		const Real inverselength = (Real)1.0 / len;

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		return len;
	}

	VEC3_INLINE Real normalise_ret_length(Real& inv_len_out)
	{
		assert(length() != 0);
		const Real len = length();
		const Real inverselength = (Real)1.0 / len;

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		inv_len_out = inverselength;

		return len;
	}

	VEC3_INLINE Real normalise_ret_length2()
	{
		assert(length2() != 0);
		const Real len2 = length2();
		const Real inverselength = (Real)1.0 / std::sqrt(len2);

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		return len2;
	}


	VEC3_INLINE const Vec3<Real> negated() const
	{
		return Vec3<Real>(-x, -y, -z);
	}

	VEC3_INLINE Real length() const
	{
		return std::sqrt(x*x + y*y + z*z);
	}

	VEC3_INLINE Real length2() const
	{
		return x*x + y*y + z*z;
	}

	VEC3_INLINE void scale(Real factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
	}

	VEC3_INLINE Vec3& operator *= (Real factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
		return *this;
	}

	VEC3_INLINE void setLength(Real newlength)
	{
		scale(newlength / length());
	}

	VEC3_INLINE Vec3& operator /= (Real divisor)
	{
		assert(divisor != 0);
		*this *= ((Real)1.0 / divisor);
		return *this;
	}

	VEC3_INLINE const Vec3 operator * (Real factor) const
	{
		return Vec3(x * factor, y * factor, z * factor);
	}

	VEC3_INLINE const Vec3 operator / (Real divisor) const
	{
		assert(divisor != 0);
		const Real inverse_d = 1 / divisor;

		return Vec3(x * inverse_d, y * inverse_d, z * inverse_d);
	}

	VEC3_INLINE void zero()
	{
		x = 0.0;
		y = 0.0;
		z = 0.0;
	}

	VEC3_INLINE Real getDist(const Vec3& other) const
	{
		//const Vec3 dif = other - *this;
		//return dif.length();
		return std::sqrt(getDist2(other));
	}

	VEC3_INLINE Real getDist2(const Vec3& other) const
	{
		//const Vec3 dif = other - *this;
		//return dif.length2();
		//Real sum = other.x - x;

		//sum += other.y - y;

		//sum += other.z - z;

		Real sum = other.x - x;
		sum *= sum;

		Real dif = other.y - y;
		sum += dif*dif;

		dif = other.z - z;

		return sum + dif*dif;


		//return (other.x - x) + (other.y - y) + (other.z - z);
	}

	VEC3_INLINE void assertUnitVector() const
	{
#ifdef DEBUG
		const Real len = length();

		const Real var = std::fabs(1 - len);

		const Real EPSILON_ = 0.001;

		assert(var <= EPSILON_);
#endif
	}

	void print() const;
	const std::string toString() const;
	const std::string toStringFullPrecision() const;
	const std::string toStringMaxNDecimalPlaces(int n) const;

//	const static Vec3 zerovector;	//(0,0,0)
//	const static Vec3 i;			//(1,0,0)
//	const static Vec3 j;			//(0,1,0)
//	const static Vec3 k;			//(0,0,1)

	Real x,y,z;



	VEC3_INLINE const Real* data() const { return (Real*)this; }




	//-----------------------------------------------------------------
	//Euler angle stuff
	//-----------------------------------------------------------------
	//static Vec3 ws_up;//default z axis
	//static Vec3 ws_right;//default -y axis
	//static Vec3 ws_forwards;//must = crossProduct(ws_up, ws_right).   default x axis

	//static void setWsUp(const Vec3& vec){ ws_up = vec; }
	//static void setWsRight(const Vec3& vec){ ws_right = vec; }
	//static void setWsForwards(const Vec3& vec){ ws_forwards = vec; }

	/*Real getYaw() const { return x; }
	Real getPitch() const { return y; }
	Real getRoll() const { return z; }

	void setYaw(Real newyaw){ x = newyaw; }
	void setPitch(Real newpitch){ y = newpitch; }
	void setRoll(Real newroll){ z = newroll; }*/

	/*==================================================================
	getAngles
	---------
	Gets the Euler angles of this vector.  Returns the vector (yaw, pitch, roll).
	Yaw is the angle between this vector and the vector 'ws_forwards' measured
	anticlockwise when looking towards the origin from along the vector 'ws_up'.
	Yaw will be in the range (-Pi, Pi).

	Pitch is the angle between this vector and the vector 'ws_forwards' as seen when
	looking from along the vecotr 'ws_right' towards the origin.  A pitch of Pi means
	the vector is pointing along the vector 'ws_up', a pitch of -Pi means the vector is
	pointing straight down. (ie pointing in the opposite direction from 'ws_up'.
	A pitch of 0 means the vector is in the 'ws_right'-'ws_forwards' plane.
	Will be in the range [-Pi/2, Pi/2].

	Roll will be 0.
	====================================================================*/
	const Vec3 getAngles(const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right) const;
	//const Vec3 getAngles() const; //around i, j, k

	const Vec3 fromAngles(const Vec3& ws_forwards, const Vec3& ws_up, const Vec3& ws_right) const;

	VEC3_INLINE Real dotProduct(const Vec3& rhs) const
	{
		return x*rhs.x + y*rhs.y + z*rhs.z;
	}

	VEC3_INLINE Real dot(const Vec3& rhs) const
	{
		return dotProduct(rhs);
	}

	static const Vec3 randomVec(Real component_lowbound, Real component_highbound);


	VEC3_INLINE void setToMult(const Vec3& other, Real factor)
	{
		x = other.x * factor;
		y = other.y * factor;
		z = other.z * factor;
	}

	VEC3_INLINE void addMult(const Vec3& other, Real factor)
	{
		x += other.x * factor;
		y += other.y * factor;
		z += other.z * factor;
	}

	VEC3_INLINE void subMult(const Vec3& other, Real factor)
	{
		x -= other.x * factor;
		y -= other.y * factor;
		z -= other.z * factor;
	}

	VEC3_INLINE void add(const Vec3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
	}

	VEC3_INLINE void sub(const Vec3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
	}

	VEC3_INLINE void removeComponentInDir(const Vec3& unitdir)
	{
		subMult(unitdir, this->dot(unitdir));
	}

	VEC3_INLINE bool isUnitLength() const
	{
		return ::epsEqual(length(), (Real)1.0);
	}

	//will be in range [-Pi/2, Pi/2]
	VEC3_INLINE Real theta() const
	{
		return std::atan2(std::sqrt(x*x + y*y), z);
	}

	VEC3_INLINE Real phi() const
	{
		Real phi = std::atan2(x, y);
		if(phi < 0.0)
		    phi += NICKMATHS_2PI;
		return phi;
	}

	VEC3_INLINE Real r() const
	{
		return length();
	}

	/*inline void clamp(Real lowerbound, Real upperbound)
	{
		x = myClamp(x, lowerbound, upperbound);
		y = myClamp(y, lowerbound, upperbound);
		z = myClamp(z, lowerbound, upperbound);
	}*/

	VEC3_INLINE const Vec3 clamp(const Vec3& lo, const Vec3& up) const
	{
		return Vec3(
			myClamp(x, lo.x, up.x),
			myClamp(y, lo.y, up.y),
			myClamp(z, lo.z, up.z)
			);
	}

	VEC3_INLINE const Vec3 min(const Vec3& other) const
	{
		return Vec3(
			myMin(x, other.x),
			myMin(y, other.y),
			myMin(z, other.z)
			);
	}

	VEC3_INLINE const Vec3 max(const Vec3& other) const
	{
		return Vec3(
			myMax(x, other.x),
			myMax(y, other.y),
			myMax(z, other.z)
			);
	}

	VEC3_INLINE void lowerClamp(Real lowerbound)
	{
		x = myMax(x, lowerbound);
		y = myMax(y, lowerbound);
		z = myMax(z, lowerbound);
	}

	// returns true if all components c satisfy c >= minval && c < maxval, i.e. c e [minval, maxval)
	VEC3_INLINE bool inHalfClosedInterval(Real minval, Real maxval) const
	{
		return
			Maths::inHalfClosedInterval(x, minval, maxval) &&
			Maths::inHalfClosedInterval(y, minval, maxval) &&
			Maths::inHalfClosedInterval(z, minval, maxval);
	}


	VEC3_INLINE bool isFinite() const
	{
		return ::isFinite(x) && ::isFinite(y) && ::isFinite(z);
	}


	VEC3_INLINE void vectorToVec4f(Vec4f& v) const
	{
		v.set((float)x, (float)y, (float)z, 0.0f);
	}


	VEC3_INLINE void pointToVec4f(Vec4f& v) const
	{
		v.set((float)x, (float)y, (float)z, 1.0f);
	}

	VEC3_INLINE const Vec4f toVec4fPoint() const
	{
		return Vec4f((float)x, (float)y, (float)z, 1.0f);
	}

	VEC3_INLINE const Vec4f toVec4fVector() const
	{
		return Vec4f((float)x, (float)y, (float)z, 0.0f);
	}

	static void test();
};

template <class Real>
VEC3_INLINE const Vec3<Real> normalise(const Vec3<Real>& v)
{
	return v * ((Real)1.0 / v.length());
}

template <class Real>
VEC3_INLINE const Vec3<Real> normalise(const Vec3<Real>& v, Real& original_length_out)
{
	original_length_out = v.length();
	return v * ((Real)1.0 / original_length_out);
}

template <class Real>
VEC3_INLINE const Vec3<Real> operator * (Real m, const Vec3<Real>& right)
{
	return Vec3<Real>(right.x * m, right.y * m, right.z * m);
}

template <class Real>
VEC3_INLINE Real dotProduct(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}


template <class Real>
VEC3_INLINE Real dot(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}


template <class Real>
VEC3_INLINE const Vec3<Real> crossProduct(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return Vec3<Real>(
	(v1.y * v2.z) - (v1.z * v2.y),
	(v1.z * v2.x) - (v1.x * v2.z),
	(v1.x * v2.y) - (v1.y * v2.x)
	);
}


// Unary -
template <class Real>
VEC3_INLINE const Vec3<Real> operator - (const Vec3<Real>& v)
{
	return Vec3<Real>(-v.x, -v.y, -v.z);
}

	//v1 and v2 unnormalized
template <class Real>
VEC3_INLINE Real angleBetween(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	const Real lf = v1.length() * v2.length();

	if(!lf)
		return 1.57079632679489661923f;

	const Real dp = dotProduct(v1, v2);

	return std::acos(dp / lf);
}

template <class Real>
VEC3_INLINE Real angleBetweenNormalized(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return std::acos(dotProduct(v1, v2));
}

template <class Real>
VEC3_INLINE const Vec3<Real> removeComponentInDir(const Vec3<Real>& v, const Vec3<Real>& unit_dir)
{
	assert(unit_dir.isUnitLength());
	return v - unit_dir * dot(v, unit_dir);
}


/*inline bool epsEqual(const Vec3& v1, const Vec3& v2)
{
	const Real dp = dotProduct(v1, v2);

	return dp >= 0.99999f;
}*/

/*template <class Real>
inline bool epsEqual(const Vec3<Real>& a, const Vec3<Real>& b)
{
	return ::epsEqual(a.x, b.x) && ::epsEqual(a.y, b.y) && ::epsEqual(a.z, b.z);
}*/

/*template <class Real>
inline bool epsEqual(const Vec3<Real>& a, const Vec3<Real>& b, Real eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}*/

VEC3_INLINE bool epsEqual(const Vec3<float>& a, const Vec3<float>& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}

VEC3_INLINE bool epsEqual(const Vec3<double>& a, const Vec3<double>& b, double eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}

template <class Real>
inline bool approxEq(const Vec3<Real>& a, const Vec3<Real>& b, Real eps = NICKMATHS_EPSILON)
{
	return Maths::approxEq(a.x, b.x, eps) && Maths::approxEq(a.y, b.y, eps) && Maths::approxEq(a.z, b.z, eps);
}

template <class Real>
inline Vec3<Real> min(const Vec3<Real>& a, const Vec3<Real>& b)
{
	return Vec3<Real>(
		myMin(a.x, b.x),
		myMin(a.y, b.y),
		myMin(a.z, b.z)
	);
}

template <class Real>
inline Vec3<Real> max(const Vec3<Real>& a, const Vec3<Real>& b)
{
	return Vec3<Real>(
		myMax(a.x, b.x),
		myMax(a.y, b.y),
		myMax(a.z, b.z)
	);
}

//for sorting Vec3's
template <class Real>
VEC3_INLINE bool operator < (const Vec3<Real>& a, const Vec3<Real>& b)
{
	if(a.x < b.x)
		return true;
	else if(a.x > b.x)
		return false;
	else // else a.x == b.x:
	{
		if(a.y < b.y)
			return true;
		else if(a.y > b.y)
			return false;
		else // else a.y == b.y:
		{
			return a.z < b.z;
		}
	}
}

template <class Real>
VEC3_INLINE const Vec3<Real> lerp(const Vec3<Real>& a, const Vec3<Real>& b, Real t)
{
	assert(t >= (Real)0.0 && t <= (Real)1.0);
	return a * ((Real)1.0 - t) + b * t;
}


template <class Real>
inline const std::string toString(const Vec3<Real>& v)
{
	return v.toString();
}


VEC3_INLINE const Vec3<float> toVec3f(const Vec3<double>& v)
{
	return Vec3<float>((float)v.x, (float)v.y, (float)v.z);
}

VEC3_INLINE const Vec3<double> toVec3d(const Vec3<float>& v)
{
	return Vec3<double>((double)v.x, (double)v.y, (double)v.z);
}

VEC3_INLINE const Vec3<double> toVec3d(const Vec4f& v)
{
	return Vec3<double>((double)v.x[0], (double)v.x[1], (double)v.x[2]);
}

VEC3_INLINE const Vec3<float> toVec3f(const Vec4f& v)
{
	return Vec3<float>((float)v.x[0], (float)v.x[1], (float)v.x[2]);
}


template <class Real>
inline void writeToStream(const Vec3<Real>& v, OutStream& stream)
{
	stream.writeData(&v.x, sizeof(Real) * 3);
}


template <class Real>
inline Vec3<Real> readVec3FromStream(InStream& stream)
{
	Vec3<Real> v;
	stream.readData(&v.x, sizeof(Real) * 3);
	return v;
}


typedef Vec3<float> Vec3f;
typedef Vec3<double> Vec3d;
typedef Vec3<int> Vec3i;
