/*=====================================================================
PaddedVec3.h
------------
File created by ClassTemplate on Sun Jun 26 00:54:46 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PADDEDVEC3_H_666_
#define __PADDEDVEC3_H_666_



#include "vec3.h"

/*=====================================================================
PaddedVec3
----------
Should be 16 bytes all up.
=====================================================================*/
class PaddedVec3 : public Vec3f
{
public:
	/*=====================================================================
	PaddedVec3
	----------
	
	=====================================================================*/
	inline/*__forceinline TEMP NO FORCE INLINE*/ PaddedVec3();
	inline PaddedVec3(float x, float y, float z);
	inline PaddedVec3(const Vec3f& vec3);

	inline ~PaddedVec3(){}

	inline PaddedVec3& operator = (const Vec3f& vec3);


	float padding;

};

PaddedVec3::PaddedVec3()
{
	//using zero is a bad idea because the recip will be an INF special
	padding = 1.0f;
}

PaddedVec3::PaddedVec3(float x_, float y_, float z_)
//TEMP:	Vec3(x, y, z)
{
	//TEMP:
	x = x_;
	y = y_;
	z = z_;

	padding = 1.0f;
}

PaddedVec3::PaddedVec3(const Vec3f& vec3)
//TEMP:	Vec3(vec3)
{
	x = vec3.x;
	y = vec3.y;
	z = vec3.z;
	padding = 1.0f;
}

PaddedVec3& PaddedVec3::operator = (const Vec3f& vec3)
{
	x = vec3.x;
	y = vec3.y;
	z = vec3.z;
	return *this;
}


inline bool epsEqual(const PaddedVec3& a, const PaddedVec3& b, float eps = (float)NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}


#endif //__PADDEDVEC3_H_666_



























