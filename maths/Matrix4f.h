/*=====================================================================
Matrix4f.h
----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "Vec4f.h"
#include "SSE.h"
#include <string>
// Template forwards declarations
template <class Real> class Matrix3;
template <class Real> class Vec3;


/*=====================================================================
Matrix4f
--------
4x4 Matrix class with single-precision float elements.
Optimised for multiplications with Vec4fs.
=====================================================================*/
SSE_CLASS_ALIGN Matrix4f
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	inline Matrix4f() {}
	inline Matrix4f(const float* data);
	inline Matrix4f(const Matrix4f& other);

	Matrix4f(const Matrix3<float>& upper_left_mat, const Vec3<float>& translation);
	inline Matrix4f(const Vec4f& col0, const Vec4f& col1, const Vec4f& col2, const Vec4f& col3);

	inline Matrix4f& operator = (const Matrix4f& other);

	//void setToUpperLeftAndTranslation(const Matrix3<float>& upper_left_mat, const Vec3<float>& translation);

	inline void setToTranslationMatrix(float x, float y, float z);
	inline static const Matrix4f translationMatrix(float x, float y, float z);
	inline static const Matrix4f translationMatrix(const Vec4f& v);
	
	void setToRotationMatrix(const Vec4f& unit_axis, float angle);
	static const Matrix4f rotationMatrix(const Vec4f& unit_axis, float angle);
	static const Matrix4f rotationAroundXAxis(float angle);
	static const Matrix4f rotationAroundYAxis(float angle);
	static const Matrix4f rotationAroundZAxis(float angle);

	void applyUniformScale(float scale); // right multiply the matrix by a uniform scale matrix.
	void setToUniformScaleMatrix(float scale); // Make the upper-left 3x3 matrix a uniform scaling matrix.
	static const Matrix4f uniformScaleMatrix(float scale);

	void setToScaleMatrix(float xscale, float yscale, float zscale);
	static const Matrix4f scaleMatrix(float xscale, float yscale, float zscale);
	
	

	inline const Vec4f mul3Vector(const Vec4f& v) const;
	inline const Vec4f mul3Point(const Vec4f& v) const;
	inline const Vec4f transposeMult(const Vec4f& v) const;

	// Ignores W component of vector, returns vector with W component of 0.
	inline const Vec4f transposeMult3Vector(const Vec4f& v) const;

	inline void getTranspose(Matrix4f& transpose_out) const;

	void getUpperLeftMatrix(Matrix3<float>& upper_left_mat_out) const;
	void setUpperLeftMatrix(const Matrix3<float>& upper_left_mat);


	inline void constructFromVector(const Vec4f& vec);
	static inline const Vec4f constructFromVectorAndMul(const Vec4f& vec, const Vec4f& other_v);

	inline bool operator == (const Matrix4f& a) const;

	const Matrix4f operator * (const Matrix4f& rhs) const;


	inline float elem(unsigned int row_index, unsigned int column_index) const { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }
	inline float& elem(unsigned int row_index, unsigned int column_index) { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }


	// Get column (col_index is a zero-based index)
	inline const Vec4f getColumn(unsigned int col_index) const;
	inline const Vec4f getRow(unsigned int row_index) const;

	inline void setColumn(unsigned int col_index, const Vec4f& v);
	inline void setRow(unsigned int row_index, const Vec4f& v);


	// Is A the inverse of B?
	static bool isInverse(const Matrix4f& A, const Matrix4f& B);

	// Asumming that this matrix is the concatenation of a 3x3 rotation/scale/shear matrix and a translation matrix, return the inverse.
	bool getInverseForAffine3Matrix(Matrix4f& inverse_out) const;
	bool getUpperLeftInverseTranspose(Matrix4f& inverse_trans_out) const;

	inline float upperLeftDeterminant() const;

	void setToIdentity();
	static const Matrix4f identity();

	const std::string rowString(int row_index) const;
	const std::string toString() const;

	static void test();

	/*
	Elements are laid out in the following way. (column-major)
	This layout is for fast matrix-vector SSE multiplication.

	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	float e[16];
};


Matrix4f::Matrix4f(const float* data)
{
	_mm_store_ps(e +  0, _mm_loadu_ps(data +  0));
	_mm_store_ps(e +  4, _mm_loadu_ps(data +  4));
	_mm_store_ps(e +  8, _mm_loadu_ps(data +  8));
	_mm_store_ps(e + 12, _mm_loadu_ps(data + 12));
}


Matrix4f::Matrix4f(const Matrix4f& other)
{
	_mm_store_ps(e +  0, _mm_load_ps(other.e +  0));
	_mm_store_ps(e +  4, _mm_load_ps(other.e +  4));
	_mm_store_ps(e +  8, _mm_load_ps(other.e +  8));
	_mm_store_ps(e + 12, _mm_load_ps(other.e + 12));
}


Matrix4f::Matrix4f(const Vec4f& col0, const Vec4f& col1, const Vec4f& col2, const Vec4f& col3)
{
	_mm_store_ps(e + 0,  col0.v);
	_mm_store_ps(e + 4,  col1.v);
	_mm_store_ps(e + 8,  col2.v);
	_mm_store_ps(e + 12, col3.v);
}


Matrix4f& Matrix4f::operator = (const Matrix4f& other)
{
	_mm_store_ps(e +  0, _mm_load_ps(other.e +  0));
	_mm_store_ps(e +  4, _mm_load_ps(other.e +  4));
	_mm_store_ps(e +  8, _mm_load_ps(other.e +  8));
	_mm_store_ps(e + 12, _mm_load_ps(other.e + 12));
	return *this;
}


void Matrix4f::setToTranslationMatrix(float x, float y, float z)
{
	/*
	1	0	0	x
	0	1	0	y
	0	0	1	z
	0	0	0	1
	*/

	e[0] = 1.f;
	e[1] = 0.f;
	e[2] = 0.f;
	e[3] = 0.f;
	e[4] = 0.f;
	e[5] = 1.f;
	e[6] = 0.f;
	e[7] = 0.f;
	e[8] = 0.f;
	e[9] = 0.f;
	e[10] = 1.f;
	e[11] = 0.f;
	e[12] = x;
	e[13] = y;
	e[14] = z;
	e[15] = 1.f;
}


const Matrix4f Matrix4f::translationMatrix(float x, float y, float z)
{
	Matrix4f m;
	m.setToTranslationMatrix(x, y, z);
	return m;
}


const Matrix4f Matrix4f::translationMatrix(const Vec4f& v)
{
	Matrix4f m;
	m.setToTranslationMatrix(v[0], v[1], v[2]);
	return m;
}


const Vec4f Matrix4f::transposeMult(const Vec4f& v) const
{
	/*
	Current layout:
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15

	transpose:
	0	1	2	3
	4	5	6	7
	8	9	10	11
	12	13	14	15
	*/

	// A nice way of doing transpose multiplications, given here: http://www.virtualdub.org/blog/pivot/entry.php?id=150
	__m128 x = _mm_dp_ps(_mm_load_ps(e +  0), v.v, 0xF1); // Compute dot product with all 4 components, write result to x, set others to zero.
	__m128 y = _mm_dp_ps(_mm_load_ps(e +  4), v.v, 0xF2); // Compute dot product with all 4 components, write result to y, set others to zero.
	__m128 z = _mm_dp_ps(_mm_load_ps(e +  8), v.v, 0xF4); // Compute dot product with all 4 components, write result to z, set others to zero.
	__m128 w = _mm_dp_ps(_mm_load_ps(e + 12), v.v, 0xF8); // Compute dot product with all 4 components, write result to w, set others to zero.

	return _mm_or_ps(_mm_or_ps(x, y), _mm_or_ps(z, w));
}


// Ignores W component of vector, returns vector with W component of 0.
const Vec4f Matrix4f::transposeMult3Vector(const Vec4f& v) const
{
	__m128 x = _mm_dp_ps(_mm_load_ps(e +  0), v.v, 0x71); // Compute dot product with x, y, z components, write result to x, set others to zero.
	__m128 y = _mm_dp_ps(_mm_load_ps(e +  4), v.v, 0x72); // Compute dot product with x, y, z components, write result to y, set others to zero.
	__m128 z = _mm_dp_ps(_mm_load_ps(e +  8), v.v, 0x74); // Compute dot product with x, y, z components, write result to z, set others to zero.

	return _mm_or_ps(_mm_or_ps(x, y), z);
}


void Matrix4f::getTranspose(Matrix4f& transpose_out) const
{
	Vec4f c0, c1, c2, c3;
	transpose(getColumn(0), getColumn(1), getColumn(2), getColumn(3), c0, c1, c2, c3);

	transpose_out.setColumn(0, c0);
	transpose_out.setColumn(1, c1);
	transpose_out.setColumn(2, c2);
	transpose_out.setColumn(3, c3);
}


void mul(const Matrix4f& a, const Matrix4f& b, Matrix4f& result_out);


INDIGO_STRONG_INLINE const Vec4f operator * (const Matrix4f& m, const Vec4f& v)
{
	assert(SSE::isSSEAligned(&m));
	assert(SSE::isSSEAligned(&v));

	const __m128 vx = indigoCopyToAll(v.v, 0);
	const __m128 vy = indigoCopyToAll(v.v, 1);
	const __m128 vz = indigoCopyToAll(v.v, 2);
	const __m128 vw = indigoCopyToAll(v.v, 3);

	return Vec4f(
		_mm_add_ps(
			_mm_add_ps(
				_mm_mul_ps(
					vx,
					_mm_load_ps(m.e) // e[0]-e[3]
					),
				_mm_mul_ps(
					vy,
					_mm_load_ps(m.e + 4) // e[4]-e[7]
					)
				),
			_mm_add_ps(
				_mm_mul_ps(
					vz,
					_mm_load_ps(m.e + 8) // e[8]-e[11]
					),
				_mm_mul_ps(
					vw,
					_mm_load_ps(m.e + 12) // e[12]-e[15]
					)
				)
			)
		);
}


INDIGO_STRONG_INLINE const Vec4f Matrix4f::mul3Vector(const Vec4f& v) const
{
	const __m128 vx = indigoCopyToAll(v.v, 0);
	const __m128 vy = indigoCopyToAll(v.v, 1);
	const __m128 vz = indigoCopyToAll(v.v, 2);

	return Vec4f(
		_mm_add_ps(
			_mm_add_ps(
				_mm_mul_ps(
					vx,
					_mm_load_ps(e) // e[0]-e[3]
					),
				_mm_mul_ps(
					vy,
					_mm_load_ps(e + 4) // e[4]-e[7]
					)
				),
			_mm_mul_ps(
				vz,
				_mm_load_ps(e + 8) // e[8]-e[11]
			)
		)
	);
}


// Treats v.w as 1
INDIGO_STRONG_INLINE const Vec4f Matrix4f::mul3Point(const Vec4f& v) const
{
	const __m128 vx = indigoCopyToAll(v.v, 0);
	const __m128 vy = indigoCopyToAll(v.v, 1);
	const __m128 vz = indigoCopyToAll(v.v, 2);

	return Vec4f(
		_mm_add_ps(
			_mm_add_ps(
				_mm_mul_ps(vx, _mm_load_ps(e)),
				_mm_mul_ps(vy, _mm_load_ps(e + 4))
				),
			_mm_add_ps(
				_mm_mul_ps(vz, _mm_load_ps(e + 8)),
				 _mm_load_ps(e + 12) // Just add column 3.
			)
		)
	);
}


// Sets to the matrix that will transform a vector from an orthonormal basis with k = vec, to parent space.
void Matrix4f::constructFromVector(const Vec4f& vec)
{
	assert(SSE::isSSEAligned(this));
	assert(SSE::isSSEAligned(&vec));
	assertIsUnitLength(vec);
	assert(vec[3] == 0.f);

	Vec4f x_axis;

	// From PBR
	const Vec4f abs_vec = abs(vec);
	const Vec4f neg_v = -vec;
	if(abs_vec[0] > abs_vec[1])
	{
		
		const Vec4f len = sqrt(_mm_dp_ps(vec.v, vec.v, 0x5F));// Compute dot product with x,z components, write result to all components.
		x_axis = div(shuffle<2, 3, 0, 3>(neg_v, vec), len); // = (neg_v[2], neg_v[3], vec[0], vec[3]) / len = (neg_v[2], 0, vec[0], 0) / len
	}
	else
	{
		const Vec4f len = sqrt(_mm_dp_ps(vec.v, vec.v, 0x6F)); // Compute dot product with y,z components, write result to all components.
		x_axis = div(shuffle<3, 2, 1, 3>(vec, neg_v), len); // = (vec[3], vec[2], neg_v[1], neg_v[3]) / len = = (0, vec[2], neg_v[1], 0) / len
	}
	/* Reference code:
	if(std::fabs(vec[0]) > std::fabs(vec[1]))
	{
		const float recip_len = 1 / std::sqrt(vec[0] * vec[0] + vec[2] * vec[2]);

		x_axis = Vec4f(-vec[2] * recip_len, 0, vec[0] * recip_len, 0);
	}
	else
	{
		const float recip_len = 1 / std::sqrt(vec[1] * vec[1] + vec[2] * vec[2]);

		x_axis = Vec4f(0, vec[2] * recip_len, -vec[1] * recip_len, 0);
	}*/

	const Vec4f y_axis = crossProduct(vec, x_axis);

	assertIsUnitLength(x_axis);
	assertIsUnitLength(y_axis);
	assert(epsEqual(dot(x_axis, vec), 0.f) && epsEqual(dot(y_axis, vec), 0.f) && epsEqual(dot(x_axis, y_axis), 0.f));
	
	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/

	// elems 0...3 = v2
	_mm_store_ps(e, x_axis.v);

	// elems 4...7 = v1
	_mm_store_ps(e + 4, y_axis.v);

	// elems 8...11 = vec
	_mm_store_ps(e + 8, vec.v);

	// elems 12...15 = (0,0,0,1)
	_mm_store_ps(e + 12, Vec4f(0,0,0,1).v);
}


inline const Vec4f Matrix4f::constructFromVectorAndMul(const Vec4f& vec, const Vec4f& other_v)
{
	assertIsUnitLength(vec);
	assert(vec[3] == 0.f);

	Vec4f x_axis;

	// From PBR
	const Vec4f abs_vec = abs(vec);
	const Vec4f neg_v = -vec;
	if(abs_vec[0] > abs_vec[1])
	{
		
		const Vec4f len = sqrt(_mm_dp_ps(vec.v, vec.v, 0x5F));// Compute dot product with x,z components, write result to all components.
		x_axis = div(shuffle<2, 3, 0, 3>(neg_v, vec), len); // = (neg_v[2], neg_v[3], vec[0], vec[3]) / len = (neg_v[2], 0, vec[0], 0) / len
	}
	else
	{
		const Vec4f len = sqrt(_mm_dp_ps(vec.v, vec.v, 0x6F)); // Compute dot product with y,z components, write result to all components.
		x_axis = div(shuffle<3, 2, 1, 3>(vec, neg_v), len); // = (vec[3], vec[2], neg_v[1], neg_v[3]) / len = = (0, vec[2], neg_v[1], 0) / len
	}
	
	const Vec4f y_axis = crossProduct(vec, x_axis);

	assertIsUnitLength(x_axis);
	assertIsUnitLength(y_axis);
	assert(epsEqual(dot(x_axis, vec), 0.f) && epsEqual(dot(y_axis, vec), 0.f) && epsEqual(dot(x_axis, y_axis), 0.f));

	return mul(x_axis, copyToAll<0>(other_v)) + mul(y_axis, copyToAll<1>(other_v)) + mul(vec, copyToAll<2>(other_v));
}


bool Matrix4f::operator == (const Matrix4f& a) const
{
	return 
		getColumn(0) == a.getColumn(0) &&
		getColumn(1) == a.getColumn(1) &&
		getColumn(2) == a.getColumn(2) &&
		getColumn(3) == a.getColumn(3);
}


const Vec4f Matrix4f::getColumn(unsigned int col_index) const
{
	assert(col_index < 4);
	return Vec4f(_mm_load_ps(e + 4*col_index));
}


const Vec4f Matrix4f::getRow(unsigned int row_index) const
{
	assert(row_index < 4);
	return Vec4f(e[row_index], e[row_index + 4], e[row_index + 8], e[row_index + 12]);
}


void Matrix4f::setColumn(unsigned int col_index, const Vec4f& v)
{
	assert(col_index < 4);
	_mm_store_ps(e + 4*col_index, v.v);
}


void Matrix4f::setRow(unsigned int row_index, const Vec4f& v)
{
	assert(row_index < 4);
	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	e[row_index +  0] = v[0];
	e[row_index +  4] = v[1];
	e[row_index +  8] = v[2];
	e[row_index + 12] = v[3];
}


inline bool epsEqual(const Matrix4f& a, const Matrix4f& b, float eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<16; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}


inline bool approxEq(const Matrix4f& a, const Matrix4f& b, float eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<16; ++i)
		if(!Maths::approxEq(a.e[i], b.e[i], eps))
			return false;
	return true;
}
