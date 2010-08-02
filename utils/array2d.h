/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __ARRAY2D__
#define __ARRAY2D__

#include "../maths/SSE.h" // for _mm_malloc
#include "../maths/mathstypes.h" // for myMin
#include <assert.h>
#include <stdlib.h> // for NULL


template <class Field>
class Array2d
{
public:
	inline Array2d();
	inline Array2d(size_t dim1, size_t dim2);
	inline Array2d(const Array2d& rhs);
	inline ~Array2d();

	inline Field& elem(unsigned int dim1, unsigned int dim2);
	inline const Field& elem(unsigned int dim1, unsigned int dim2) const;

	Array2d& operator = (const Array2d& rhs);
	bool operator == (const Array2d& rhs) const;

	inline void setAllElems(const Field& newval);

	void resize(size_t newdim1, size_t newdim2);
	inline void checkResize(size_t xindex, size_t yindex);

	inline size_t getWidth()  const { return dim1; }
	inline size_t getHeight() const { return dim2; }

	inline const Field* getData() const { return data; }
	inline Field* getData() { return data; }

	inline Field* rowBegin(unsigned int y);
	inline Field* rowEnd(unsigned int y);

	inline const Field* rowBegin(unsigned int y) const;
	inline const Field* rowEnd(unsigned int y) const;

private:
	void resizeAndScrapData(size_t newdim1, size_t newdim2);

	Field* data;
	size_t dim1;
	size_t dim2;
};



template <class Field>
Array2d<Field>::Array2d()
:	data(NULL),
	dim1(0),
	dim2(0)
{
}


template <class Field>
Array2d<Field>::Array2d(size_t dim1_, size_t dim2_)
:	data(NULL),
	dim1(dim1_),
	dim2(dim2_)
{
	//data = new Field[dim1 * dim2];
	data = (Field *)_mm_malloc(dim1 * dim2 * sizeof(Field), 64);
}


template <class Field>
Array2d<Field>::Array2d(const Array2d& rhs)
:	dim1(rhs.dim1),
	dim2(rhs.dim2)
{
	const size_t num_elems = dim1 * dim2;

	//data = new Field[num_elems];
	data = (Field *)_mm_malloc(num_elems * sizeof(Field), 64);

	for(size_t i = 0; i < num_elems; ++i)
		data[i] = rhs.data[i];
}


template <class Field>
Array2d<Field>::~Array2d()
{
	//delete[] data;
	_mm_free(data);
}


template <class Field>
Field& Array2d<Field>::elem(unsigned int x, unsigned int y)
{
	assert(x < dim1 && y < dim2);
	return data[y * dim1 + x];
}


template <class Field>
const Field& Array2d<Field>::elem(unsigned int x, unsigned int y) const
{
	assert(x < dim1 && y < dim2);
	return data[y * dim1 + x];
}


template <class Field>
Array2d<Field>& Array2d<Field>::operator = (const Array2d<Field>& rhs)
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
bool Array2d<Field>::operator == (const Array2d<Field>& rhs) const
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
void Array2d<Field>::setAllElems(const Field& newval)
{
	const size_t num_elems = dim1 * dim2;
	for(size_t i = 0; i < num_elems; ++i)
		data[i] = newval;
}


template <class Field>
void Array2d<Field>::resize(size_t newdim1, size_t newdim2)
{
	//Field* newdata = new Field[newdim1 * newdim2];
	Field* newdata = (Field*)_mm_malloc(newdim1 * newdim2 * sizeof(Field), 64);

	const size_t minx = myMin(newdim1, dim1);
	const size_t miny = myMin(newdim2, dim2);

	for(size_t y = 0; y < miny; y++)
		for(size_t x = 0; x < minx; x++)
			newdata[x + y * newdim1] = data[x + y * dim1];

	//delete[] data;
	_mm_free(data);
	data = newdata;

	dim1 = newdim1;
	dim2 = newdim2;
}


template <class Field>
void Array2d<Field>::resizeAndScrapData(size_t newdim1, size_t newdim2)
{
	//delete[] data;
	//data = new Field[newdim1 * newdim2];
	_mm_free(data);
	data = (Field*)_mm_malloc(newdim1 * newdim2 * sizeof(Field), 64);

	dim1 = newdim1;
	dim2 = newdim2;
}


template <class Field>
void Array2d<Field>::checkResize(size_t xindex, size_t yindex)
{
	if(xindex >= dim1 || yindex >= dim2)
		resize( myMax(xindex + 1, dim1), myMax(yindex + 1, dim2) );
}


template <class Field>
Field* Array2d<Field>::rowBegin(unsigned int y)
{
	return data + (y * dim1);
}


template <class Field>
Field* Array2d<Field>::rowEnd(unsigned int y)
{
	return data + ((y + 1) * dim1);
}


template <class Field>
const Field* Array2d<Field>::rowBegin(unsigned int y) const
{
	return data + (y * dim1);
}


template <class Field>
const Field* Array2d<Field>::rowEnd(unsigned int y) const
{
	return data + ((y + 1) * dim1);
}


#endif //__ARRAY2D__
