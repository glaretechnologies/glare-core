#ifndef INDIGO_MATRIX4F_H
#define INDIGO_MATRIX4F_H


#include "Vec4f.h"
#include "SSE.h"


class Matrix4f
{
public:

	inline Matrix4f() {}
	Matrix4f(const float* data);

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

__forceinline __m128 operator * (const Matrix4f& m, const Vec4f& v)
{
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




#endif INDIGO_MATRIX4F_H
