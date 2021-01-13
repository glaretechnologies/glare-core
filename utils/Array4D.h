/*=====================================================================
Array4D.h
---------
File created by ClassTemplate on Fri May 22 15:53:01 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ARRAY4D_H_666_
#define __ARRAY4D_H_666_


#include "Platform.h"
#include <vector>
#include <cassert>


/*=====================================================================
Array4D
-------

=====================================================================*/
template <class T>
class Array4D
{
public:
	Array4D() {}
	Array4D(unsigned int d0, unsigned int d1, unsigned int d2, unsigned int d3) : dx(d0), dy(d1), dz(d2), dw(d3), data(d0 * d1 * d2 * d3) {}

	~Array4D() {}


	GLARE_STRONG_INLINE T& e(unsigned int x, unsigned int y, unsigned int z, unsigned int w);
	GLARE_STRONG_INLINE const T& e(unsigned int x, unsigned int y, unsigned int z, unsigned int w) const;

	GLARE_STRONG_INLINE const unsigned int dX() const { return dx; }
	GLARE_STRONG_INLINE const unsigned int dY() const { return dy; }
	GLARE_STRONG_INLINE const unsigned int dZ() const { return dz; }
	GLARE_STRONG_INLINE const unsigned int dW() const { return dw; }

	std::vector<T>& getData() { return data; }
	const std::vector<T>& getData() const { return data; }
private:
	unsigned int dx, dy, dz, dw;
	std::vector<T> data;
};


template <class T> T& Array4D<T>::e(unsigned int x, unsigned int y, unsigned int z, unsigned int w)
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	assert(w < dw);
	return data[x * dy * dz * dw + y * dz * dw + z * dw + w];
}


template <class T> const T& Array4D<T>::e(unsigned int x, unsigned int y, unsigned int z, unsigned int w) const
{
	assert(x < dx);
	assert(y < dy);
	assert(z < dz);
	assert(w < dw);
	return data[x * dy * dz * dw + y * dz * dw + z * dw + w];
}


#endif //__ARRAY4D_H_666_
