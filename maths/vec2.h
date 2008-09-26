/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __Vec2_H__
#define __Vec2_H__



/*=================================================================
2 component vector class
------------------------
Coded by NIck Chapman in the year 2000
=================================================================*/

#include <math.h>
#include <string>
//#ifdef CYBERSPACE
//#include "../networking/mystream.h"
////#endif
#include "mathstypes.h"



template <class Real>
class Vec2
{
public:

	inline Vec2()
	{}

	inline ~Vec2()
	{}

	inline Vec2(Real x_, Real y_)
	:	x(x_),
		y(y_)
	{}

	inline Vec2(const Vec2& rhs)
	:	x(rhs.x),
		y(rhs.y)
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

	inline Vec2& operator = (const Vec2& rhs)
	{	
		x = rhs.x;
		y = rhs.y;
		return *this;
	}

	inline bool operator == (const Vec2& rhs) const
	{
		return (x == rhs.x) && (y == rhs.y);
	}

		//for sorting Vec2's
	inline bool operator < (const Vec2& rhs) const
	{
		if(x < rhs.x)
			return true;
		else if(x > rhs.x)
			return false;
		else	//else if x == rhs.x
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
		//if(!x && !y)
		//	return;

		const Real inverselength = Real(1.0) / length();

		x *= inverselength;
		y *= inverselength;
	}

	inline Real normalise_ret_length()
	{
		//if(!x && !y)
		//	return 0.0f;

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

	static const Vec2 randomVec(Real component_lowbound, Real component_highbound);

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

	// returns true if all components c satisfy c >= minval && c < maxval, i.e. c e [minval, maxval)
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


/*inline const Vec2 crossProduct(const Vec2& v1, const Vec2& v2)
{
	return Vec2(
	(v1.y * v2.z) - (v1.z * v2.y),
	(v1.z * v2.x) - (v1.x * v2.z),
	(v1.x * v2.y) - (v1.y * v2.x)z
	);	//NOTE: check me

}*/

	//v1 and v2 unnormalized
/*inline float angleBetween(Vec2& v1, Vec2& v2)
{
	float lf = v1.length() * v2.length();

	if(!lf)
		return PI_OVER_2; //90 //Pi/2 = 1.57079632679489661923

	float dp = dotProduct(v1, v2);

	return acos( dp / lf);
}*/

template <class Real>
inline Real angleBetweenNormalized(const Vec2<Real>& v1, const Vec2<Real>& v2)
{
	const Real dp = dotProduct(v1, v2);

	return (Real)acos(dp);
}

template <class Real>
inline const Vec2<Real> normalise(const Vec2<Real>& v)
{
	//const Real vlen = v.length();

	//if(!vlen)
	//	return Vec2<Real>(1.0f, 0.0f);

	//return v * (1.0f / vlen);

	return v / v.length();
}

template <class Real>
inline bool epsEqual(const Vec2<Real>& a, const Vec2<Real>& b)
{
	return ::epsEqual(a.x, b.x) && ::epsEqual(a.y, b.y);
}


//#ifdef CYBERSPACE
/*
inline MyStream& operator << (MyStream& stream, const Vec2& point)
{
	stream << point.x;
	stream << point.y;	

	return stream;
}

inline MyStream& operator >> (MyStream& stream, Vec2& point)
{
	stream >> point.x;
	stream >> point.y;	

	return stream;
}*/

//#endif//CYBERSPACE


inline const Vec2<float> toVec2f(const Vec2<double>& v)
{
	return Vec2<float>((float)v.x, (float)v.y);
}
inline const Vec2<double> toVec2d(const Vec2<float>& v)
{
	return Vec2<double>((double)v.x, (double)v.y);
}

typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;


#endif //__Vec2_H__
