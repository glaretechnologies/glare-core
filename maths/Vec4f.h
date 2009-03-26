/*=====================================================================
Vec4.h
------
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __VEC4_H_666_
#define __VEC4_H_666_


#include "sse.h"
#include "mathstypes.h"
#include <assert.h>
#include <string>


/*=====================================================================
Vec4
----

=====================================================================*/
class Vec4f
{
public:
	Vec4f() {}
	Vec4f(float x_, float y_, float z_, float w_) { x[0] = x_; x[1] = y_; x[2] = z_; x[3] = w_; }
	explicit Vec4f(__m128 v_) : v(v_) {}

	~Vec4f() {}


	inline void operator += (const Vec4f& a);
	inline void operator -= (const Vec4f& a);
	const std::string toString() const;


	union
	{
		float x[4];
		__m128 v;
	};
	//float x[4];
};


/*template <class Real> const Vec4<Real> operator + (const Vec4<Real>& a, const Vec4<Real> b);

template <> const Vec4<float> operator + (const Vec4<float>& a, const Vec4<float> b)
{
}*/


inline const Vec4f operator + (const Vec4f& a, const Vec4f& b)
{
	return Vec4f(_mm_add_ps(a.v, b.v));
}


inline const Vec4f operator - (const Vec4f& a, const Vec4f& b)
{
	return Vec4f(_mm_sub_ps(a.v, b.v));
}


void Vec4f::operator += (const Vec4f& a)
{
	v = _mm_add_ps(v, a.v);
}


void Vec4f::operator -= (const Vec4f& a)
{
	v = _mm_add_ps(v, a.v);
}



//typedef Vec4<float> Vec4f;
//typedef Vec4<double> Vec4d;


#endif //__VEC4_H_666_
