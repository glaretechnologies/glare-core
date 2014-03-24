/*=====================================================================
Array2D.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#pragma once


#include "../maths/SSE.h" // for SSE::alignedMalloc etc..
#include "../maths/mathstypes.h" // for myMin
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

	void resize(size_t newdim1, size_t newdim2);
	inline void checkResize(size_t xindex, size_t yindex);

	inline size_t getWidth()  const { return dim1; }
	inline size_t getHeight() const { return dim2; }

	inline const Field* getData() const { return data; }
	inline Field* getData() { return data; }

	inline Field* rowBegin(size_t y);
	inline Field* rowEnd(size_t y);

	inline const Field* rowBegin(size_t y) const;
	inline const Field* rowEnd(size_t y) const;

	void getTranspose(Array2D<Field>& transpose_out) const;

private:
	void resizeAndScrapData(size_t newdim1, size_t newdim2);

	Field* data;
	size_t dim1;
	size_t dim2;
};



template <class Field>
Array2D<Field>::Array2D()
:	data(NULL),
	dim1(0),
	dim2(0)
{
}


template <class Field>
Array2D<Field>::Array2D(size_t dim1_, size_t dim2_)
:	data(NULL),
	dim1(dim1_),
	dim2(dim2_)
{
	//data = new Field[dim1 * dim2];
	data = (Field *)SSE::alignedMalloc(dim1 * dim2 * sizeof(Field), 64);
}


template <class Field>
Array2D<Field>::Array2D(const Array2D& rhs)
:	dim1(rhs.dim1),
	dim2(rhs.dim2)
{
	const size_t num_elems = dim1 * dim2;

	//data = new Field[num_elems];
	data = (Field *)SSE::alignedMalloc(num_elems * sizeof(Field), 64);

	for(size_t i = 0; i < num_elems; ++i)
		data[i] = rhs.data[i];
}


template <class Field>
Array2D<Field>::~Array2D()
{
	//delete[] data;
	SSE::alignedFree(data);
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

	if(rhs.dim1 != dim1 || rhs.dim2 != dim2) resizeAndScrapData(rhs.dim1, rhs.dim2);

	assert(dim1 == rhs.dim1 && dim2 == rhs.dim2);

	const size_t num_elems = dim1 * dim2;
	for(size_t i = 0; i < num_elems; ++i)
		data[i] = rhs.data[i];

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
	//Field* newdata = new Field[newdim1 * newdim2];
	Field* newdata = (Field*)SSE::alignedMalloc(newdim1 * newdim2 * sizeof(Field), 64);

	const size_t minx = myMin(newdim1, dim1);
	const size_t miny = myMin(newdim2, dim2);

	for(size_t y = 0; y < miny; y++)
		for(size_t x = 0; x < minx; x++)
			newdata[x + y * newdim1] = data[x + y * dim1];

	//delete[] data;
	SSE::alignedFree(data);
	data = newdata;

	dim1 = newdim1;
	dim2 = newdim2;
}


template <class Field>
void Array2D<Field>::resizeAndScrapData(size_t newdim1, size_t newdim2)
{
	//delete[] data;
	//data = new Field[newdim1 * newdim2];
	SSE::alignedFree(data);
	data = (Field*)SSE::alignedMalloc(newdim1 * newdim2 * sizeof(Field), 64);

	dim1 = newdim1;
	dim2 = newdim2;
}


template <class Field>
void Array2D<Field>::checkResize(size_t xindex, size_t yindex)
{
	if(xindex >= dim1 || yindex >= dim2)
		resize( myMax(xindex + 1, dim1), myMax(yindex + 1, dim2) );
}


template <class Field>
Field* Array2D<Field>::rowBegin(size_t y)
{
	return data + (y * dim1);
}


template <class Field>
Field* Array2D<Field>::rowEnd(size_t y)
{
	return data + ((y + 1) * dim1);
}


template <class Field>
const Field* Array2D<Field>::rowBegin(size_t y) const
{
	return data + (y * dim1);
}


template <class Field>
const Field* Array2D<Field>::rowEnd(size_t y) const
{
	return data + ((y + 1) * dim1);
}


template <class Field>
void Array2D<Field>::getTranspose(Array2D<Field>& transpose_out) const
{
	transpose_out.resize(getHeight(), getWidth());

	for(size_t y=0; y<getHeight(); ++y)
		for(size_t x=0; x<getWidth(); ++x)
			transpose_out.elem(y, x) = elem(x, y);
}
