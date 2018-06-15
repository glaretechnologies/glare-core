/*=====================================================================
Matrix4f.h
----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "Matrix4f.h"


#include "matrix3.h"
#include "vec3.h"
#include "../utils/StringUtils.h"


Matrix4f::Matrix4f(const Matrix3f& upper_left_mat, const Vec3f& translation)
{
	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/

	e[0] = upper_left_mat.e[0];
	e[4] = upper_left_mat.e[1];
	e[8] = upper_left_mat.e[2];
	e[1] = upper_left_mat.e[3];
	e[5] = upper_left_mat.e[4];
	e[9] = upper_left_mat.e[5];
	e[2] = upper_left_mat.e[6];
	e[6] = upper_left_mat.e[7];
	e[10] = upper_left_mat.e[8];

	e[12] = translation.x;
	e[13] = translation.y;
	e[14] = translation.z;

	e[3] = e[7] = e[11] = 0.0f;
	e[15] = 1.0f;
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


// See also https://gist.github.com/rygorous/4172889
void mul(const Matrix4f& b, const Matrix4f& c, Matrix4f& result_out)
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


bool Matrix4f::isInverse(const Matrix4f& A, const Matrix4f& B)
{
	Matrix4f AB, BA;
	mul(A, B, AB);
	mul(B, A, BA);
	return (epsEqual(AB, identity()) || approxEq(AB, identity())) && (epsEqual(BA, identity()) || approxEq(BA, identity()));
}


bool Matrix4f::getUpperLeftInverse(Matrix4f& inverse_out, float& det_out) const
{
	Vec4f c0 = getColumn(0);
	Vec4f c1 = getColumn(1);
	Vec4f c2 = getColumn(2);

	// Get inverse of upper left matrix (R).
	const float d = upperLeftDeterminant();
	det_out = d;
	if(d == 0.0f)
		return false; // Singular matrix

	/*
	inverse of upper-left matrix, without 1/det factor is  (See http://mathworld.wolfram.com/MatrixInverse.html, figure 6)
	m_22.m_33 - m_23.m_32    m_13.m_32 - m_12.m_33   m_12.m_23 - m_13.m_22   0
	m_23.m_31 - m_21.m_33    m_11.m_33 - m_13.m_31   m_13.m_21 - m_11.m_23   0
	m_21.m_32 - m_22.m_31    m_12.m_31 - m_11.m_32   m_11.m_22 - m_12.m_21   0
	0                        0                       0                       1

	using 0-based columns and indices:
	=
	c1[1].c2[2] - c2[1].c1[2]    c2[0].c1[2] - c1[0].c2[2]    c1[0].c2[1] - c2[0].c1[1]  0
	c2[1].c0[2] - c0[1].c2[2]    c0[0].c2[2] - c2[0].c0[2]    c2[0].c0[1] - c0[0].c2[1]  0
	c0[1].c1[2] - c1[1].c0[2]    c1[0].c0[2] - c0[0].c1[2]    c0[0].c1[1] - c1[0].c0[1]  0
	0                            0                            0                          1
	*/
	const Vec4f lo_c1_c2 = unpacklo(c1, c2); // (c1[0], c2[0], c1[1], c2[1])
	const Vec4f lo_c0_c2 = unpacklo(c0, c2); // (c0[0], c2[0], c0[1], c2[1])
	const Vec4f hi_c0_c2 = unpackhi(c0, c2); // (c0[2], c2[2], c0[3], c2[3])
	const Vec4f hi_c1_c2 = unpackhi(c1, c2); // (c1[2], c2[2], c1[3], c2[3])

	const Vec4f c11_c21_c01 = shuffle<2, 3, 1, 3>(lo_c1_c2, c0); // (c1[1], c2[1], c0[1], c0[3])
	const Vec4f c22_c02_c12 = shuffle<1, 0, 2, 3>(hi_c0_c2, c1); // (c2[2], c0[2], c1[2], c1[3])
	const Vec4f c21_c01_c11 = shuffle<3, 2, 1, 3>(lo_c0_c2, c1);
	const Vec4f c12_c22_c02 = shuffle<0, 1, 2, 3>(hi_c1_c2, c0);
	const Vec4f c20_c00_c10 = shuffle<1, 0, 0, 3>(lo_c0_c2, c1);
	const Vec4f c10_c20_c00 = shuffle<0, 1, 0, 3>(lo_c1_c2, c0);

	c0 = mul(c11_c21_c01, c22_c02_c12) - mul(c21_c01_c11, c12_c22_c02);
	c1 = mul(c20_c00_c10, c12_c22_c02) - mul(c10_c20_c00, c22_c02_c12);
	c2 = mul(c10_c20_c00, c21_c01_c11) - mul(c20_c00_c10, c11_c21_c01);

	assert(c0[3] == 0.0f && c1[3] == 0.0f && c2[3] == 0.0f);

	const Vec4f recip_det = Vec4f(1 / d);

	c0 = mul(c0, recip_det);
	c1 = mul(c1, recip_det);
	c2 = mul(c2, recip_det);

	inverse_out.setColumn(0, c0);
	inverse_out.setColumn(1, c1);
	inverse_out.setColumn(2, c2);
	inverse_out.setColumn(3, Vec4f(0,0,0,1));
	return true;
}


bool Matrix4f::getInverseForAffine3Matrix(Matrix4f& inverse_out) const 
{
	/*
	We are assuming the matrix is a concatenation of a translation and rotation/scale/shear matrix.  In other words we are assuming the bottom row is (0,0,0,1).
	M = T R
	M^-1 = (T R)^-1 = R^-1 T^-1
	*/
	assert(e[3] == 0.f && e[7] == 0.f && e[11] == 0.f); // Check bottom row has zeroes.
	
	Vec4f c0 = getColumn(0);
	Vec4f c1 = getColumn(1);
	Vec4f c2 = getColumn(2);
	Vec4f c3 = getColumn(3);

	// Get inverse of upper left matrix (R).
	const float d = upperLeftDeterminant();
	if(d == 0.0f)
		return false; // Singular matrix

	/*
	inverse of upper-left matrix, without 1/det factor is  (See http://mathworld.wolfram.com/MatrixInverse.html, figure 6)
	m_22.m_33 - m_23.m_32    m_13.m_32 - m_12.m_33   m_12.m_23 - m_13.m_22   0
	m_23.m_31 - m_21.m_33    m_11.m_33 - m_13.m_31   m_13.m_21 - m_11.m_23   0
	m_21.m_32 - m_22.m_31    m_12.m_31 - m_11.m_32   m_11.m_22 - m_12.m_21   0
	0                        0                       0                       1

	using 0-based columns and indices:
	=
	c1[1].c2[2] - c2[1].c1[2]    c2[0].c1[2] - c1[0].c2[2]    c1[0].c2[1] - c2[0].c1[1]  0
	c2[1].c0[2] - c0[1].c2[2]    c0[0].c2[2] - c2[0].c0[2]    c2[0].c0[1] - c0[0].c2[1]  0
	c0[1].c1[2] - c1[1].c0[2]    c1[0].c0[2] - c0[0].c1[2]    c0[0].c1[1] - c1[0].c0[1]  0
	0                            0                            0                          1
	*/
	const Vec4f lo_c1_c2 = unpacklo(c1, c2); // (c1[0], c2[0], c1[1], c2[1])
	const Vec4f lo_c0_c2 = unpacklo(c0, c2); // (c0[0], c2[0], c0[1], c2[1])
	const Vec4f hi_c0_c2 = unpackhi(c0, c2); // (c0[2], c2[2], c0[3], c2[3])
	const Vec4f hi_c1_c2 = unpackhi(c1, c2); // (c1[2], c2[2], c1[3], c2[3])

	const Vec4f c11_c21_c01 = shuffle<2, 3, 1, 3>(lo_c1_c2, c0); // (c1[1], c2[1], c0[1], c0[3])
	const Vec4f c22_c02_c12 = shuffle<1, 0, 2, 3>(hi_c0_c2, c1); // (c2[2], c0[2], c1[2], c1[3])
	const Vec4f c21_c01_c11 = shuffle<3, 2, 1, 3>(lo_c0_c2, c1);
	const Vec4f c12_c22_c02 = shuffle<0, 1, 2, 3>(hi_c1_c2, c0);
	const Vec4f c20_c00_c10 = shuffle<1, 0, 0, 3>(lo_c0_c2, c1);
	const Vec4f c10_c20_c00 = shuffle<0, 1, 0, 3>(lo_c1_c2, c0);

	c0 = mul(c11_c21_c01, c22_c02_c12) - mul(c21_c01_c11, c12_c22_c02);
	c1 = mul(c20_c00_c10, c12_c22_c02) - mul(c10_c20_c00, c22_c02_c12);
	c2 = mul(c10_c20_c00, c21_c01_c11) - mul(c20_c00_c10, c11_c21_c01);

	assert(c0[3] == 0.0f && c1[3] == 0.0f && c2[3] == 0.0f);

	const Vec4f recip_det = Vec4f(1 / d);

	c0 = mul(c0, recip_det);
	c1 = mul(c1, recip_det);
	c2 = mul(c2, recip_det);

	inverse_out.setColumn(0, c0);
	inverse_out.setColumn(1, c1);
	inverse_out.setColumn(2, c2);

	/*
	(m_11   m_12   m_13   0)  (1  0  0  t_x) = (m_11   m_12   m_13   m_11.t_x + m_12.t_y + m_13.t_z)
	(m_21   m_22   m_23   0)  (0  1  0  t_y)   (m_21   m_22   m_23   m_21.t_x + m_22.t_y + m_23.t_z)
	(m_31   m_32   m_33   0)  (0  0  1  t_z)   (m_31   m_32   m_33   m_31.t_x + m_32.t_y + m_33.t_z)
	(0      0      0      1)  (0  0  0    1)   (0      0      0                                   1)
	*/
	const Vec4f t = -c3; // Last column of T^-1
	inverse_out.setColumn(3, 
		mul(c0, copyToAll<0>(t)) + 
		mul(c1, copyToAll<1>(t)) + 
		mul(c2, copyToAll<2>(t))
	);
	inverse_out.e[15] = 1.f;

	//assert(Matrix4f::isInverse(*this, inverse_out)); // This can fail due to imprecision for large translations.
	return true;
}


bool Matrix4f::getUpperLeftInverseTranspose(Matrix4f& inverse_trans_out) const 
{
	const float d = upperLeftDeterminant();
	if(d == 0.0f)
		return false; // Singular matrix

	/*
	inverse of upper-left matrix, without 1/det factor is  (See http://mathworld.wolfram.com/MatrixInverse.html, figure 6)
	m_22.m_33 - m_23.m_32    m_13.m_32 - m_12.m_33   m_12.m_23 - m_13.m_22   0
	m_23.m_31 - m_21.m_33    m_11.m_33 - m_13.m_31   m_13.m_21 - m_11.m_23   0
	m_21.m_32 - m_22.m_31    m_12.m_31 - m_11.m_32   m_11.m_22 - m_12.m_21   0
	0                        0                       0                       1

	So transpose inverse without 1/det factor is
	m_22.m_33 - m_23.m_32    m_23.m_31 - m_21.m_33   m_21.m_32 - m_22.m_31   0
	m_13.m_32 - m_12.m_33    m_11.m_33 - m_13.m_31   m_12.m_31 - m_11.m_32   0
	m_12.m_23 - m_13.m_22    m_13.m_21 - m_11.m_23   m_11.m_22 - m_12.m_21   0
	0                        0                       0                       1

	using 0-based columns and indices:
	= 
	c1[1].c2[2] - c2[1].c1[2]    c2[1].c0[2] - c0[1].c2[2]   c0[1].c1[2] - c1[1].c0[2]   0
	c2[0].c1[2] - c1[0].c2[2]    c0[0].c2[2] - c2[0].c0[2]   c1[0].c0[2] - c0[0].c1[2]   0
	c1[0].c2[1] - c2[0].c1[1]    c2[0].c0[1] - c0[0].c2[1]   c0[0].c1[1] - c1[0].c0[1]   0
	0                             0                           0                          1

	=
	c1[1].c2[2] - c2[1].c1[2]    c2[1].c0[2] - c0[1].c2[2]   c0[1].c1[2] - c1[1].c0[2]   0
	c1[2].c2[0] - c2[2].c1[0]    c2[2].c0[0] - c0[2].c2[0]   c0[2].c1[0] - c1[2].c0[0]   0
	c1[0].c2[1] - c2[0].c1[1]    c2[0].c0[1] - c0[0].c2[1]   c0[0].c1[1] - c1[0].c0[1]   0
	0                             0                           0                          1
	*/

	Vec4f c0 = getColumn(0);
	Vec4f c1 = getColumn(1);
	Vec4f c2 = getColumn(2);

	const Vec4f c0_201 = swizzle<2, 0, 1, 3>(c0);
	const Vec4f c0_120 = swizzle<1, 2, 0, 3>(c0);
	const Vec4f c1_201 = swizzle<2, 0, 1, 3>(c1);
	const Vec4f c1_120 = swizzle<1, 2, 0, 3>(c1);
	const Vec4f c2_201 = swizzle<2, 0, 1, 3>(c2);
	const Vec4f c2_120 = swizzle<1, 2, 0, 3>(c2);
	
	c0 = mul(c1_120, c2_201) - mul(c2_120, c1_201);
	c1 = mul(c2_120, c0_201) - mul(c0_120, c2_201);
	c2 = mul(c0_120, c1_201) - mul(c1_120, c0_201);

	assert(c0[3] == 0.0f && c1[3] == 0.0f && c2[3] == 0.0f);

	const Vec4f recip_det = Vec4f(1 / d);

	c0 = mul(c0, recip_det);
	c1 = mul(c1, recip_det);
	c2 = mul(c2, recip_det);
	
	inverse_trans_out.setColumn(0, c0);
	inverse_trans_out.setColumn(1, c1);
	inverse_trans_out.setColumn(2, c2);
	inverse_trans_out.setColumn(3, Vec4f(0,0,0,1));
	return true;
}


void Matrix4f::setToRotationMatrix(const Vec4f& unit_axis, float angle)
{
	assert(unit_axis.isUnitLength());

	//-----------------------------------------------------------------
	//build the rotation matrix
	//see http://mathworld.wolfram.com/RodriguesRotationFormula.html
	//-----------------------------------------------------------------
	const float a = unit_axis[0];
	const float b = unit_axis[1];
	const float c = unit_axis[2];

	const float cost = std::cos(angle);
	const float sint = std::sin(angle);

	const float asint = a*sint;
	const float bsint = b*sint;
	const float csint = c*sint;

	const float one_minus_cost = 1 - cost;

	e[ 0] = a*a*one_minus_cost + cost;
	e[ 1] = a*b*one_minus_cost + csint;
	e[ 2] = a*c*one_minus_cost - bsint;
	e[ 3] = 0;
	e[ 4] = a*b*one_minus_cost - csint;
	e[ 5] = b*b*one_minus_cost + cost;
	e[ 6] = b*c*one_minus_cost + asint;
	e[ 7] = 0;
	e[ 8] = a*c*one_minus_cost + bsint;
	e[ 9] = b*c*one_minus_cost - asint;
	e[10] = c*c*one_minus_cost + cost;
	e[11] = 0;
	e[12] = 0;
	e[13] = 0;
	e[14] = 0;
	e[15] = 1;
}


const Matrix4f Matrix4f::rotationMatrix(const Vec4f& unit_axis, float angle)
{
	Matrix4f m;
	m.setToRotationMatrix(unit_axis, angle);
	return m;
}


const Matrix4f Matrix4f::rotationAroundXAxis(float angle)
{
	const float cos_theta = std::cos(angle);
	const float sin_theta = std::sin(angle);
	Matrix4f m;
	m.e[0] = 1;
	m.e[1] = 0;
	m.e[2] = 0;
	m.e[3] = 0;
	m.e[4] = 0;
	m.e[5] = cos_theta;
	m.e[6] = sin_theta;
	m.e[7] = 0;
	m.e[8] = 0;
	m.e[9] = -sin_theta;
	m.e[10] = cos_theta;
	m.e[11] = 0;
	m.e[12] = 0;
	m.e[13] = 0;
	m.e[14] = 0;
	m.e[15] = 1;
	return m;
}


const Matrix4f Matrix4f::rotationAroundYAxis(float angle)
{
	const float cos_theta = std::cos(angle);
	const float sin_theta = std::sin(angle);
	Matrix4f m;
	m.e[0] = cos_theta;
	m.e[1] = 0;
	m.e[2] = -sin_theta;
	m.e[3] = 0;
	m.e[4] = 0;
	m.e[5] = 1;
	m.e[6] = 0;
	m.e[7] = 0;
	m.e[8] = sin_theta;
	m.e[9] = 0;
	m.e[10] = cos_theta;
	m.e[11] = 0;
	m.e[12] = 0;
	m.e[13] = 0;
	m.e[14] = 0;
	m.e[15] = 1;
	return m;
}


const Matrix4f Matrix4f::rotationAroundZAxis(float angle)
{
	const float cos_theta = std::cos(angle);
	const float sin_theta = std::sin(angle);
	Matrix4f m;
	m.e[0] = cos_theta;
	m.e[1] = sin_theta;
	m.e[2] = 0;
	m.e[3] = 0;
	m.e[4] = -sin_theta;
	m.e[5] = cos_theta;
	m.e[6] = 0;
	m.e[7] = 0;
	m.e[8] = 0;
	m.e[9] = 0;
	m.e[10] = 1;
	m.e[11] = 0;
	m.e[12] = 0;
	m.e[13] = 0;
	m.e[14] = 0;
	m.e[15] = 1;
	return m;
}


void Matrix4f::applyUniformScale(float scale)
{
	const Vec4f scalev(scale);

	_mm_store_ps(e + 0,  mul(Vec4f(_mm_load_ps(e + 0)), scalev).v);
	_mm_store_ps(e + 4,  mul(Vec4f(_mm_load_ps(e + 4)), scalev).v);
	_mm_store_ps(e + 8,  mul(Vec4f(_mm_load_ps(e + 8)), scalev).v);
	// Last column is not scaled.
}


void Matrix4f::setToUniformScaleMatrix(float scale)
{
	e[ 0] = scale;
	e[ 1] = 0;
	e[ 2] = 0;
	e[ 3] = 0;
	e[ 4] = 0;
	e[ 5] = scale;
	e[ 6] = 0;
	e[ 7] = 0;
	e[ 8] = 0;
	e[ 9] = 0;
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
	e[ 0] = xscale;
	e[ 1] = 0;
	e[ 2] = 0;
	e[ 3] = 0;
	e[ 4] = 0;
	e[ 5] = yscale;
	e[ 6] = 0;
	e[ 7] = 0;
	e[ 8] = 0;
	e[ 9] = 0;
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


const std::string Matrix4f::rowString(int row_index) const
{
	return ::toString(e[row_index + 0]) + " " + ::toString(e[row_index + 4]) + " " + ::toString(e[row_index + 8]) + " " + ::toString(e[row_index + 12]);
}


const std::string Matrix4f::toString() const
{
	return rowString(0) + "\n" + rowString(1) + "\n" + rowString(2) + "\n" + rowString(3);
}


inline static bool isSignificantDiff(const Matrix4f& a, const Matrix4f& b)
{
	Vec4f max_diff_v = 
		max(max(abs(a.getColumn(0) - b.getColumn(0)), 
				abs(a.getColumn(1) - b.getColumn(1))),
			max(abs(a.getColumn(2) - b.getColumn(2)), 
				abs(a.getColumn(3) - b.getColumn(3)))
		);

	const float max_diff = horizontalMax(max_diff_v.v);
	return max_diff > 2.0e-7f;
}


// See 'Matrix Animation and Polar Decomposition' by Ken Shoemake & Tom Duff
// http://research.cs.wisc.edu/graphics/Courses/838-s2002/Papers/polar-decomp.pdf
// Assumes only the top-left 3x3 matrix is non-zero, apart from the bottom-right elem (which equals 1)
bool Matrix4f::polarDecomposition(Matrix4f& rot_out, Matrix4f& rest_out) const
{
	assert(getRow(3) == Vec4f(0,0,0,1) && getColumn(3) == Vec4f(0,0,0,1));

	Matrix4f Q = *this;

	while(1)
	{
		// Compute inverse transpose of Q
		const Matrix4f T = Q.getTranspose();
		Matrix4f T_inv;
		float dummy_det;
		const bool invertible = T.getUpperLeftInverse(T_inv, dummy_det);
		if(!invertible)
			return false;

		Matrix4f new_Q = (Q + T_inv);
		new_Q.e[15] = 1.f;
		new_Q.applyUniformScale(0.5f);
		
		if(!isSignificantDiff(new_Q, Q))
		{
			Q = new_Q;
			break;
		}

		Q = new_Q;
	}

	if(Q.upperLeftDeterminant() < 0)
		Q.applyUniformScale(-1);

	rot_out = Q;

	// M = QS
	// Q^-1 M = Q^-1 Q S
	// Q^T M = S			[Q^-1 = Q^T as Q is a rotation matrix]
	rest_out = Q.getTranspose() * *this;
	return true;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"


// Is the matrix orthogonal and does it have determinant 1?
static bool isSpecialOrthogonal(const Matrix4f& m)
{
	return epsEqual(m * m.getTranspose(), Matrix4f::identity()) && epsEqual(m.upperLeftDeterminant(), 1.0f);
}


static void refMul(const Matrix4f& a, const Matrix4f& b, Matrix4f& result_out)
{
	for(unsigned int outcol=0; outcol<4; ++outcol)
		for(unsigned int outrow=0; outrow<4; ++outrow)
		{
			float x = 0.0f;

			for(unsigned int i=0; i<4; ++i)
				x += a.elem(outrow, i) * b.elem(i, outcol);

			result_out.elem(outrow, outcol) = x;
		}
}


static void testIsOrthogonal(const Matrix4f& m)
{
	testEpsEqual(m.getColumn(0).length(), 1.0f);
	testEpsEqual(m.getColumn(1).length(), 1.0f);
	testEpsEqual(m.getColumn(2).length(), 1.0f);
	testEpsEqual(dot(m.getColumn(0), m.getColumn(1)), 0.f);
	testEpsEqual(dot(m.getColumn(0), m.getColumn(2)), 0.f);
	testEpsEqual(dot(m.getColumn(1), m.getColumn(2)), 0.f);
}


static void testVectorsFormOrthonormalBasis(const Vec4f& a, const Vec4f& b, const Vec4f& c)
{
	testEpsEqual(a.length(), 1.0f);
	testEpsEqual(b.length(), 1.0f);
	testEpsEqual(c.length(), 1.0f);
	testEpsEqual(dot(a, b), 0.f);
	testEpsEqual(dot(a, c), 0.f);
	testEpsEqual(dot(b, c), 0.f);
}


static void testConstructFromVectorAndMulForVec(const Vec4f& v)
{
	const Vec4f M_i = Matrix4f::constructFromVectorAndMul(v, Vec4f(1,0,0,0));
	const Vec4f M_j = Matrix4f::constructFromVectorAndMul(v, Vec4f(0,1,0,0));
	const Vec4f M_k = Matrix4f::constructFromVectorAndMul(v, Vec4f(0,0,1,0));
	testVectorsFormOrthonormalBasis(M_i, M_j, M_k);
}


void Matrix4f::test()
{
	conPrint("Matrix4f::test()");

	//-------------------------- Test Copy constructor ----------------------------
	{
		const Matrix4f m = Matrix4f::rotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f) * Matrix4f::translationMatrix(1.f, 2.f, 3.f);
		Matrix4f m2(m);
		testAssert(m == m2);
	}

	//-------------------------- Test operator = ----------------------------
	{
		const Matrix4f m = Matrix4f::rotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f) * Matrix4f::translationMatrix(1.f, 2.f, 3.f);
		Matrix4f m2;
		m2 = m;
		testAssert(m == m2);
	}

	//-------------------------- Test operator + ----------------------------
	{
		float ea[16];
		float eb[16];
		for(int i=0; i<16; ++i)
		{
			ea[i] = (float)i;
			eb[i] = (float)(10 + i);
		}
		const Matrix4f m1(ea);
		const Matrix4f m2(eb);
		const Matrix4f sum = m1 + m2;
		for(int i=0; i<16; ++i)
			testAssert(sum.e[i] == ea[i] + eb[i]);
	}

	//-------------------------- Test operator == ----------------------------
	{
		const Matrix4f m = Matrix4f::rotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f) * Matrix4f::translationMatrix(1.f, 2.f, 3.f);
		Matrix4f m2 = m;
		testAssert(m == m2);
		m2.e[0] = 0.1f;
		testAssert(!(m == m2));
	}

	//-------------------------- Test getUpperLeftInverse ----------------------------
	{
		// Test with non-zero values in translation column, that should have no effect.
		Matrix4f m = Matrix4f::rotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f);
		m.e[12] = 1.f;
		m.e[13] = 2.f;
		m.e[14] = 3.f;

		Matrix4f inv;
		float det;
		testAssert(m.getUpperLeftInverse(inv, det));
		testAssert(det == m.upperLeftDeterminant());
		m.setColumn(3, Vec4f(0,0,0,1)); // Clear translation
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		Matrix4f inv;
		float det;
		testAssert(m.getUpperLeftInverse(inv, det));
		testAssert(det == m.upperLeftDeterminant());
		m.setColumn(3, Vec4f(0,0,0,1)); // Clear translation
		testAssert(isInverse(m, inv));
	}

	//-------------------------- Test getInverseForAffine3Matrix ----------------------------
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f);
		m.e[12] = 1.f;
		m.e[13] = 2.f;
		m.e[14] = 3.f;

		Matrix4f inv;
		testAssert(m.getInverseForAffine3Matrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		Matrix4f inv;
		testAssert(m.getInverseForAffine3Matrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		m.e[12] = 1.f;
		m.e[13] = 2.f;
		m.e[14] = 3.f;
		Matrix4f inv;
		testAssert(m.getInverseForAffine3Matrix(inv));
		testAssert(isInverse(m, inv));
	}
	{
		Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(-0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		m.e[12] = 100.f;
		m.e[13] = 200.f;
		m.e[14] = 300.f;
		Matrix4f inv;
		testAssert(m.getInverseForAffine3Matrix(inv));
		testAssert(isInverse(m, inv));
	}

	//-------------------------- Test getUpperLeftInverseTranspose ----------------------------
	{
		const Matrix4f R = Matrix4f::rotationMatrix(normalise(Vec4f(0.5f, 0.6f, 0.7f, 0)), 0.3f);
		Matrix4f m = R;
		m.e[12] = 1.f; // Give translation as well
		m.e[13] = 2.f;
		m.e[14] = 3.f;

		Matrix4f upper_left_inv_T;
		testAssert(m.getUpperLeftInverseTranspose(upper_left_inv_T));

		Matrix4f upper_left_inv;
		upper_left_inv_T.getTranspose(upper_left_inv);

		testAssert(isInverse(R, upper_left_inv));
	}

	//{
	//	const float e[16] = {
	//		0.949394524f, 0.31114384f, 0.0428893045f, 0.000000000f, 
	//		-0.314085960f, 0.940527320f, 0.129453972f, 0.000000000f,
	//		-5.97569488e-005f, -0.136373818f, 0.990657449f, 0.000000000f,
	//		//0, 0, 0, 1.00000000f};
	//		-18.0982876f, -109.567505f, -111.967888f, 1.00000000f};

	//	Matrix4f m(e);
	//	Matrix4f inv;
	//	testAssert(m.getInverseForAffine3Matrix(inv));
	//	testAssert(isInverse(m, inv));
	//}

	// Test setToRotationMatrix
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0,0,1,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(1,0,0,0), Vec4f(0,1,0,0)));
		testAssert(epsEqual(m * Vec4f(0,1,0,0), Vec4f(-1,0,0,0)));
	}
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(1,0,0,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(0,1,0,0), Vec4f(0,0,1,0)));
		testAssert(epsEqual(m * Vec4f(0,0,1,0), Vec4f(0,-1,0,0)));
	}
	{
		Matrix4f m;
		m.setToRotationMatrix(normalise(Vec4f(0,1,0,0)), Maths::pi_2<float>());
		testAssert(epsEqual(m * Vec4f(1,0,0,0), Vec4f(0,0,-1,0)));
		testAssert(epsEqual(m * Vec4f(0,0,1,0), Vec4f(1,0,0,0)));
	}

	// Test rotationAroundXAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsEqual(Matrix4f::rotationAroundXAxis(theta), Matrix4f::rotationMatrix(Vec4f(1, 0, 0, 0), theta)));
	}
	// Test rotationAroundYAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsEqual(Matrix4f::rotationAroundYAxis(theta), Matrix4f::rotationMatrix(Vec4f(0, 1, 0, 0), theta)));
	}
	// Test rotationAroundZAxis
	{
		for(float theta = -10.0f; theta < 10.0f; theta += 0.1f)
			testAssert(epsEqual(Matrix4f::rotationAroundZAxis(theta), Matrix4f::rotationMatrix(Vec4f(0, 0, 1, 0), theta)));
	}

	//------------- Matrix4f::identity() ----------------
	{
		const Matrix4f m = Matrix4f::identity();

		const Vec4f v(1, 2, 3, 4);
		const Vec4f res = m * v;
		testAssert(epsEqual(v, res));
	}


	//------------- Matrix4f::setToIdentity() ----------------
	{
		Matrix4f m;
		m.setToIdentity();
		testAssert(epsEqual(m, Matrix4f::identity()));

		const Vec4f v(1, 2, 3, 4);
		const Vec4f res = m * v;
		testAssert(epsEqual(v, res));
	}

	

	//-------------------------- Test setToUniformScaleMatrix() ----------------------
	{
		Matrix4f m;
		m.setToUniformScaleMatrix(2.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(2, 4, 6, 4)));
	}

	//-------------------------- Test applyUniformScale() ----------------------
	{
		Matrix4f m = Matrix4f::identity();
		m.applyUniformScale(2.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(2, 4, 6, 4)));
	}
	{
		Matrix4f m = Matrix4f::identity();
		m.applyUniformScale(2.0f);
		m.applyUniformScale(3.0f);

		const Vec4f res = m * Vec4f(1, 2, 3, 4);
		testAssert(epsEqual(res, Vec4f(6, 12, 18, 4)));
	}


	{
		const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

		const Matrix4f m(e);
		/*
		0	4	8	12
		1	5	9	13
		2	6	10	14
		3	7	11	15
		*/

		//------------- test getTranspose() ----------------
		{
			Matrix4f transpose;
			m.getTranspose(transpose);

			const float transposed_e[16] = { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 };
			testAssert(transpose == Matrix4f(transposed_e));
		}
		{
			const Matrix4f transpose = m.getTranspose();

			const float transposed_e[16] = { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 };
			testAssert(transpose == Matrix4f(transposed_e));
		}

		//------------- test operator * (const Matrix4f& m, const Vec4f& v) ----------------
		{
			const Vec4f v(1, 0, 0, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(0,1,2,3)));
		}
		{
			const Vec4f v(0, 1, 0, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(4,5,6,7)));
		}
		{
			const Vec4f v(0, 0, 1, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(8,9,10,11)));
		}
		{
			const Vec4f v(0, 0, 0, 1);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(12,13,14,15)));
		}

		//------------- test mul3Vector -------------
		testAssert(epsEqual(m.mul3Vector(Vec4f(1,0,0,0)), Vec4f(0,1,2,3)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(1,0,0,1)), Vec4f(0,1,2,3)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,1,0,0)), Vec4f(4,5,6,7)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,1,0,1)), Vec4f(4,5,6,7)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,1,0)), Vec4f(8,9,10,11)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,1,1)), Vec4f(8,9,10,11)));
		testAssert(epsEqual(m.mul3Vector(Vec4f(0,0,0,1)), Vec4f(0,0,0,0)));

		//------------- test tranpose mult ----------------
		{
			const Vec4f v(1, 0, 0, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(0,4,8,12)));
		}
		{
			const Vec4f v(0, 1, 0, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(1,5,9,13)));
		}
		{
			const Vec4f v(0, 0, 1, 0);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(2,6,10,14)));
		}
		{
			const Vec4f v(0, 0, 0, 1);
			const Vec4f res(m.transposeMult(v));
			testAssert(epsEqual(res, Vec4f(3,7,11,15)));
		}
		{
			const Vec4f v(1, 2, 3, 4);
			const Vec4f res(m.transposeMult(v));
			const Matrix4f transpose = m.getTranspose();
			testAssert(epsEqual(res, Vec4f(transpose * v)));
		}

		//------------- test transposeMult3Vector -------------
		/*
		m =
		0	4	8	12
		1	5	9	13
		2	6	10	14
		3	7	11	15
		*/
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,0,0,0)), Vec4f(0,4,8,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,0,0,1)), Vec4f(0,4,8,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,1,0,0)), Vec4f(1,5,9,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,1,0,1)), Vec4f(1,5,9,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,1,0)), Vec4f(2,6,10,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,1,1)), Vec4f(2,6,10,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(0,0,0,1)), Vec4f(0,0,0,0)));
		testAssert(epsEqual(m.transposeMult3Vector(Vec4f(1,1,1,1)), Vec4f(3,15,27,0)));
	}
	{
		// Test affine transformation
		const Matrix4f m(Matrix3f::identity(), Vec3f(1, 2, 3));

		{
			const Vec4f v(5, 6, 7, 1);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(6, 8, 10, 1)));
		}
		{
			const Vec4f v(5, 6, 7, 0);
			const Vec4f res(m * v);
			testAssert(epsEqual(res, Vec4f(5, 6, 7, 0)));
		}
	}

	{
		// Test matrix multiplication
		const Matrix4f m = Matrix4f::identity();
		const Matrix4f m2 = Matrix4f::identity();

		Matrix4f product;
		mul(m2, m, product);
		testAssert(product == Matrix4f::identity());

		mul(m, m2, product);
		testAssert(product == Matrix4f::identity());
	}

	{
		// Test matrix multiplication
		const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
		const Matrix4f m(e);

		const float e2[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
		const Matrix4f m2(e2);

		{
			const Vec4f v(5, 6, 7, 1);
			const Vec4f res(m2 * Vec4f(m * v));

			Matrix4f product;
			mul(m2, m, product);

			const Vec4f res2(product * v);

			testAssert(epsEqual(res, res2));

			// Compute m * m2
			Matrix4f m_m2;
			mul(m, m2, m_m2);
			Matrix4f ref_m_m2;
			refMul(m, m2, ref_m_m2);

			testAssert(epsEqual(m_m2, ref_m_m2));

			Matrix4f m_m2_op_mul;
			m_m2_op_mul = m * m2;

			testAssert(epsEqual(m_m2_op_mul, ref_m_m2));

			// Compute m2 * m
			Matrix4f m2_m;
			mul(m2, m, m2_m);
			Matrix4f ref_m2_m;
			refMul(m2, m, ref_m2_m);

			testAssert(epsEqual(m2_m, ref_m2_m));

			Matrix4f m2_m_op_mul;
			m2_m_op_mul = m2 * m;

			testAssert(epsEqual(m2_m_op_mul, ref_m2_m));
		}
	}

	{
		// Test translation matrix
		Matrix4f m;
		m.setToTranslationMatrix(1, 2, 3);

		{
		const Vec4f v(10, 20, 30, 1.0);
		const Vec4f res(m * v);
		
		testAssert(epsEqual(res, Vec4f(11, 22, 33, 1.0f)));
		}
		{
		const Vec4f v(10, 20, 30, 0.0);
		const Vec4f res(m * v);
		
		testAssert(epsEqual(res, Vec4f(10, 20, 30, 0.0f)));
		}
	}

	//-------------------------- Test constructFromVector() ----------------------
	{
		{
			const Vec4f vec(0,0,1,0);
			Matrix4f m;
			m.constructFromVector(vec);
			testIsOrthogonal(m);

			const Vec4f M_k = m * Vec4f(0,0,1,0);
			testEpsEqual(M_k, vec);
		}

		{
			const Vec4f vec = normalise(Vec4f(0.2,1,1,0));
			Matrix4f m;
			m.constructFromVector(vec);
			testIsOrthogonal(m);

			const Vec4f M_k = m * Vec4f(0,0,1,0);
			testEpsEqual(M_k, vec);
		}

		{
			const Vec4f vec = normalise(Vec4f(1, 0.2, 1,0));
			Matrix4f m;
			m.constructFromVector(vec);
			testIsOrthogonal(m);

			const Vec4f M_k = m * Vec4f(0,0,1,0);
			testEpsEqual(M_k, vec);
		}
	}


	//-------------------------- Test constructFromVectorAndMul() ----------------------
	{
		{
			const Vec4f v = Vec4f(0.482021928f, -0.00631375983f, -0.876137257f, 0.000000000);
			//printVar(v.length());
			testConstructFromVectorAndMulForVec(v);
		}
		//testConstructFromVectorAndMulForVec(normalise(Vec4f(0.482021928f, -0.00631375983f, -0.876137257f, 0.000000000)));

		testConstructFromVectorAndMulForVec(Vec4f(1,0,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,1,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,1,0));
		testConstructFromVectorAndMulForVec(Vec4f(-1,0,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,-1,0,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-1,0));// This should trigger the special case in constructFromVectorAndMul().

		// More vectors around special case threshold (if(vec[2] > -0.999999f)):
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.999999f,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.9999991f,0));
		testConstructFromVectorAndMulForVec(Vec4f(0,0,-0.9999998f,0));

		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.9998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.99998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.999998f,0)));
		testConstructFromVectorAndMulForVec(normalise(Vec4f(0.00001f,0,-0.9999998f,0)));

		// Test around v.z = 0.999f
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.9989)), 0.f, 0.9989f, 0.f));
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.999)), 0.f, 0.999f, 0.f));
		testConstructFromVectorAndMulForVec(Vec4f((float)std::sin(std::acos(0.9991)), 0.f, 0.9991f, 0.f));

		for(float theta = Maths::pi<float>() - 0.01f; theta <= Maths::pi<float>(); theta += 0.00001f)
		{
			const Vec4f v = Vec4f(sin(theta), 0.f, cos(theta), 0.0f);
			testConstructFromVectorAndMulForVec(v);
		}
	}


	//-------------------------- Test polarDecomposition() ----------------------

	// Decompose a matrix with negative scale
	{
		Matrix4f m(Vec4f(-1, 0, 0, 0), Vec4f(0, 1, 0, 0), Vec4f(0, 0, 1, 0), Vec4f(0, 0, 0, 1));

		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		
		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsEqual(rot * rest, m, 1.0e-6f));
	}

	// Decompose the identity matrix
	{
		Matrix4f m = Matrix4f::identity();
		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsEqual(rot, Matrix4f::identity()));
		testAssert(epsEqual(rest, Matrix4f::identity()));
	}

	// Decompose a pure rotation matrix
	{
		Matrix4f m = Matrix4f::rotationMatrix(Vec4f(0, 0, 1, 0), 0.6f);
		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsEqual(rot, m));
		testAssert(epsEqual(rest, Matrix4f::identity()));
	}

	// Decompose a more complicated pure rotation matrix
	{
		Matrix4f m = Matrix4f::rotationMatrix(normalise(Vec4f(-0.5, 0.6, 1, 0)), 0.6f);
		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsEqual(rot, m));
		testAssert(epsEqual(rest, Matrix4f::identity()));
	}

	// Decompose a rotation combined with a uniform scale
	{
		Matrix4f rot_matrix = Matrix4f::rotationMatrix(normalise(Vec4f(-0.5, 0.6, 1, 0)), 0.6f);
		Matrix4f scale_matrix = Matrix4f::identity();
		scale_matrix.applyUniformScale(0.3f);

		{
		Matrix4f m = rot_matrix * scale_matrix;
		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsEqual(rot, rot_matrix));
		testAssert(epsEqual(rest, scale_matrix));

		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsEqual(rot * rest, m, 1.0e-6f));
		}

		// Try with the rot and scale concatenated in reverse order
		{
		Matrix4f m = scale_matrix * rot_matrix;
		Matrix4f rot, rest;
		testAssert(m.polarDecomposition(rot, rest));
		testAssert(epsEqual(rot, rot_matrix));
		testAssert(epsEqual(rest, scale_matrix));

		testAssert(isSpecialOrthogonal(rot));
		testAssert(epsEqual(rot * rest, m, 1.0e-6f));
		}
	}

	//---------------------------------- Test rightMultiplyAffine3WithTranslationMatrix() ---------------------
	{
		const Matrix4f m(Matrix3f(Vec3f(1.f, 0.2f, 0.1f), Vec3f(-0.2f, 2.0f, 0.35f), Vec3f(0.6f, 0.1f, 3.1f)), Vec3f(1.f, 4.f, 5.f));
		const Vec4f translation(30.f, -2.f, 5.f, 0);
		Matrix4f res;
		m.rightMultiplyAffine3WithTranslationMatrix(translation, res);
		testAssert(res == m * Matrix4f::translationMatrix(translation));
	}


	//=================================== Perf tests =====================================
	if(false)
	{
		// Test speed of getInverseForAffine3Matrix()
		{
			Timer timer;

			const float e[16] = { 1, 0, 0.5f, 0, 0, 1.3f, 0, 0, 0.2f, 0, 1, 0, 12, 13, 14, 1 };
			const Matrix4f m(e);

			int N = 1000000;
			float sum = 0.0f;
			for(int i=0; i<N; ++i)
			{
				Matrix4f res;
				m.getInverseForAffine3Matrix(res);
				sum += res.e[i % 16];
			}
			double elapsed = timer.elapsed();

			conPrint("getInverseForAffine3Matrix() time:    " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(sum));
		}

		// Test speed of getUpperLeftInverseTranspose()
		{
			Timer timer;

			const float e[16] = { 1, 0, 0.5f, 0, 0, 1.3f, 0, 0, 0.2f, 0, 1, 0, 12, 13, 14, 1 };
			const Matrix4f m(e);

			int N = 1000000;
			float sum = 0.0f;
			for(int i=0; i<N; ++i)
			{
				Matrix4f res;
				m.getUpperLeftInverseTranspose(res);
				sum += res.e[i % 16];
			}
			double elapsed = timer.elapsed();

			conPrint("getUpperLeftInverseTranspose() time:    " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(sum));
		}

		// Test speed of getTranspose()
		{
			Timer timer;

			const float e[16] = { 1, 0, 0.5f, 0, 0, 1.3f, 0, 0, 0.2f, 0, 1, 0, 12, 13, 14, 1 };
			const Matrix4f m(e);

			int N = 1000000;
			float sum = 0.0f;
			for(int i=0; i<N; ++i)
			{
				Matrix4f res;
				m.getTranspose(res);
				sum += res.e[i % 16];
			}
			double elapsed = timer.elapsed();

			conPrint("getTranspose() time:    " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(sum));
		}


		// Test speed of matrix-matrix mult
		{
			Timer timer;

			const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
			const Matrix4f m(e);
			const float e2[16] ={ 5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
			const Matrix4f m2(e2);

			int N = 1000000;
			float sum = 0.0f;
			for(int i=0; i<N; ++i)
			{
				Matrix4f res;
				mul(m, m2, res);
				sum += res.e[i % 16];
			}

			double elapsed = timer.elapsed();

			conPrint("mul time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(sum));
		}

		// Test speed of reference matrix-matrix mult
		{
			Timer timer;

			const float e[16] ={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
			const Matrix4f m(e);
			const float e2[16] ={ 5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
			const Matrix4f m2(e2);

			int N = 1000000;
			float sum = 0.0f;
			for(int i=0; i<N; ++i)
			{
				Matrix4f res;
				refMul(m, m2, res);
				sum += res.e[i % 16];
			}

			double elapsed = timer.elapsed();

			conPrint("ref mul time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(sum));
		}


		// Test speed of constructFromVector()
		{
			Timer timer;

			//const float e[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
			//const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);

				Matrix4f m;
				m.constructFromVector(v);
				sum += m * Vec4f(1, 0, 0, 0);
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("constructFromVector time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}

		// Test speed of constructFromVectorAndMul()
		{
			Timer timer;

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);

				const Vec4f res = Matrix4f::constructFromVectorAndMul(v, Vec4f(1,0,0,0));
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("constructFromVectorAndMul time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}


		// Test speed of transposeMult()
		{
			Timer timer;

			const float e[16] ={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				const Vec4f res = m.transposeMult(v);
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("transposeMult time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}


		// Test speed of matrix / vec mul
		{
			Timer timer;

			const float e[16] ={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				const Vec4f res = m * v;
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("mul time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}

		// Test speed of mul3Point()
		{
			Timer timer;

			const float e[16] ={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

			const Matrix4f m(e);

			int N = 1000000;
			Vec4f sum(0.0f);
			for(int i=0; i<N; ++i)
			{
				const Vec4f v((float)i, (float)i + 2, (float)i + 3, (float)i + 4);
				const Vec4f res = m.mul3Point(v);
				sum += res;
			}

			double elapsed = timer.elapsed();
			double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

			conPrint("mul3Point() time: " + ::toString(1.0e9 * elapsed / N) + " ns");
			TestUtils::silentPrint(::toString(scalarsum));
		}
	}
}


#endif // BUILD_TESTS
