/*=====================================================================
Matrix2.h
---------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "vec2.h"
#include "../utils/OutStream.h"
#include "../utils/InStream.h"


/*=====================================================================
Matrix2
-------
2x2 matrix class.
=====================================================================*/
template <class T>
class Matrix2
{
public:
	/*=====================================================================
	Matrix2
	-------
	
	=====================================================================*/
	inline Matrix2();
	inline Matrix2(T a, T b, T c, T d);

	inline static const Matrix2 identity();

	inline bool operator == (const Matrix2& rhs) const;

	inline Vec2<T> operator * (const Vec2<T>& rhs) const;

	inline const Matrix2 operator * (const Matrix2& rhs) const;

	inline const Matrix2 inverse() const;
	inline void invert();

	inline const Matrix2 transpose() const;

	inline T determinant() const;


	//A matrix which will rotate a vector counterclockwise
	inline static const Matrix2 rotationMatrix(T angle_rad);

	const std::string toString() const;
	const std::string toStringNSigFigs(int n) const;

	inline T& elem(unsigned int i, unsigned int j);
	inline T elem(unsigned int i, unsigned int j) const;


	T e[4];//entries in row-major order
	/*
	0 1

	2 3
	*/

};

template <class T>
Matrix2<T>::Matrix2()
{}

template <class T>
Matrix2<T>::Matrix2(T a, T b, T c, T d)
{
	e[0] = a;
	e[1] = b;
	e[2] = c;
	e[3] = d;
}


template <class T>
const Matrix2<T> Matrix2<T>::identity()
{
	return Matrix2<T>(1, 0, 0, 1);
}

template <class T>
bool Matrix2<T>::operator == (const Matrix2<T>& rhs) const
{
	return e[0] == rhs.e[0] &&
			e[1] == rhs.e[1] &&
			e[2] == rhs.e[2] &&
			e[3] == rhs.e[3];
}

template <class T>
T& Matrix2<T>::elem(unsigned int i, unsigned int j)
{
	assert(i < 2);
	assert(j < 2);
	return e[i*2 + j];
}
template <class T>
T Matrix2<T>::elem(unsigned int i, unsigned int j) const
{
	assert(i < 2);
	assert(j < 2);
	return e[i*2 + j];
}


template <class T>
inline Vec2<T> Matrix2<T>::operator * (const Vec2<T>& rhs) const
{
	return Vec2<T>(e[0]*rhs.x + e[1]*rhs.y, e[2]*rhs.x + e[3]*rhs.y);
}

template <class T>
inline const Matrix2<T> Matrix2<T>::inverse() const
{
	const T recip_det = (T)1.0 / determinant();

	return Matrix2(e[3]*recip_det, -e[1]*recip_det, -e[2]*recip_det, e[0]*recip_det);
}

template <class T>
inline void Matrix2<T>::invert()
{
	*this = inverse();
}


template <class T>
inline const Matrix2<T> Matrix2<T>::transpose() const
{
	return Matrix2(e[0], e[2], e[1], e[3]);
}


template <class T>
inline T Matrix2<T>::determinant() const
{
	return e[0]*e[3] - e[1]*e[2];
}


	//A matrix which will rotate a vector counterclockwise
template <class T>
inline const Matrix2<T> Matrix2<T>::rotationMatrix(T theta_rad)
{
	const T cos_theta = cos(theta_rad);
	const T sin_theta = sin(theta_rad);

	return Matrix2(cos_theta, -sin_theta, sin_theta, cos_theta);
}


template <class T>
const Matrix2<T> Matrix2<T>::operator * (const Matrix2& rhs) const
{
	//NOTE: checkme

	/*const T r0 = dotProduct(getRow0(), rhs.getColumn0());
	const T r1 = dotProduct(getRow0(), rhs.getColumn1());
	const T r2 = dotProduct(getRow1(), rhs.getColumn0());
	const T r3 = dotProduct(getRow1(), rhs.getColumn1());*/

	return Matrix2(
		e[0]*rhs.e[0] + e[1]*rhs.e[2], 
		e[0]*rhs.e[1] + e[1]*rhs.e[3], 
		e[2]*rhs.e[0] + e[3]*rhs.e[2], 
		e[2]*rhs.e[1] + e[3]*rhs.e[3]
		);
}


template <class T>
inline Matrix2<T> operator * (const Matrix2<T>& m, T factor)
{
	return Matrix2<T>(m.e[0]*factor, m.e[1]*factor, m.e[2]*factor, m.e[3]*factor);
}


template <class T>
inline Matrix2<T> operator + (const Matrix2<T>& a, const Matrix2<T>& b)
{
	return Matrix2<T>(a.e[0] + b.e[0], a.e[1] + b.e[1], a.e[2] + b.e[2], a.e[3] + b.e[3]);
}


template <class T>
inline bool epsMatrixEqual(const Matrix2<T>& a, const Matrix2<T>& b, T eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<4; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}


template <class T>
inline const std::string toString(const Matrix2<T>& x)
{
	return x.toString();
}


template <class T>
inline void writeToStream(const Matrix2<T>& m, OutStream& stream)
{
	stream.writeData(m.e, sizeof(T) * 4);
}


template <class T>
inline Matrix2<T> readMatrix2FromStream(InStream& stream)
{
	Matrix2<T> m;
	stream.readData(m.e, sizeof(T) * 4);
	return m;
}


typedef Matrix2<float> Matrix2f;
typedef Matrix2<double> Matrix2d;
