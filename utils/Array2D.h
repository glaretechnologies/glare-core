/*=====================================================================
Array2D.h
---------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#pragma once


#include "AllocatorVector.h"
#include <assert.h>
#include <stdlib.h> // for NULL


template <class Field>
class Array2D
{
public:
	inline Array2D();
	inline Array2D(size_t dim1, size_t dim2);
	inline Array2D(const Array2D& rhs);
	inline ~Array2D();

	inline Field& elem(size_t dim1, size_t dim2);
	inline const Field& elem(size_t dim1, size_t dim2) const;

	Array2D& operator = (const Array2D& rhs);
	bool operator == (const Array2D& rhs) const;

	inline void setAllElems(const Field& newval);

	void resize(size_t newdim1, size_t newdim2); // Resize, copying any existing data to new data array.
	void resizeNoCopy(size_t newdim1, size_t newdim2); // Resize without copying exising data.

	inline size_t getWidth()  const { return dim1; }
	inline size_t getHeight() const { return dim2; }

	inline const Field* getData() const { return data.data(); }
	inline Field* getData() { return data.data(); }

	inline Field* rowBegin(size_t y);
	inline Field* rowEnd(size_t y);

	inline const Field* rowBegin(size_t y) const;
	inline const Field* rowEnd(size_t y) const;

	void getTranspose(Array2D<Field>& transpose_out) const;

	void setAllocator(const Reference<glare::Allocator>& al) { data.setAllocator(al); }
	Reference<glare::Allocator>& getAllocator() { return data.getAllocator(); }

private:
	glare::AllocatorVector<Field, 64> data;
	size_t dim1;
	size_t dim2;
};



template <class Field>
Array2D<Field>::Array2D()
:	dim1(0),
	dim2(0)
{
}


template <class Field>
Array2D<Field>::Array2D(size_t dim1_, size_t dim2_)
:	dim1(dim1_),
	dim2(dim2_),
	data(dim1_ * dim2_)
{
}


template <class Field>
Array2D<Field>::Array2D(const Array2D& rhs)
:	dim1(rhs.dim1),
	dim2(rhs.dim2),
	data(rhs.data)
{
}


template <class Field>
Array2D<Field>::~Array2D()
{
}


template <class Field>
Field& Array2D<Field>::elem(size_t x, size_t y)
{
	assert(x < dim1 && y < dim2);
	return data[y * dim1 + x];
}


template <class Field>
const Field& Array2D<Field>::elem(size_t x, size_t y) const
{
	assert(x < dim1 && y < dim2);
	return data[y * dim1 + x];
}


template <class Field>
Array2D<Field>& Array2D<Field>::operator = (const Array2D<Field>& rhs)
{
	if(this == &rhs) return *this;

	dim1 = rhs.dim1;
	dim2 = rhs.dim2;
	data = rhs.data;
	
	return *this;
}


template <class Field>
bool Array2D<Field>::operator == (const Array2D<Field>& rhs) const
{
	if(rhs.dim1 != dim1 || rhs.dim2 != dim2)
		return false;

	const size_t num_elems = dim1 * dim2;
	for(size_t i = 0; i < num_elems; ++i)
		if(data[i] != rhs.data[i])
			return false;

	return true;
}


template <class Field>
void Array2D<Field>::setAllElems(const Field& newval)
{
	const size_t num_elems = dim1 * dim2;
	for(size_t i = 0; i < num_elems; ++i)
		data[i] = newval;
}


template <class Field>
void Array2D<Field>::resize(size_t newdim1, size_t newdim2)
{
	if(dim1 == 0 && dim2 == 0)
	{
		// Existing array is empty, so can just allocate new array directly.
		data.resize(newdim1 * newdim2);
		dim1 = newdim1;
		dim2 = newdim2;
	}
	else
	{
		glare::AllocatorVector<Field, 64> newdata;
		newdata.setAllocator(getAllocator());
		newdata.resize(newdim1 * newdim2);

		const size_t minx = myMin(newdim1, dim1);
		const size_t miny = myMin(newdim2, dim2);

		for(size_t y = 0; y < miny; y++)
			for(size_t x = 0; x < minx; x++)
				newdata[x + y * newdim1] = data[x + y * dim1];

		data = newdata;
		dim1 = newdim1;
		dim2 = newdim2;
	}
}


template <class Field>
void Array2D<Field>::resizeNoCopy(size_t newdim1, size_t newdim2) // Resize without copying exising data
{
	data.resizeNoCopy(newdim1 * newdim2);
	dim1 = newdim1;
	dim2 = newdim2;
}


template <class Field>
Field* Array2D<Field>::rowBegin(size_t y)
{
	return &data[y * dim1];
}


template <class Field>
Field* Array2D<Field>::rowEnd(size_t y)
{
	return &data[(y + 1) * dim1];
}


template <class Field>
const Field* Array2D<Field>::rowBegin(size_t y) const
{
	return &data[y * dim1];
}


template <class Field>
const Field* Array2D<Field>::rowEnd(size_t y) const
{
	return &data[(y + 1) * dim1];
}


template <class Field>
void Array2D<Field>::getTranspose(Array2D<Field>& transpose_out) const
{
	transpose_out.resize(getHeight(), getWidth());

	for(size_t y=0; y<getHeight(); ++y)
		for(size_t x=0; x<getWidth(); ++x)
			transpose_out.elem(y, x) = elem(x, y);
}
