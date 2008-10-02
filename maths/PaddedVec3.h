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
template <class T>
class PaddedVec3 : public Vec3<T>
{
public:
	/*=====================================================================
	PaddedVec3
	----------
	
	=====================================================================*/
	inline/*__forceinline TEMP NO FORCE INLINE*/ PaddedVec3();
	inline PaddedVec3(T x, T y, T z);
	inline PaddedVec3(const Vec3<T>& vec3);

	inline ~PaddedVec3(){}

	inline PaddedVec3& operator = (const Vec3<T>& vec3);

	static inline bool epsEqual(const PaddedVec3<T>& a, const PaddedVec3<T>& b, T eps = NICKMATHS_EPSILON);


	T padding;
};

template <class T>
PaddedVec3<T>::PaddedVec3()
{
	// Using zero is a bad idea because the recip will be an INF special
	padding = (T)1.0;
}

template <class T>
PaddedVec3<T>::PaddedVec3(T x_, T y_, T z_)
//TEMP:	Vec3(x, y, z)
{
	//TEMP:
	x = x_;
	y = y_;
	z = z_;

	padding = (T)1.0;
}

template <class T>
PaddedVec3<T>::PaddedVec3(const Vec3<T>& vec3)
//TEMP:	Vec3(vec3)
{
	x = vec3.x;
	y = vec3.y;
	z = vec3.z;

	padding = (T)1.0;
}

template <class T>
PaddedVec3<T>& PaddedVec3<T>::operator = (const Vec3<T>& vec3)
{
	x = vec3.x;
	y = vec3.y;
	z = vec3.z;
	return *this;
}

template <class T>
inline bool PaddedVec3<T>::epsEqual(const PaddedVec3<T>& a, const PaddedVec3<T>& b, T eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps);
}

typedef PaddedVec3<float> PaddedVec3f;
typedef PaddedVec3<double> PaddedVec3d;

#endif //__PADDEDVEC3_H_666_



























