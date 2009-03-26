#ifndef INDIGO_MATRIX4F_H
#define INDIGO_MATRIX4F_H


#include "Vec4f.h"


class Matrix4f
{



	const Vec4f operator * (const Vec4f& v) const;
	


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

*/
const Vec4f Matrix4f::operator * (const Vec4f& v) const
{
	return Vec4
}




#endif INDIGO_MATRIX4F_H
