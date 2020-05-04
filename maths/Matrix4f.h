/*=====================================================================
Matrix4f.h
----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Vec4f.h"
#include "../utils/MemAlloc.h"
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
	GLARE_ALIGNED_16_NEW_DELETE

	inline Matrix4f() {}
	inline Matrix4f(const float* data);
	inline Matrix4f(const Matrix4f& other);

	Matrix4f(const Matrix3<float>& upper_left_mat, const Vec3<float>& translation);
	inline Matrix4f(const Vec4f& col0, const Vec4f& col1, const Vec4f& col2, const Vec4f& col3);

	inline Matrix4f& operator = (const Matrix4f& other);

	inline void setToTranslationMatrix(float x, float y, float z);
	inline static const Matrix4f translationMatrix(float x, float y, float z);
	inline static const Matrix4f translationMatrix(const Vec4f& v);
	
	void setToRotationMatrix(const Vec4f& unit_axis, float angle);
	static const Matrix4f rotationMatrix(const Vec4f& unit_axis, float angle);
	static const Matrix4f rotationAroundXAxis(float angle);
	static const Matrix4f rotationAroundYAxis(float angle);
	static const Matrix4f rotationAroundZAxis(float angle);

	inline void applyUniformScale(float scale); // right multiply the matrix by a uniform scale matrix.
	inline void setToUniformScaleMatrix(float scale); // Make the upper-left 3x3 matrix a uniform scaling matrix.
	inline static const Matrix4f uniformScaleMatrix(float scale);

	inline void setToScaleMatrix(float xscale, float yscale, float zscale);
	inline static const Matrix4f scaleMatrix(float xscale, float yscale, float zscale);
	
	

	inline const Vec4f mul3Vector(const Vec4f& v) const;
	inline const Vec4f mul3Point(const Vec4f& v) const;
	inline const Vec4f transposeMult(const Vec4f& v) const;

	// Ignores W component of vector, returns vector with W component of 0.
	inline const Vec4f transposeMult3Vector(const Vec4f& v) const;

	inline void getTranspose(Matrix4f& transpose_out) const;
	inline Matrix4f getTranspose() const;

	inline void constructFromVector(const Vec4f& vec);
	static inline const Vec4f constructFromVectorAndMul(const Vec4f& vec, const Vec4f& other_v);

	inline bool operator == (const Matrix4f& a) const;

	inline const Matrix4f operator + (const Matrix4f& rhs) const;
	inline const Matrix4f operator * (const Matrix4f& rhs) const;


	inline float elem(unsigned int row_index, unsigned int column_index) const { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }
	inline float& elem(unsigned int row_index, unsigned int column_index) { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }


	// Get column (col_index is a zero-based index)
	inline const Vec4f getColumn(unsigned int col_index) const;
	inline const Vec4f getRow(unsigned int row_index) const;

	inline void setColumn(unsigned int col_index, const Vec4f& v);
	inline void setRow(unsigned int row_index, const Vec4f& v);


	// Is A the inverse of B?
	static bool isInverse(const Matrix4f& A, const Matrix4f& B, float eps = NICKMATHS_EPSILON);

	bool getUpperLeftInverse(Matrix4f& inverse_out, float& det_out) const;

	// Asumming that this matrix is the concatenation of a 3x3 rotation/scale/shear matrix and a translation matrix, return the inverse.
	bool getInverseForAffine3Matrix(Matrix4f& inverse_out) const;
	bool getUpperLeftInverseTranspose(Matrix4f& inverse_trans_out) const;

	inline float upperLeftDeterminant() const;

	// Returns *this * translation_matrix
	inline void rightMultiplyWithTranslationMatrix(const Vec4f& translation_vec, Matrix4f& result_out) const;

	// Assumes only the top-left 3x3 matrix is non-zero, apart from the bottom-right elem (which equals 1)
	bool polarDecomposition(Matrix4f& rot_out, Matrix4f& rest_out) const;

	Matrix3<float> getUpperLeftMatrix() const; // Returns the upper-left 3x3 matrix in a Matrix3<float> object, which is row-major.

	inline void setToIdentity();
	inline static const Matrix4f identity();

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


// Function declarations for standalone functions.
INDIGO_STRONG_INLINE const Vec4f operator * (const Matrix4f& m, const Vec4f& v);
inline bool epsEqual(const Matrix4f& a, const Matrix4f& b, float eps = NICKMATHS_EPSILON);
inline bool approxEq(const Matrix4f& a, const Matrix4f& b, float eps = NICKMATHS_EPSILON);
inline void mul(const Matrix4f& b, const Matrix4f& c, Matrix4f& result_out);
inline Matrix4f rightTranslate(const Matrix4f& mat, const Vec4f& translation_vec);
inline Matrix4f leftTranslateAffine3(const Vec4f& translation_vec, const Matrix4f& mat);


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


const Matrix4f Matrix4f::identity()
{
	const float data[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	return Matrix4f(data);
}


void Matrix4f::setToIdentity()
{
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
	e[12] = 0.f;
	e[13] = 0.f;
	e[14] = 0.f;
	e[15] = 1.f;
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


Matrix4f Matrix4f::getTranspose() const
{
	Matrix4f ret;

	Vec4f c0, c1, c2, c3;
	transpose(getColumn(0), getColumn(1), getColumn(2), getColumn(3), c0, c1, c2, c3);

	ret.setColumn(0, c0);
	ret.setColumn(1, c1);
	ret.setColumn(2, c2);
	ret.setColumn(3, c3);
	return ret;
}


const Matrix4f Matrix4f::operator + (const Matrix4f& other) const
{
	Matrix4f res;
	res.setColumn(0, getColumn(0) + other.getColumn(0));
	res.setColumn(1, getColumn(1) + other.getColumn(1));
	res.setColumn(2, getColumn(2) + other.getColumn(2));
	res.setColumn(3, getColumn(3) + other.getColumn(3));
	return res;
}


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


inline bool epsEqual(const Matrix4f& a, const Matrix4f& b, float eps/* = NICKMATHS_EPSILON*/)
{
	for(unsigned int i=0; i<16; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}


inline bool approxEq(const Matrix4f& a, const Matrix4f& b, float eps/* = NICKMATHS_EPSILON*/)
{
	for(unsigned int i=0; i<16; ++i)
		if(!Maths::approxEq(a.e[i], b.e[i], eps))
			return false;
	return true;
}


void Matrix4f::applyUniformScale(float scale)
{
	const Vec4f scalev(scale);

	_mm_store_ps(e + 0, mul(Vec4f(_mm_load_ps(e + 0)), scalev).v);
	_mm_store_ps(e + 4, mul(Vec4f(_mm_load_ps(e + 4)), scalev).v);
	_mm_store_ps(e + 8, mul(Vec4f(_mm_load_ps(e + 8)), scalev).v);
	// Last column is not scaled.
}


void Matrix4f::setToUniformScaleMatrix(float scale)
{
	e[0] = scale;
	e[1] = 0;
	e[2] = 0;
	e[3] = 0;
	e[4] = 0;
	e[5] = scale;
	e[6] = 0;
	e[7] = 0;
	e[8] = 0;
	e[9] = 0;
	e[10] = scale;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::uniformScaleMatrix(float scale)
{
	Matrix4f m;
	m.setToUniformScaleMatrix(scale);
	return m;
}


void Matrix4f::setToScaleMatrix(float xscale, float yscale, float zscale)
{
	e[0] = xscale;
	e[1] = 0;
	e[2] = 0;
	e[3] = 0;
	e[4] = 0;
	e[5] = yscale;
	e[6] = 0;
	e[7] = 0;
	e[8] = 0;
	e[9] = 0;
	e[10] = zscale;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::scaleMatrix(float x, float y, float z)
{
	Matrix4f m;
	m.setToScaleMatrix(x, y, z);
	return m;
}



// See also https://gist.github.com/rygorous/4172889
inline void mul(const Matrix4f& b, const Matrix4f& c, Matrix4f& result_out)
{
	/*
	a_11 = b_11*c_11 + b_12*c_21 + b_13*c_32 + b_14*c_41
	a_21 = b_21*c_11 + b_22*c_21 + b_23*c_32 + b_24*c_41
	a_31 = b_31*c_11 + b_32*c_21 + b_33*c_32 + b_34*c_41
	a_41 = b_41*c_11 + b_42*c_21 + b_43*c_32 + b_44*c_41

	so
	a_col_1 = b_col_1*c_11 + b_col_2*c_21 + b_col_3*c_31 + b_col_4*c_41
	etc..
	*/

	const Vec4f bcol0 = Vec4f(_mm_load_ps(b.e + 0));
	const Vec4f bcol1 = Vec4f(_mm_load_ps(b.e + 4));
	const Vec4f bcol2 = Vec4f(_mm_load_ps(b.e + 8));
	const Vec4f bcol3 = Vec4f(_mm_load_ps(b.e + 12));

	const Vec4f ccol0 = Vec4f(_mm_load_ps(c.e + 0));
	const Vec4f ccol1 = Vec4f(_mm_load_ps(c.e + 4));
	const Vec4f ccol2 = Vec4f(_mm_load_ps(c.e + 8));
	const Vec4f ccol3 = Vec4f(_mm_load_ps(c.e + 12));

	const Vec4f a_col0 = mul(bcol0, Vec4f(indigoCopyToAll(ccol0.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol0.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol0.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol0.v, 3)));
	const Vec4f a_col1 = mul(bcol0, Vec4f(indigoCopyToAll(ccol1.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol1.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol1.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol1.v, 3)));
	const Vec4f a_col2 = mul(bcol0, Vec4f(indigoCopyToAll(ccol2.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol2.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol2.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol2.v, 3)));
	const Vec4f a_col3 = mul(bcol0, Vec4f(indigoCopyToAll(ccol3.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol3.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol3.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol3.v, 3)));

	_mm_store_ps(result_out.e + 0, a_col0.v);
	_mm_store_ps(result_out.e + 4, a_col1.v);
	_mm_store_ps(result_out.e + 8, a_col2.v);
	_mm_store_ps(result_out.e + 12, a_col3.v);
}


// See comments in mul() above.
const Matrix4f Matrix4f::operator * (const Matrix4f& c) const
{
	Matrix4f res;

	// b = this matrix.  Result = a.
	const Vec4f bcol0 = Vec4f(_mm_load_ps(e + 0));
	const Vec4f bcol1 = Vec4f(_mm_load_ps(e + 4));
	const Vec4f bcol2 = Vec4f(_mm_load_ps(e + 8));
	const Vec4f bcol3 = Vec4f(_mm_load_ps(e + 12));

	const Vec4f ccol0 = Vec4f(_mm_load_ps(c.e + 0));
	const Vec4f ccol1 = Vec4f(_mm_load_ps(c.e + 4));
	const Vec4f ccol2 = Vec4f(_mm_load_ps(c.e + 8));
	const Vec4f ccol3 = Vec4f(_mm_load_ps(c.e + 12));

	const Vec4f a_col0 = mul(bcol0, Vec4f(indigoCopyToAll(ccol0.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol0.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol0.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol0.v, 3)));
	const Vec4f a_col1 = mul(bcol0, Vec4f(indigoCopyToAll(ccol1.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol1.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol1.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol1.v, 3)));
	const Vec4f a_col2 = mul(bcol0, Vec4f(indigoCopyToAll(ccol2.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol2.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol2.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol2.v, 3)));
	const Vec4f a_col3 = mul(bcol0, Vec4f(indigoCopyToAll(ccol3.v, 0))) + mul(bcol1, Vec4f(indigoCopyToAll(ccol3.v, 1)))  + mul(bcol2, Vec4f(indigoCopyToAll(ccol3.v, 2))) + mul(bcol3, Vec4f(indigoCopyToAll(ccol3.v, 3)));

	_mm_store_ps(res.e + 0, a_col0.v);
	_mm_store_ps(res.e + 4, a_col1.v);
	_mm_store_ps(res.e + 8, a_col2.v);
	_mm_store_ps(res.e + 12, a_col3.v);
	return res;
}


float Matrix4f::upperLeftDeterminant() const
{
	const Vec4f c0 = getColumn(0);
	const Vec4f c1 = getColumn(1);
	const Vec4f c2 = getColumn(2);
	/*
	det = m_11.m_22.m_33 + m_12.m_23.m_31 + m_13.m_21.m_32 -
		  m_31.m_22.m_31 - m_11.m_23.m_32 - m_12.m_21.m_33

	c0 = (m_11, m_21, m_31, 0)
	c1 = (m_12, m_22, m_32, 0)
	c2 = (m_13, m_23, m_33, 0)
	
	so shuffle columns to
	c0' = (m_11, m_31, m_21, 0)
	c1' = (m_22, m_12, m_32, 0)
	c2' = (m_33, m_23, m_13, 0)

	Then if do component-wise multiplication between of c0', c1', c2' we get

	(m_11.m_22.m_33, m_31.m_12.m_23, m_21.m_32.m_13, 0)

	If we do a horizontal add on this we get the upper part of the determinant.
	To do this component-wise multiplication and horizontal add, we can use a mulps, then a dot-product.
	Likewise for the lower part of the determinant.
	*/
	return	dot(mul(swizzle<0, 2, 1, 3>(c0), swizzle<1, 0, 2, 3>(c1)), swizzle<2, 1, 0, 3>(c2)) - 
			dot(mul(swizzle<2, 1, 0, 3>(c0), swizzle<1, 0, 2, 3>(c1)), swizzle<0, 2, 1, 3>(c2));
}


void Matrix4f::rightMultiplyWithTranslationMatrix(const Vec4f& translation_vec, Matrix4f& result_out) const
{
	/*
	(m_11   m_12   m_13   m_14)  (1  0  0  t_x) = (m_11   m_12   m_13   m_11.t_x + m_12.t_y + m_13.t_z + m_14)
	(m_21   m_22   m_23   m_24)  (0  1  0  t_y)   (m_21   m_22   m_23   m_21.t_x + m_22.t_y + m_23.t_z + m_24)
	(m_31   m_32   m_33   m_34)  (0  0  1  t_z)   (m_31   m_32   m_33   m_31.t_x + m_32.t_y + m_33.t_z + m_34)
	(m_41   m_42   m_43   m_44)  (0  0  0    1)   (m_41   m_42   m_43   m_41.t_x + m_42.t_y + m_43.t_z + m_44)

	See code_documentation/Matrix4f proofs.wxmx also.
	*/
	const Vec4f c0 = getColumn(0);
	const Vec4f c1 = getColumn(1);
	const Vec4f c2 = getColumn(2);
	const Vec4f c3 = getColumn(3);
	result_out.setColumn(0, c0);
	result_out.setColumn(1, c1);
	result_out.setColumn(2, c2);
	result_out.setColumn(3, 
		(mul(c0, copyToAll<0>(translation_vec)) + 
		 mul(c1, copyToAll<1>(translation_vec))) + 
		(mul(c2, copyToAll<2>(translation_vec)) + 
		 c3)
	);
}


// Equivalent to mat * Matrix4f::translationMatrix(translation_vec)
inline Matrix4f rightTranslate(const Matrix4f& mat, const Vec4f& translation_vec)
{
	assert(translation_vec[3] == 0.f);

	/*
	(m_11   m_12   m_13   m_14)  (1  0  0  t_x) = (m_11   m_12   m_13   m_11.t_x + m_12.t_y + m_13.t_z + m_14)
	(m_21   m_22   m_23   m_24)  (0  1  0  t_y)   (m_21   m_22   m_23   m_21.t_x + m_22.t_y + m_23.t_z + m_24)
	(m_31   m_32   m_33   m_34)  (0  0  1  t_z)   (m_31   m_32   m_33   m_31.t_x + m_32.t_y + m_33.t_z + m_34)
	(m_41   m_42   m_43   m_44)  (0  0  0    1)   (m_41   m_42   m_43   m_41.t_x + m_42.t_y + m_43.t_z + m_44)
	*/
	const Vec4f c0 = mat.getColumn(0);
	const Vec4f c1 = mat.getColumn(1);
	const Vec4f c2 = mat.getColumn(2);
	const Vec4f c3 = mat.getColumn(3);

	return Matrix4f(
		c0,
		c1,
		c2,
		(mul(c0, copyToAll<0>(translation_vec)) +
		 mul(c1, copyToAll<1>(translation_vec))) +
		(mul(c2, copyToAll<2>(translation_vec)) +
		 c3)
	);
}


// Equivalent to Matrix4f::translationMatrix(translation_vec) * mat, assuming mat has bottom row (0,0,0,1).
inline Matrix4f leftTranslateAffine3(const Vec4f& translation_vec, const Matrix4f& mat)
{
	assert(translation_vec[3] == 0.f);
	assert(mat.getRow(3) == Vec4f(0, 0, 0, 1));

	/*
	(1  0  0  t_x)  (m_11   m_12   m_13   m_14)  = (m_11   m_12   m_13   t_x + m_14)
	(0  1  0  t_y)  (m_21   m_22   m_23   m_24)    (m_21   m_22   m_23   t_y + m_24)
	(0  0  1  t_z)  (m_31   m_32   m_33   m_34)    (m_31   m_32   m_33   t_z + m_34)
	(0  0  0    1)  (   0      0      0      1)    (   0      0      0            1)
	*/
	return Matrix4f(
		mat.getColumn(0),
		mat.getColumn(1),
		mat.getColumn(2),
		mat.getColumn(3) + translation_vec
	);
}
