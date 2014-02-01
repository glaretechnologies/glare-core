/*===================================================================
Copyright Glare Technologies Limited 2012 -
====================================================================*/
#pragma once


#include "platform.h"
#include "../utils/Vector.h"
#include <cassert>


/*=====================================================================
Array3D
-------

=====================================================================*/
template <class T>
class Array3D
{
public:
	Array3D() {}
	Array3D(unsigned int d0, unsigned int d1, unsigned int d2) : dx(d0), dy(d1), dz(d2), data(d0 * d1 * d2) {}

	~Array3D() {}


	INDIGO_STRONG_INLINE T& e(unsigned int x, unsigned int y, unsigned int z);
	INDIGO_STRONG_INLINE const T& e(unsigned int x, unsigned int y, unsigned int z) const;

	INDIGO_STRONG_INLINE const unsigned int dX() const { return dx; }
	INDIGO_STRONG_INLINE const unsigned int dY() const { return dy; }
	INDIGO_STRONG_INLINE const unsigned int dZ() const { return dz; }

	js::Vector<T, 16>& getData() { return data; }
	const js::Vector<T, 16>& getData() const { return data; }
private:
	js::Vector<T, 16> data;
	unsigned int dx, dy, dz;
};


template <class T> T& Array3D<T>::e(unsigned int x, unsigned int y, unsigned int z)
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	return data[z * dx * dy + y * dx + x];
}


template <class T> const T& Array3D<T>::e(unsigned int x, unsigned int y, unsigned int z) const
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	return data[z * dx * dy + y * dx + x];
}
