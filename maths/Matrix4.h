/*=====================================================================
Matrix4.h
---------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "Vec4.h"
#include "SSE.h"
#include <string>


/*=====================================================================
Matrix4
-------
Basic templated 4x4 Matrix class.

When dealing with floats, prefer to use the more optimised Matrix4f.

Not thoroughly tested or optimised.
=====================================================================*/
template <class T>
class Matrix4
{
public:
	inline Matrix4() {}

	explicit inline Matrix4(const T* t)
	{
		for(int i=0; i<16; ++i)
			e[i] = t[i];
	}

	inline Matrix4(const Matrix4& rhs)
	{
		for(int i=0; i<16; ++i)
			e[i] = rhs.e[i];
	}

	inline Matrix4(const Vec4<T>& row0, const Vec4<T>& row1, const Vec4<T>& row2, const Vec4<T>& row3)
	{
		for(int i=0; i<4; ++i)
			e[i] = row0[i];
		for(int i=0; i<4; ++i)
			e[4 + i] = row1[i];
		for(int i=0; i<4; ++i)
			e[8 + i] = row2[i];
		for(int i=0; i<4; ++i)
			e[12 + i] = row3[i];
	}

	inline Matrix4& operator = (const Matrix4& rhs)
	{
		for(int i=0; i<16; ++i)
			e[i] = rhs.e[i];
	}

	inline bool operator == (const Matrix4& rhs) const
	{
		for(int i=0; i<16; ++i)
			if(e[i] != rhs.e[i])
				return false;
		return true;
	}

	inline const Matrix4 operator + (const Matrix4& rhs) const
	{
		Matrix4 res;
		for(int i=0; i<16; ++i)
			res.e[i] = e[i] + rhs.e[i];
		return res;
	}

	inline const Matrix4 operator * (const Matrix4& rhs) const
	{
		Matrix4 res;
		for(int dy=0; dy<4; ++dy)
			for(int dx=0; dx<4; ++dx)
			{
				T sum(0);
				for(int j=0; j<4; ++j)
					sum += e[dy*4 + j] * rhs.e[j*4 + dx];
				res.e[dy*4 + dx] = sum;
			}
		return res;
	}

	inline Vec4<T> operator * (const Vec4<T>& rhs) const
	{
		Vec4<T> res;
		for(int dy=0; dy<4; ++dy)
		{
			T sum(0);
			for(int j=0; j<4; ++j)
				sum += e[dy*4 + j] * rhs[j];
			res[dy] = sum;
		}
		return res;
	}

	inline const static Matrix4<T> identity()
	{
		const T data[16] = { 1, 0, 0, 0,    0, 1, 0, 0,     0, 0, 1, 0,    0, 0, 0, 1 };
		return Matrix4<T>(data);
	}

	inline const static Matrix4<T> zero()
	{
		const T data[16] = { 0, 0, 0, 0,    0, 0, 0, 0,     0, 0, 0, 0,    0, 0, 0, 0 };
		return Matrix4<T>(data);
	}

	/*
	0 1 2 3

	4 5 6 7

	8 9 10 11

	12 13 14 15
	*/
	T e[16];
};


void doMatrix4Tests();
