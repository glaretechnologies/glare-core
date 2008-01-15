/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __COLOUR3_H__
#define __COLOUR3_H__


#include "../maths/mathstypes.h"
#include "../maths/vec3.h"
#include <math.h>
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

	Colour3(const Colour3& rhs)
	:	r(rhs.r),
		g(rhs.g),
		b(rhs.b)
	{}

	/*explicit Colour3(const Colour3<double>& rhs)
	:	r((Real)rhs.r),
		g((Real)rhs.g),
		b((Real)rhs.b)
	{}*/

	explicit Colour3(const Vec3<Real>& v)
	:	r(v.x),
		g(v.y),
		b(v.z)
	{}

	~Colour3(){}

	const Colour3<double> toColour3d() const { return Colour3<double>(r, g, b); }

	static Colour3 red()	{	return Colour3(1.0f ,0.0f ,0.0f); }
	static Colour3 green()	{	return Colour3(0.0f ,1.0f ,0.0f); }
	static Colour3 blue() 	{	return Colour3(0.0f ,0.0f ,1.0f); }
	static Colour3 grey()	{	return Colour3(0.5f ,0.5f ,0.5f); }
	static Colour3 yellow()	{	return Colour3(0.0f ,1.0f ,1.0f); }

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
		return Colour3(r / scale, g / scale, b / scale);
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
		r /= scale;
		g /= scale;
		b /= scale;

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
		if(r < 0.0f)
			r = 0.0f;
		else if(r > 1.0f)
			r = 1.0f;

		if(g < 0.0f)
			g = 0.0f;
		else if(g > 1.0f)
			g = 1.0f;

		if(b < 0.0f)
			b = 0.0f;
		else if(b > 1.0f)
			b = 1.0f;
	}

	inline void positiveClipComponents(Real threshold = 1.0f)
	{
		/*if(r > threshold)
			r = threshold;

		if(g > threshold)
			g = threshold;

		if(b > threshold)
			b = threshold;*/
		r = myMin(r, threshold);
		g = myMin(g, threshold);
		b = myMin(b, threshold);
	}

	inline bool isZero() const
	{
		return r == 0.0f && g == 0.0f && b == 0.0f;
	}

	//assuming in linear sRGB space
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

	/*static inline const Colour3 randomColour3()
	{
		return Colour(Random::unit(), Random::unit(), Random::unit());
	}*/

	const Real* toFloatArray() const { return (const Real*)this; }
	const Real* data() const { return (const Real*)this; }
	
	const Real length() const { return (Real)sqrt(r*r + g*g + b*b); }

	Real& operator [] (int index)
	{
		assert(index >= 0 && index < 3);
		return *(&r + index);
	}
	
	Real operator [] (int index) const
	{
		assert(index >= 0 && index < 3);
		return *(&r + index);
	}

	inline bool isFinite() const
	{
		return ::isFinite(r) && ::isFinite(g) && ::isFinite(b);
	}

	inline void clamp(Real lowerbound, Real upperbound)
	{
		r = myClamp(r, lowerbound, upperbound);
		g = myClamp(g, lowerbound, upperbound);
		b = myClamp(b, lowerbound, upperbound);
	}

	inline void lowerClamp(Real lowerbound)
	{
		r = myMax(r, lowerbound);
		g = myMax(g, lowerbound);
		b = myMax(b, lowerbound);
	}

	inline const Vec3<Real> toVec3() const
	{
		return Vec3<Real>(r, g, b);
	}

	Real r,g,b;
};

typedef Colour3<float> Colour3f;
typedef Colour3<double> Colour3d;


//}//end namespace RayT

#endif //__COLOUR3_H__
