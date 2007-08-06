/*=====================================================================
Matrix2.h
---------
File created by ClassTemplate on Sun Mar 05 17:55:13 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MATRIX2_H_666_
#define __MATRIX2_H_666_

#include "../maths/vec2.h"



/*=====================================================================
Matrix2
-------

=====================================================================*/
template <class Real>
class Matrix2
{
public:
	/*=====================================================================
	Matrix2
	-------
	
	=====================================================================*/
	inline Matrix2();
	inline Matrix2(Real a, Real b, Real c, Real d);

	inline ~Matrix2();




	inline Vec2<Real> operator * (const Vec2<Real>& rhs) const;

	inline const Matrix2 inverse() const;
	inline void invert();

	inline Real determinant() const;


	//A matrix which will rotate a vector counterclockwise
	inline static const Matrix2 rotationMatrix(Real angle_rad);


	inline Real& elem(unsigned int i, unsigned int j);
	inline Real elem(unsigned int i, unsigned int j) const;


	Real e[4];//entries in row-major order
	/*
	0 1

	2 3
	*/

};

template <class Real>
Matrix2<Real>::Matrix2()
{}

template <class Real>
Matrix2<Real>::Matrix2(Real a, Real b, Real c, Real d)
{
	e[0] = a;
	e[1] = b;
	e[2] = c;
	e[3] = d;
}

template <class Real>
Matrix2<Real>::~Matrix2()
{}


template <class Real>
Real& Matrix2<Real>::elem(unsigned int i, unsigned int j)
{
	assert(i < 2);
	assert(j < 2);
	return e[i*2 + j];
}
template <class Real>
Real Matrix2<Real>::elem(unsigned int i, unsigned int j) const
{
	assert(i < 2);
	assert(j < 2);
	return e[i*2 + j];
}


template <class Real>
inline Vec2<Real> Matrix2<Real>::operator * (const Vec2<Real>& rhs) const
{
	return Vec2<Real>(e[0]*rhs.x + e[1]*rhs.y, e[2]*rhs.x + e[3]*rhs.y);
}

template <class Real>
inline const Matrix2<Real> Matrix2<Real>::inverse() const
{
	const Real recip_det = (Real)1.0 / determinant();

	return Matrix2(e[3]*recip_det, -e[1]*recip_det, -e[2]*recip_det, e[0]*recip_det);
}

template <class Real>
inline void Matrix2<Real>::invert()
{
	*this = inverse();
}

template <class Real>
inline Real Matrix2<Real>::determinant() const
{
	return e[0]*e[3] - e[1]*e[2];
}


	//A matrix which will rotate a vector counterclockwise
template <class Real>
inline const Matrix2<Real> Matrix2<Real>::rotationMatrix(Real theta_rad)
{
	const Real cos_theta = cos(theta_rad);
	const Real sin_theta = sin(theta_rad);

	return Matrix2(cos_theta, -sin_theta, sin_theta, cos_theta);
}


typedef Matrix2<float> Matrix2f;
typedef Matrix2<double> Matrix2d;


#endif //__MATRIX2_H_666_






























