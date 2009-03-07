/*===================================================================
Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __VEC3_H__
#define __VEC3_H__

/*=================================================================
3 component vector class
------------------------
Coded by Nick Chapman in the year 2000-
=================================================================*/

#include "mathstypes.h"
#include <assert.h>
#include <string>

template <class Real>
class Vec3
{
public:

	inline Vec3()
	{}

	inline ~Vec3()
	{}

	inline explicit Vec3(Real x_)
	:	x(x_),
		y(x_),
		z(x_)
	{}

	inline Vec3(Real x_, Real y_, Real z_)
	:	x(x_),
		y(y_),
		z(z_)
	{}

	inline Vec3(const Vec3& rhs)
	:	x(rhs.x),
		y(rhs.y),
		z(rhs.z)
	{}

	/*inline Vec3<float>(const Vec3<double>& rhs)
	:	x((float)rhs.x),
		y((float)rhs.y),
		z((float)rhs.z)
	{}*/

	inline Vec3(const Real* f)
	:	x(f[0]),
		y(f[1]),
		z(f[2])
	{}

	inline Vec3(const Vec3& v, Real scale)
	:	x(v.x * scale),
		y(v.y * scale),
		z(v.z * scale)
	{}
		

	inline void set(Real newx, Real newy, Real newz)
	{
		x = newx;
		y = newy;
		z = newz;
	}

	inline Real& operator[] (unsigned int index)
	{
		assert(index >= 0 && index < 3);
		return ((Real*)(&x))[index];
	}

	inline const Real& operator[] (unsigned int index) const
	{
		assert(index >= 0 && index < 3);
		return ((Real*)(&x))[index];
	}

	inline const Vec3 operator + (const Vec3& rhs) const
	{
		return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
	}


	inline const Vec3 operator - (const Vec3& rhs) const
	{	
		return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	inline const Vec3 operator * (const Vec3& rhs) const
	{	
		return Vec3(x * rhs.x, y * rhs.y, z * rhs.z);
	}

	inline Vec3& operator += (const Vec3& rhs)
	{		
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	inline Vec3& operator -= (const Vec3& rhs)
	{	
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}

	inline Vec3& operator = (const Vec3& rhs)
	{	
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		return *this;
	}

	inline bool operator == (const Vec3& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y) && (z == rhs.z);
	}

	inline bool operator != (const Vec3& rhs) const
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

	inline void normalise()
	{
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

	inline Real normalise_ret_length()
	{
		const Real len = length();
		const Real inverselength = (Real)1.0 / len;

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		return len;
	}

	inline Real normalise_ret_length(Real& inv_len_out)
	{
		const Real len = length();
		const Real inverselength = (Real)1.0 / len;

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		inv_len_out = inverselength;

		return len;
	}

	inline Real normalise_ret_length2()
	{
		const Real len2 = length2();
		const Real inverselength = (Real)1.0 / sqrt(len2);

		x *= inverselength;
		y *= inverselength;
		z *= inverselength;

		return len2;
	}


	inline const Vec3<Real> negated() const
	{
		return Vec3<Real>(-x, -y, -z);
	}

	inline Real length() const
	{
		return sqrt(x*x + y*y + z*z);
	}

	inline Real length2() const
	{
		return x*x + y*y + z*z;
	}

	inline void scale(Real factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
	}

	inline Vec3& operator *= (Real factor)
	{
		x *= factor;
		y *= factor;
		z *= factor;
		return *this;
	}

	inline void setLength(Real newlength)
	{
		scale(newlength / length());
	}

	inline Vec3& operator /= (Real divisor)
	{
		*this *= ((Real)1.0 / divisor);
		return *this;
	}

	inline const Vec3 operator * (Real factor) const
	{
		return Vec3(x * factor, y * factor, z * factor);
	}

	inline const Vec3 operator / (Real divisor) const
	{
		const Real inverse_d = 1.0 / divisor;

		return Vec3(x * inverse_d, y * inverse_d, z * inverse_d);
	}

	inline void zero()
	{
		x = 0.0;
		y = 0.0;
		z = 0.0;
	}

	inline Real getDist(const Vec3& other) const
	{
		//const Vec3 dif = other - *this;
		//return dif.length();
		return sqrt(getDist2(other));
	}

	inline Real getDist2(const Vec3& other) const
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

	inline void assertUnitVector() const
	{
#ifdef DEBUG
		const Real len = length();

		const Real var = fabs(1.0 - len);

		const Real EPSILON_ = 0.001;

		assert(var <= EPSILON_);
#endif
	}

	void print() const;
	const std::string toString() const;
	const std::string toStringFullPrecision() const;

//	const static Vec3 zerovector;	//(0,0,0)
//	const static Vec3 i;			//(1,0,0)
//	const static Vec3 j;			//(0,1,0)
//	const static Vec3 k;			//(0,0,1)

	Real x,y,z;



	inline const Real* data() const { return (Real*)this; }
	



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

	Real dotProduct(const Vec3& rhs) const
	{
		return x*rhs.x + y*rhs.y + z*rhs.z;
	}

	Real dot(const Vec3& rhs) const
	{
		return dotProduct(rhs);
	}

	static const Vec3 randomVec(Real component_lowbound, Real component_highbound);


	inline void setToMult(const Vec3& other, Real factor)
	{
		x = other.x * factor;
		y = other.y * factor;
		z = other.z * factor;
	}

	inline void addMult(const Vec3& other, Real factor)
	{
		x += other.x * factor;
		y += other.y * factor;
		z += other.z * factor;
	}

	inline void subMult(const Vec3& other, Real factor)
	{
		x -= other.x * factor;
		y -= other.y * factor;
		z -= other.z * factor;
	}

	inline void add(const Vec3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
	}

	inline void sub(const Vec3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
	}

	inline void removeComponentInDir(const Vec3& unitdir)
	{
		subMult(unitdir, this->dot(unitdir));
	}

	inline bool isUnitLength() const
	{
		return ::epsEqual(length(), (Real)1.0);
	}
	
	//will be in range [-Pi/2, Pi/2]
	inline Real theta() const
	{
		return atan2(sqrt(x*x + y*y), z);
	}
	
	inline Real phi() const
	{
		Real phi = atan2(x, y);
		if(phi < 0.0)
		    phi += NICKMATHS_2PI;
		return phi;
	}
	
	inline Real r() const
	{
		return length();
	}

	/*inline void clamp(Real lowerbound, Real upperbound)
	{
		x = myClamp(x, lowerbound, upperbound);
		y = myClamp(y, lowerbound, upperbound);
		z = myClamp(z, lowerbound, upperbound);
	}*/

	inline const Vec3 clamp(const Vec3& lo, const Vec3& up) const
	{
		return Vec3(
			myClamp(x, lo.x, up.x),
			myClamp(y, lo.y, up.y),
			myClamp(z, lo.z, up.z)
			);
	}

	inline const Vec3 min(const Vec3& other) const
	{
		return Vec3(
			myMin(x, other.x),
			myMin(y, other.y),
			myMin(z, other.z)
			);
	}

	inline const Vec3 max(const Vec3& other) const
	{
		return Vec3(
			myMax(x, other.x),
			myMax(y, other.y),
			myMax(z, other.z)
			);
	}

	inline void lowerClamp(Real lowerbound)
	{
		x = myMax(x, lowerbound);
		y = myMax(y, lowerbound);
		z = myMax(z, lowerbound);
	}

	// returns true if all components c satisfy c >= minval && c < maxval, i.e. c e [minval, maxval)
	inline bool inHalfClosedInterval(Real minval, Real maxval) const
	{
		return 
			Maths::inHalfClosedInterval(x, minval, maxval) && 
			Maths::inHalfClosedInterval(y, minval, maxval) &&
			Maths::inHalfClosedInterval(z, minval, maxval);
	}

};

template <class Real>
inline const Vec3<Real> normalise(const Vec3<Real>& v)
{
	return v * ((Real)1.0 / v.length());
}

template <class Real>
inline const Vec3<Real> operator * (Real m, const Vec3<Real>& right)
{
	return Vec3<Real>(right.x * m, right.y * m, right.z * m);
}

template <class Real>
inline Real dotProduct(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

template <class Real>
inline Real dot(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

template <class Real>
inline Real absDot(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return std::fabs(dot(v1, v2));
}

template <class Real>
inline const Vec3<Real> crossProduct(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return Vec3<Real>(
	(v1.y * v2.z) - (v1.z * v2.y),
	(v1.z * v2.x) - (v1.x * v2.z),
	(v1.x * v2.y) - (v1.y * v2.x)
	);	//NOTE: check me

}

	//v1 and v2 unnormalized
template <class Real>
inline Real angleBetween(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	const Real lf = v1.length() * v2.length();

	if(!lf)
		return 1.57079632679489661923f;

	const Real dp = dotProduct(v1, v2);

	return acos( dp / lf);
}

template <class Real>
inline Real angleBetweenNormalized(const Vec3<Real>& v1, const Vec3<Real>& v2)
{
	return acos(dotProduct(v1, v2));
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

inline bool epsEqual(const Vec3<float>& a, const Vec3<float>& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}

inline bool epsEqual(const Vec3<double>& a, const Vec3<double>& b, double eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}


//for sorting Vec3's
template <class Real>
inline bool operator < (const Vec3<Real>& a, const Vec3<Real>& b)
{
	if(a.x < b.x)
		return true;
	else if(a.x > b.x)
		return false;
	else	//else x == rhs.x
	{
		if(a.y < b.y)
			return true;
		else if(a.y > b.y)
			return false;
		else
		{
			return a.z < b.z;
		}
	}
}

template <class Real>
inline const Vec3<Real> lerp(const Vec3<Real>& a, const Vec3<Real>& b, Real t)
{
	assert(t >= (Real)0.0 && t <= (Real)1.0);
	return a * ((Real)1.0 - t) + b * t;
}


template <class Real>
inline const std::string toString(const Vec3<Real>& v)
{
	return v.toString();
}


inline const Vec3<float> toVec3f(const Vec3<double>& v)
{
	return Vec3<float>((float)v.x, (float)v.y, (float)v.z);
}
inline const Vec3<double> toVec3d(const Vec3<float>& v)
{
	return Vec3<double>((double)v.x, (double)v.y, (double)v.z);
}


typedef Vec3<float> Vec3f;
typedef Vec3<double> Vec3d;


#endif //__VEC3_H__
