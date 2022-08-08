/*===================================================================
Array3D.h
---------
Copyright Glare Technologies Limited 2020 -
====================================================================*/
#pragma once


#include "Platform.h"
#include "Vector.h"
#include <cassert>


/*=====================================================================
Array3D
-------

=====================================================================*/
template <class T>
class Array3D
{
public:
	Array3D() : dx(0), dx_times_dy(0), dy(0), dz(0) {}
	Array3D(size_t d0, size_t d1, size_t d2) : dx(d0), dx_times_dy(d0 * d1), dy(d1), dz(d2), data(d0 * d1 * d2) {}
	Array3D(size_t d0, size_t d1, size_t d2, const T& val) : dx(d0), dx_times_dy(d0 * d1), dy(d1), dz(d2), data(d0 * d1 * d2, val) {}

	inline void resizeNoCopy(uint32 d0, uint32 d1, uint32 d2) { dx = d0; dx_times_dy = d0 * d1; dy = d1; dz = d2; data.resizeNoCopy(d0 * d1 * d2); }

	GLARE_STRONG_INLINE       T& e(size_t x, size_t y, size_t z);
	GLARE_STRONG_INLINE const T& e(size_t x, size_t y, size_t z) const;

	GLARE_STRONG_INLINE       T& elem(size_t x, size_t y, size_t z)       { return e(x, y, z); }
	GLARE_STRONG_INLINE const T& elem(size_t x, size_t y, size_t z) const { return e(x, y, z); }

	GLARE_STRONG_INLINE size_t dX() const { return dx; }
	GLARE_STRONG_INLINE size_t dY() const { return dy; }
	GLARE_STRONG_INLINE size_t dZ() const { return dz; }

	js::Vector<T, 16>& getData() { return data; }
	const js::Vector<T, 16>& getData() const { return data; }
private:
	js::Vector<T, 16> data;
	size_t dx, dx_times_dy, dy, dz;
};


template <class T> T& Array3D<T>::e(size_t x, size_t y, size_t z)
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	return data[z * dx_times_dy + y * dx + x];
}


template <class T> const T& Array3D<T>::e(size_t x, size_t y, size_t z) const
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	return data[z * dx_times_dy + y * dx + x];
}
