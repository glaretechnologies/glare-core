/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __COLOUR3D_H__
#define __COLOUR3D_H__

NOTUSED

#include "../maths/mathstypes.h"
#include "../maths/vec3.h"
#include <math.h>
#include <assert.h>

class Colour3d
{
public:
	Colour3d(){}

	explicit Colour3d(double x)
	:	r(x), g(x), b(x)
	{}

	Colour3d(double r_, double g_, double b_)
	:	r(r_), g(g_), b(b_) 
	{}

	Colour3d(const Colour3d& rhs)
	:	r(rhs.r),
		g(rhs.g),
		b(rhs.b)
	{}


	explicit Colour3d(const Vec3& v)
	:	r(v.x),
		g(v.y),
		b(v.z)
	{}

	~Colour3d(){}

	inline void set(double r_, double g_, double b_)
	{
		r = r_;
		g = g_;
		b = b_;
	}

	inline Colour3d operator * (double scale) const
	{
		return Colour3d(r * scale, g * scale, b * scale);
	}

	inline Colour3d operator / (double scale) const
	{
		return Colour3d(r / scale, g / scale, b / scale);
	}

	inline Colour3d& operator *= (double scale)
	{
		r *= scale;
		g *= scale;
		b *= scale;

		return *this;
	}

	inline Colour3d& operator /= (double scale)
	{
		r /= scale;
		g /= scale;
		b /= scale;

		return *this;
	}

	inline Colour3d operator + (const Colour3d& rhs) const
	{
		return Colour3d(r + rhs.r, g + rhs.g, b + rhs.b);
	}
	
	inline Colour3d operator - (const Colour3d& rhs) const
	{
		return Colour3d(r - rhs.r, g - rhs.g, b - rhs.b);
	}

	inline void operator = (const Colour3d& rhs)
	{
		r = rhs.r;
		g = rhs.g;
		b = rhs.b;
	}

	inline bool operator == (const Colour3d& rhs) const
	{
		return r == rhs.r && g == rhs.g && b == rhs.b;
	}

	inline bool operator != (const Colour3d& rhs) const
	{
		return r != rhs.r || g != rhs.g || b != rhs.b;
	}

	inline void operator += (const Colour3d& rhs)
	{
		r += rhs.r;
		g += rhs.g;
		b += rhs.b;
	}

	inline Colour3d& operator *= (const Colour3d& rhs)
	{
		r *= rhs.r;
		g *= rhs.g;
		b *= rhs.b;
	
		return *this;
	}

	inline const Colour3d operator * (const Colour3d& rhs) const
	{
		return Colour3d(r * rhs.r, g * rhs.g, b * rhs.b);
	}

	inline void setToMult(const Colour3d& other, double factor)
	{
		r = other.r * factor;
		g = other.g * factor;
		b = other.b * factor;
	}

	inline void addMult(const Colour3d& other, double factor)
	{
		r += other.r * factor;
		g += other.g * factor;
		b += other.b * factor;
	}

	inline void add(const Colour3d& other)
	{
		r += other.r;
		g += other.g;
		b += other.b;
	}

	inline void clipComponents()
	{
		if(r < 0.0)
			r = 0.0;
		else if(r > 1.0)
			r = 1.0;

		if(g < 0.0)
			g = 0.0;
		else if(g > 1.0)
			g = 1.0;

		if(b < 0.0)
			b = 0.0;
		else if(b > 1.0)
			b = 1.0;
	}

	inline void positiveClipComponents(double threshold = 1.0)
	{
		if(r > threshold)
			r = threshold;

		if(g > threshold)
			g = threshold;

		if(b > threshold)
			b = threshold;
	}

	inline bool isZero() const
	{
		return r == 0.0 && g == 0.0 && b == 0.0;
	}

	inline double luminance() const
	{
		return 0.3*r + 0.587*g + 0.114*b;//approx
	}

	/*static inline const Colour3d randomColour3d()
	{
		return Colour(Random::unit(), Random::unit(), Random::unit());
	}*/

	const double* todoubleArray() const { return (const double*)this; }
	const double* data() const { return (const double*)this; }
	
	const double length() const { return sqrt(r*r + g*g + b*b); }

	double& operator [] (int index)
	{
		assert(index >= 0 && index < 3);
		return *(&r + index);
	}
	
	double operator [] (int index) const
	{
		assert(index >= 0 && index < 3);
		return *(&r + index);
	}

	inline bool isFinite() const
	{
		return ::isFinite(r) && ::isFinite(g) && ::isFinite(b);
	}

	inline void clamp(double lowerbound, double upperbound)
	{
		r = myClamp(r, lowerbound, upperbound);
		g = myClamp(g, lowerbound, upperbound);
		b = myClamp(b, lowerbound, upperbound);
	}

	inline void lowerClamp(double lowerbound)
	{
		r = myMax(r, lowerbound);
		g = myMax(g, lowerbound);
		b = myMax(b, lowerbound);
	}

	/*inline const Vec3 toVec3() const
	{
		return Vec3(r, g, b);
	}*/

	double r,g,b;
};



#endif //__Colour3dD_H__
