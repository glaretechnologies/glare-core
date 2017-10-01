/*=====================================================================
matrix3.h
---------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "vec3.h"


/*=====================================================================
Matrix3
-------
3x3 matrix class.
=====================================================================*/
template <class Real>
class Matrix3
{
public:
	inline Matrix3(){}
	inline Matrix3(const Matrix3& rhs);
	inline Matrix3(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2);
	inline Matrix3(const Real* entries);

	static const Matrix3 buildMatrixFromRows(const Vec3<Real>& row0, 
							const Vec3<Real>& row1, const Vec3<Real>& row2);


	inline void set(const Matrix3& rhs);
	inline void set(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2);
	inline void set(const Real* newentries);

	inline void init(){ *this = identity(); }

	inline Matrix3& operator = (const Matrix3& rhs);
	inline bool operator == (const Matrix3& rhs) const;


	inline Matrix3& operator += (const Matrix3& rhs);
	inline Matrix3& operator -= (const Matrix3& rhs);
	inline Matrix3& operator *= (const Matrix3& rhs);

	inline const Matrix3 operator + (const Matrix3& rhs) const;
	inline const Matrix3 operator - (const Matrix3& rhs) const;
	//return value = this * rhs;
	inline const Matrix3 operator * (const Matrix3& rhs) const;



	//return value = this * rhs;
	inline Vec3<Real> operator * (const Vec3<Real>& rhs) const;
	inline Vec3<Real> transposeMult(const Vec3<Real>& rhs) const;


	static const Matrix3 rotationMatrix(const Vec3<Real>& unit_axis, Real angle);


	inline const Matrix3 getTranspose() const;
	inline void transpose();

	inline Vec3<Real> getXUnitVector() const;
	inline Vec3<Real> getYUnitVector() const;
	inline Vec3<Real> getZUnitVector() const;

	inline Vec3<Real> getColumn0() const;// == GetXUnitVector
	inline Vec3<Real> getColumn1() const;// == GetYUnitVector
	inline Vec3<Real> getColumn2() const;// == GetZUnitVector

	inline void setColumn0(const Vec3<Real>& v);
	inline void setColumn1(const Vec3<Real>& v);
	inline void setColumn2(const Vec3<Real>& v);

	inline Vec3<Real> getRow0() const;
	inline Vec3<Real> getRow1() const;
	inline Vec3<Real> getRow2() const;

	void print() const;

	inline const static Matrix3<Real> identity();
	inline bool isIdentity() const;
	inline void scale(Real factor);
	inline Real determinant() const;
	inline bool inverse(Matrix3& inverse_out) const;
	inline bool inverse(Matrix3& inverse_out, float& det_out) const;
	inline bool invert();

	inline Real elem(unsigned int i, unsigned int j) const;

	const std::string toString() const;
	const std::string toStringPlain() const;

	inline void constructFromVector(const Vec3<Real>& vec);

	bool polarDecomposition(Matrix3& rot_out, Matrix3& rest_out) const;

	void rotationMatrixToAxisAngle(Vec3<Real>& unit_axis_out, Real& angle_out) const;

	static void test();


	/*
	0 1 2

	3 4 5

	6 7 8
	*/
	Real e[9];
};




template <class Real>
Matrix3<Real>::Matrix3(const Matrix3<Real>& rhs)
{
	set(rhs);
}

template <class Real>
Matrix3<Real>::Matrix3(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2)
{
	set(column0, column1, column2);
}


template <class Real>
Matrix3<Real>::Matrix3(const Real* entries)
{
	set(entries);
}


template <class Real>
Real Matrix3<Real>::elem(unsigned int i, unsigned int j) const
{
	assert(i < 3);
	assert(j < 3);
	return e[i*3 + j];
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::identity()
{
	return Matrix3<Real>(Vec3<Real>(1,0,0), Vec3<Real>(0,1,0), Vec3<Real>(0,0,1));
}


template <class Real>
void Matrix3<Real>::set(const Matrix3& rhs)
{
	//NOTE: which is faster, this or a for loop?
	e[0] = rhs.e[0];
	e[1] = rhs.e[1];
	e[2] = rhs.e[2];
	e[3] = rhs.e[3];
	e[4] = rhs.e[4];
	e[5] = rhs.e[5];
	e[6] = rhs.e[6];
	e[7] = rhs.e[7];
	e[8] = rhs.e[8];
}


template <class Real>
void Matrix3<Real>::set(const Vec3<Real>& column0, const Vec3<Real>& column1, const Vec3<Real>& column2)
{
	e[0] = column0.x;
	e[3] = column0.y;
	e[6] = column0.z;

	e[1] = column1.x;
	e[4] = column1.y;
	e[7] = column1.z;

	e[2] = column2.x;
	e[5] = column2.y;
	e[8] = column2.z;
}


template <class Real>
void Matrix3<Real>::set(const Real* newentries)
{
	e[0] = newentries[0];
	e[1] = newentries[1];
	e[2] = newentries[2];
	e[3] = newentries[3];
	e[4] = newentries[4];
	e[5] = newentries[5];
	e[6] = newentries[6];
	e[7] = newentries[7];
	e[8] = newentries[8];
}


template <class Real>
void Matrix3<Real>::scale(Real factor)
{
	for(int i=0; i<9; ++i)
		e[i] *= factor;
}


template <class Real>
Matrix3<Real>& Matrix3<Real>::operator = (const Matrix3& rhs)
{
	set(rhs);

	return *this;
}


template <class Real>
bool Matrix3<Real>::operator == (const Matrix3& rhs) const
{
	return (e[0] == rhs.e[0] &&
			e[1] == rhs.e[1] &&
			e[2] == rhs.e[2] &&
			e[3] == rhs.e[3] &&
			e[4] == rhs.e[4] &&
			e[5] == rhs.e[5] &&
			e[6] == rhs.e[6] &&
			e[7] == rhs.e[7] &&
			e[8] == rhs.e[8]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::getXUnitVector() const
{
	return Vec3<Real>(e[0], e[3], e[6]);
}
	

template <class Real>
Vec3<Real> Matrix3<Real>::getYUnitVector() const
{
	return Vec3<Real>(e[1], e[4], e[7]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::getZUnitVector() const
{
	return Vec3<Real>(e[2], e[5], e[8]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::getColumn0() const
{
	return getXUnitVector();
}


template <class Real>
Vec3<Real> Matrix3<Real>::getColumn1() const
{
	return getYUnitVector();
}


template <class Real>
Vec3<Real> Matrix3<Real>::getColumn2() const
{
	return getZUnitVector();
}


template <class Real>
void Matrix3<Real>::setColumn0(const Vec3<Real>& v)
{
	e[0] = v.x;
	e[3] = v.y;
	e[6] = v.z;
}


template <class Real>
void Matrix3<Real>::setColumn1(const Vec3<Real>& v)
{
	e[1] = v.x;
	e[4] = v.y;
	e[7] = v.z;
}


template <class Real>
void Matrix3<Real>::setColumn2(const Vec3<Real>& v)
{
	e[2] = v.x;
	e[5] = v.y;
	e[8] = v.z;
}


template <class Real>
Vec3<Real> Matrix3<Real>::getRow0() const
{
	return Vec3<Real>(e[0], e[1], e[2]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::getRow1() const
{
	return Vec3<Real>(e[3], e[4], e[5]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::getRow2() const
{
	return Vec3<Real>(e[6], e[7], e[8]);
}


template <class Real>
Vec3<Real> Matrix3<Real>::operator * (const Vec3<Real>& rhs) const
{
	return Vec3<Real>(
		rhs.x*e[0] + rhs.y*e[1] + rhs.z*e[2], 
		rhs.x*e[3] + rhs.y*e[4] + rhs.z*e[5],
		rhs.x*e[6] + rhs.y*e[7] + rhs.z*e[8]
	);
	//return transformVectorToLocal(rhs);
	//return Vec3<Real>(dot(getRow0(),rhs), dot(getRow1(), rhs), dot(getRow2(), rhs));
}


template <class Real>
Vec3<Real> Matrix3<Real>::transposeMult(const Vec3<Real>& rhs) const
{
	return Vec3<Real>(
		rhs.x*e[0] + rhs.y*e[3] + rhs.z*e[6], 
		rhs.x*e[1] + rhs.y*e[4] + rhs.z*e[7],
		rhs.x*e[2] + rhs.y*e[5] + rhs.z*e[8]
	);
}


//NOTE: make me un-inline?
template <class Real>
const Matrix3<Real> Matrix3<Real>::operator * (const Matrix3& rhs) const
{
	//return Matrix3(e[0]*rhs.e[0] + e[1]*e[3] + e[2]*e[6],	e[0]*	

	//NOTE: checkme

	const Real r0 = dotProduct(getRow0(), rhs.getColumn0());
	const Real r1 = dotProduct(getRow0(), rhs.getColumn1());
	const Real r2 = dotProduct(getRow0(), rhs.getColumn2());
	const Real r3 = dotProduct(getRow1(), rhs.getColumn0());
	const Real r4 = dotProduct(getRow1(), rhs.getColumn1());
	const Real r5 = dotProduct(getRow1(), rhs.getColumn2());
	const Real r6 = dotProduct(getRow2(), rhs.getColumn0());
	const Real r7 = dotProduct(getRow2(), rhs.getColumn1());
	const Real r8 = dotProduct(getRow2(), rhs.getColumn2());
	
	return Matrix3(Vec3<Real>(r0, r3, r6), Vec3<Real>(r1, r4, r7), Vec3<Real>(r2, r5, r8));
}


template <class Real>
Matrix3<Real>& Matrix3<Real>::operator *= (const Matrix3& rhs)
{
	const Real r0 = dotProduct(getRow0(), rhs.getColumn0());
	const Real r1 = dotProduct(getRow0(), rhs.getColumn1());
	const Real r2 = dotProduct(getRow0(), rhs.getColumn2());
	const Real r3 = dotProduct(getRow1(), rhs.getColumn0());
	const Real r4 = dotProduct(getRow1(), rhs.getColumn1());
	const Real r5 = dotProduct(getRow1(), rhs.getColumn2());
	const Real r6 = dotProduct(getRow2(), rhs.getColumn0());
	const Real r7 = dotProduct(getRow2(), rhs.getColumn1());
	const Real r8 = dotProduct(getRow2(), rhs.getColumn2());

	e[0] = r0;
	e[1] = r1;
	e[2] = r2;
	e[3] = r3;
	e[4] = r4;
	e[5] = r5;
	e[6] = r6;
	e[7] = r7;
	e[8] = r8;

	return *this;
}


template <class Real>
Matrix3<Real>& Matrix3<Real>::operator += (const Matrix3& rhs)
{
	e[0] += rhs.e[0];
	e[1] += rhs.e[1];
	e[2] += rhs.e[2];
	e[3] += rhs.e[3];
	e[4] += rhs.e[4];
	e[5] += rhs.e[5];
	e[6] += rhs.e[6];
	e[7] += rhs.e[7];
	e[8] += rhs.e[8];

	return *this;
}


template <class Real>
Matrix3<Real>& Matrix3<Real>::operator -= (const Matrix3& rhs)
{
	e[0] -= rhs.e[0];
	e[1] -= rhs.e[1];
	e[2] -= rhs.e[2];
	e[3] -= rhs.e[3];
	e[4] -= rhs.e[4];
	e[5] -= rhs.e[5];
	e[6] -= rhs.e[6];
	e[7] -= rhs.e[7];
	e[8] -= rhs.e[8];

	return *this;
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::operator + (const Matrix3& rhs) const
{
	return Matrix3( getColumn0() + rhs.getColumn0(), 
					getColumn1() + rhs.getColumn1(),
					getColumn2() + rhs.getColumn2());
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::operator - (const Matrix3& rhs) const
{
	return Matrix3( getColumn0() - rhs.getColumn0(), 
					getColumn1() - rhs.getColumn1(),
					getColumn2() - rhs.getColumn2());
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::getTranspose() const
{
	//-----------------------------------------------------------------
	//make a new matrix using the rows as the new columns
	//-----------------------------------------------------------------
	return Matrix3( getRow0(), getRow1(), getRow2());
}


template <class Real>
void Matrix3<Real>::transpose()
{
	Matrix3 temp = getTranspose();
	set(temp);
}


template <class Real>
bool Matrix3<Real>::isIdentity() const//i.e. no rotation and at origin
{
	return getColumn0() == Vec3<Real>(1,0,0) && 
			getColumn1() == Vec3<Real>(0,1,0) &&
			getColumn2() == Vec3<Real>(0,0,1);
}


//assumes v is a row vector.
template <class Real>
inline const Vec3<Real> operator * (const Vec3<Real>& v, const Matrix3<Real>& m)
{
	return Vec3<Real>(dot(v, m.getColumn0()), dot(v, m.getColumn1()), dot(v, m.getColumn2())); 
}


template <class Real>
inline bool epsMatrixEqual(const Matrix3<Real>& a, const Matrix3<Real>& b, Real eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<9; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}


//see Anton page 83
template <class Real>
Real Matrix3<Real>::determinant() const
{
	//return elem(0,0)*elem(1,1)*elem(2,2) + elem(0,1)*elem(1,2)*elem(2,0) + elem(0,2)*elem(1,0)*elem(2,1)
	//	- elem(0,2)*elem(1,1)*elem(2,0) - elem(0,1)*elem(1,0)*elem(2,2)  - elem(0,0)*elem(1,2)*elem(2,1);
	return e[0]*e[4]*e[8] + e[1]*e[5]*e[6] + e[2]*e[3]*e[7]
	     - e[2]*e[4]*e[6] - e[1]*e[3]*e[8] - e[0]*e[5]*e[7];
}


template <class Real>
bool Matrix3<Real>::inverse(Matrix3& inverse_out) const
{
	const Real d = determinant();
	if(std::fabs(d) == 0.0f)//< 1.e-9)
		return false; // Singular matrix

	//inverse_out = adjoint();
	//inverse_out.scale(1 / d);
	// See http://mathworld.wolfram.com/MatrixInverse.html, figure 6.
	const Real recip_det = 1 / d;
	inverse_out.e[0] = (e[4]*e[8] - e[5]*e[7])*recip_det;
	inverse_out.e[1] = (e[2]*e[7] - e[1]*e[8])*recip_det;
	inverse_out.e[2] = (e[1]*e[5] - e[2]*e[4])*recip_det;
	inverse_out.e[3] = (e[5]*e[6] - e[3]*e[8])*recip_det;
	inverse_out.e[4] = (e[0]*e[8] - e[2]*e[6])*recip_det;
	inverse_out.e[5] = (e[2]*e[3] - e[0]*e[5])*recip_det;
	inverse_out.e[6] = (e[3]*e[7] - e[4]*e[6])*recip_det;
	inverse_out.e[7] = (e[1]*e[6] - e[0]*e[7])*recip_det;
	inverse_out.e[8] = (e[0]*e[4] - e[1]*e[3])*recip_det;

#ifdef DEBUG
	// Check our inverse is correct.
	Matrix3<Real> product = *this * inverse_out;
	assert(epsMatrixEqual(product, Matrix3<Real>::identity()));

	product = inverse_out * *this;
	assert(epsMatrixEqual(product, Matrix3<Real>::identity()));
#endif

	return true;
}


template <class Real>
bool Matrix3<Real>::inverse(Matrix3& inverse_out, float& det_out) const
{
	const Real d = determinant();
	det_out = d;
	if(std::fabs(d) == 0.0f)//< 1.e-9)
		return false; // Singular matrix

	// See http://mathworld.wolfram.com/MatrixInverse.html, figure 6.
	const float recip_det = 1 / d;
	inverse_out.e[0] = (e[4]*e[8] - e[5]*e[7])*recip_det;
	inverse_out.e[1] = (e[2]*e[7] - e[1]*e[8])*recip_det;
	inverse_out.e[2] = (e[1]*e[5] - e[2]*e[4])*recip_det;
	inverse_out.e[3] = (e[5]*e[6] - e[3]*e[8])*recip_det;
	inverse_out.e[4] = (e[0]*e[8] - e[2]*e[6])*recip_det;
	inverse_out.e[5] = (e[2]*e[3] - e[0]*e[5])*recip_det;
	inverse_out.e[6] = (e[3]*e[7] - e[4]*e[6])*recip_det;
	inverse_out.e[7] = (e[1]*e[6] - e[0]*e[7])*recip_det;
	inverse_out.e[8] = (e[0]*e[4] - e[1]*e[3])*recip_det;

#ifndef NDEBUG
	// Check our inverse is correct.
	Matrix3<Real> product = *this * inverse_out;
	assert(epsMatrixEqual(product, Matrix3<Real>::identity()));

	product = inverse_out * *this;
	assert(epsMatrixEqual(product, Matrix3<Real>::identity()));
#endif

	return true;
}


template <class Real>
bool Matrix3<Real>::invert()
{
	Matrix3 inv;
	const bool invertible = inverse(inv);
	if(invertible)
		*this = inv;
	return invertible;
}


template <class Real>
const Matrix3<Real> Matrix3<Real>::rotationMatrix(const Vec3<Real>& unit_axis, Real angle)
{
	assert(unit_axis.isUnitLength());

	//-----------------------------------------------------------------
	//build the rotation matrix
	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	const Real a = unit_axis.x;
	const Real b = unit_axis.y;
	const Real c = unit_axis.z;

	const Real cost = std::cos(angle);
	const Real sint = std::sin(angle);

	const Real asint = a*sint;
	const Real bsint = b*sint;
	const Real csint = c*sint;

	const Real one_minus_cost = (Real)1.0 - cost;

	return Matrix3<Real>(
		Vec3<Real>(a*a*one_minus_cost + cost, a*b*one_minus_cost + csint, a*c*one_minus_cost - bsint), // column 0
		Vec3<Real>(a*b*one_minus_cost - csint, b*b*one_minus_cost + cost, b*c*one_minus_cost + asint), // column 1
		Vec3<Real>(a*c*one_minus_cost + bsint, b*c*one_minus_cost - asint, c*c*one_minus_cost + cost) // column 2
		);
}


template <class Real>
inline const std::string toString(const Matrix3<Real>& x)
{
	return x.toString();
}


template <class Real>
void Matrix3<Real>::constructFromVector(const Vec3<Real>& vec)
{
	assert(::epsEqual(vec.length(), (Real)1.0));

	Vec3<Real> v2;//x axis

	//thanks to Pharr and Humprehys for this code
	if(fabs(vec.x) > fabs(vec.y))
	{
		const Real recip_len = (Real)1.0 / sqrt(vec.x * vec.x + vec.z * vec.z);

		v2.set(-vec.z * recip_len, 0.0f, vec.x * recip_len);
	}
	else
	{
		const Real recip_len = (Real)1.0 / sqrt(vec.y * vec.y + vec.z * vec.z);

		v2.set(0.0, vec.z * recip_len, -vec.y * recip_len);
	}

	assert(::epsEqual(v2.length(), (Real)1.0));

	setColumn0(v2);
	setColumn1(::crossProduct(vec, v2));
	setColumn2(vec);
	
	assert(::epsEqual(dot(getColumn0(), getColumn1()), (Real)0.0));
	assert(::epsEqual(dot(getColumn0(), getColumn2()), (Real)0.0));
	assert(::epsEqual(dot(getColumn1(), getColumn2()), (Real)0.0));
}


typedef Matrix3<float> Matrix3f;
typedef Matrix3<double> Matrix3d;
