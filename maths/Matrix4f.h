#ifndef INDIGO_MATRIX4F_H
#define INDIGO_MATRIX4F_H


#include "Vec4f.h"
#include "SSE.h"
// Template forwards declarations
template <class Real> class Matrix3;
template <class Real> class Vec3;


SSE_CLASS_ALIGN Matrix4f
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	inline Matrix4f() {}
	Matrix4f(const float* data);

	Matrix4f(const Matrix3<float>& upper_left_mat, const Vec3<float>& translation);

	inline void setToTranslationMatrix(float x, float y, float z);

	//const Matrix4f operator * (const Matrix4f& a) const;

	inline const Vec4f transposeMult(const Vec4f& v) const;

	inline void getTranspose(Matrix4f& transpose_out) const;

	//bool inverse(Matrix4f& inverse_out);

	void getUpperLeftMatrix(Matrix3<float>& upper_left_mat_out) const;


	inline void constructFromVector(const Vec4f& vec);

	inline bool operator == (const Matrix4f& a) const;


	
// Disable a bogus VS 2010 Code analysis warning: 'warning C6385: Invalid data: accessing 'e', the readable size is '64' bytes, but '76' bytes might be read'
#pragma warning(push)
#pragma warning(disable:6385)
	inline float elem(unsigned int row_index, unsigned int column_index) const { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }
	inline float& elem(unsigned int row_index, unsigned int column_index) { assert(row_index < 4 && column_index < 4); return e[row_index + column_index * 4]; }
#pragma warning(pop)

	inline void setColumn0(const Vec4f& c);
	inline void setColumn1(const Vec4f& c);
	inline void setColumn2(const Vec4f& c);
	inline void setColumn3(const Vec4f& c);


	// Is A the inverse of B?
	static bool isInverse(const Matrix4f& A, const Matrix4f& B);


	static const Matrix4f identity();

	//__forceinline const Vec4f operator * (const Vec4f& v) const;


	static void test();

	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/
	float e[16];


};


/*
transpose mult:
r.x = (e[0], e[1], e[2], e[3])(v.x
                              v.y
                              v.z
                              v.w)

r.x = e[0]*v.x +  e[1]*v.y +  e[2]*v.z +  e[3]*v.w
r.y = e[4]*v.x +  e[5]*v.y +  e[6]*v.z +  e[7]*v.w
r.z = e[8]*v.x +  e[9]*v.y +  e[10]*v.z + e[11]*v.w
r.w = e[12]*v.x + e[13]*v.y + e[14]*v.z + e[15]*v.w


  x		y	z	w
+ x		x	z	z	[shuffle]
= 2x	x+y	2z	w+z	[add]
+ x+y	x+y	w+z	w+z	[shuffle]
= x+y+z+w			[add]

initially:
e[0]	e[1]	e[2]	e[3]
e[4]	e[5]	e[6]	e[7]
e[8]	e[9]	e[10]	e[11]
e[12]	e[13]	e[14]	e[15]

shuffle:
e[0]	e[0]	e[8]	e[8]
e[1]	e[1]	e[9]	e[9]
e[2]	e[2]	e[10]	e[10]
e[3]	e[3]	e[14]	e[14]

shuffle:
e[4]	e[4]	e[12]	e[12]
e[5]	e[5]	e[13]	e[13]
e[6]	e[6]	e[14]	e[14]
e[7]	e[7]	e[14]	e[14]

shuffle:
e[0]	e[4]	e[8]	e[12]
e[1]	e[5]	e[9]	e[13]
e[2]	e[6]	e[10]	e[14]
e[3]	e[7]	e[11]	e[15]

x	y	z	w
x	y	z	w


r.x = e[0]*v.x +	e[4]*v.y +  e[8]*v.z +  e[12]*v.w
r.y = e[1]*v.x +	e[5]*v.y +  e[9]*v.z +  e[13]*v.w
r.z = e[2]*v.x +	e[6]*v.y +	e[10]*v.z + e[14]*v.w
r.w = e[3]*v.x +	e[7]*v.y +	e[11]*v.z + e[15]*v.w

*/

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
	/*const __m128 a = _mm_load_ps(e); //		[	e41	e31	e21	e11]
	const __m128 b = _mm_load_ps(e+4); //	[	e42	e32	e22	e12]
	const __m128 c = _mm_load_ps(e+8); //	[	e43	e33	e23	e13]
	const __m128 d = _mm_load_ps(e+12); //	[	e44	e34	e24	e14]*/

	// Want v.x * [	e14,	e13,	e12,	e11]

	//TEMP:
	return Vec4f(
		dot(Vec4f(_mm_load_ps(e)), v),
		dot(Vec4f(_mm_load_ps(e+4)), v),
		dot(Vec4f(_mm_load_ps(e+8)), v),
		dot(Vec4f(_mm_load_ps(e+12)), v)
		);
}


void Matrix4f::getTranspose(Matrix4f& transpose_out) const
{
	transpose_out.e[0] = e[0];
	transpose_out.e[1] = e[4];
	transpose_out.e[2] = e[8];
	transpose_out.e[3] = e[12];
	transpose_out.e[4] = e[1];
	transpose_out.e[5] = e[5];
	transpose_out.e[6] = e[9];
	transpose_out.e[7] = e[13];
	transpose_out.e[8] = e[2];
	transpose_out.e[9] = e[6];
	transpose_out.e[10] = e[10];
	transpose_out.e[11] = e[14];
	transpose_out.e[12] = e[3];
	transpose_out.e[13] = e[7];
	transpose_out.e[14] = e[11];
	transpose_out.e[15] = e[15];
}


void mul(const Matrix4f& a, const Matrix4f& b, Matrix4f& result_out);


/*
__forceinline void mul(const Matrix4f& m, const Vec4f& v, Vec4f& res_out)
{
	const __m128 vx = indigoCopyToAll(v.v, 0);
	const __m128 vy = indigoCopyToAll(v.v, 1);
	const __m128 vz = indigoCopyToAll(v.v, 2);
	const __m128 vw = indigoCopyToAll(v.v, 3);

	res_out = Vec4f(
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
*/

INDIGO_STRONG_INLINE __m128 operator * (const Matrix4f& m, const Vec4f& v)
{
	assert(SSE::isSSEAligned(&m));
	assert(SSE::isSSEAligned(&v));

	const __m128 vx = indigoCopyToAll(v.v, 0);
	const __m128 vy = indigoCopyToAll(v.v, 1);
	const __m128 vz = indigoCopyToAll(v.v, 2);
	const __m128 vw = indigoCopyToAll(v.v, 3);

	return
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
		;
}


// Sets to the matrix that will transform a vector from an orthonormal basis with k = vec, to parent space.
void Matrix4f::constructFromVector(const Vec4f& vec)
{
	assert(SSE::isSSEAligned(this));
	assert(SSE::isSSEAligned(&vec));
	assert(vec.isUnitLength());

	SSE_ALIGN Vec4f v2; // x axis

	// From PBR
	if(std::fabs(vec[0]) > std::fabs(vec[1]))
	{
		const float recip_len = 1.0f / std::sqrt(vec[0] * vec[0] + vec[2] * vec[2]);

		v2.set(-vec[2] * recip_len, 0.0f, vec[0] * recip_len, 0.0f);
	}
	else
	{
		const float recip_len = 1.0f / std::sqrt(vec[1] * vec[1] + vec[2] * vec[2]);

		v2.set(0.0f, vec[2] * recip_len, -vec[1] * recip_len, 0.0f);
	}

	assert(v2.isUnitLength());

	/*
	0	4	8	12
	1	5	9	13
	2	6	10	14
	3	7	11	15
	*/

	e[0] = v2[0];
	e[1] = v2[1];
	e[2] = v2[2];
	e[3] = v2[3];

	const SSE_ALIGN Vec4f v1(crossProduct(vec, v2));

	e[4] = v1[0];
	e[5] = v1[1];
	e[6] = v1[2];
	e[7] = v1[3];

	e[8] = vec[0];
	e[9] = vec[1];
	e[10] = vec[2];
	e[11] = vec[3];

	e[12] = 0.0f;
	e[13] = 0.0f;
	e[14] = 0.0f;
	e[15] = 1.0f;

	//mat.setColumn0(v2);
	//mat.setColumn1(::crossProduct(vec, v2));
	//mat.setColumn2(vec);

	//assert(::epsEqual(dot(mat.getColumn0(), mat.getColumn1()), (Real)0.0));
	//assert(::epsEqual(dot(mat.getColumn0(), mat.getColumn2()), (Real)0.0));
	//assert(::epsEqual(dot(mat.getColumn1(), mat.getColumn2()), (Real)0.0));
}


bool Matrix4f::operator == (const Matrix4f& a) const
{
	for(unsigned int i=0; i<16; ++i)
		if(e[i] != a.e[i])
			return false;
	return true;
}


void Matrix4f::setColumn0(const Vec4f& c)
{
	_mm_store_ps(e, c.v);
}


void Matrix4f::setColumn1(const Vec4f& c)
{
	_mm_store_ps(e + 4, c.v);
}


void Matrix4f::setColumn2(const Vec4f& c)
{
	_mm_store_ps(e + 8, c.v);
}


void Matrix4f::setColumn3(const Vec4f& c)
{
	_mm_store_ps(e + 12, c.v);
}


inline bool epsEqual(const Matrix4f& a, const Matrix4f& b, float eps = NICKMATHS_EPSILON)
{
	for(unsigned int i=0; i<16; ++i)
		if(!epsEqual(a.e[i], b.e[i], eps))
			return false;
	return true;
}


#endif // INDIGO_MATRIX4F_H
