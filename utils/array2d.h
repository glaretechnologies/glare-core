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

#include "../maths/mathstypes.h" // for myMin
#include <assert.h>
#include <stdlib.h> // for NULL


template <class Field>
class Array2d
{
public:
	inline Array2d();
	inline Array2d(unsigned int dim1, unsigned int dim2);
	inline Array2d(const Array2d& rhs);
	inline ~Array2d();

	inline Field& elem(unsigned int dim1, unsigned int dim2);
	inline const Field& elem(unsigned int dim1, unsigned int dim2) const;

	Array2d& operator = (const Array2d& rhs);
	bool operator == (const Array2d& rhs) const;

	inline void setAllElems(const Field& newval);

	void resize(unsigned int newdim1, unsigned int newdim2);
	inline void checkResize(unsigned int xindex, unsigned int yindex);

	inline unsigned int getWidth() const { return dim1; }
	inline unsigned int getHeight() const { return dim2; }

	inline const Field* getData() const { return data; }
	inline Field* getData() { return data; }

	inline Field* rowBegin(unsigned int y);
	inline Field* rowEnd(unsigned int y);

private:
	void resizeAndScrapData(unsigned int newdim1, unsigned int newdim2);

	Field* data;
	unsigned int dim1;
	unsigned int dim2;
};




template <class Field>
Array2d<Field>::Array2d()
:	data(NULL),
	dim1(0),
	dim2(0)
{
}

template <class Field>
Array2d<Field>::Array2d(unsigned int dim1_, unsigned int dim2_)
:	data(NULL),
	dim1(dim1_),
	dim2(dim2_)
{
	data = new Field[dim1 * dim2];
}

template <class Field>
Array2d<Field>::Array2d(const Array2d& rhs)
:	dim1(rhs.dim1),
	dim2(rhs.dim2)
{
	const unsigned int num_elems = dim1 * dim2;

	data = new Field[num_elems];

	for(unsigned int i=0; i<num_elems; ++i)
		data[i] = rhs.data[i];
}


template <class Field>
Array2d<Field>::~Array2d()
{
	delete[] data;
}

template <class Field>
Field& Array2d<Field>::elem(unsigned int x, unsigned int y)
{
	assert(x < dim1 && y < dim2);
	return data[y*dim1 + x];
}

template <class Field>
const Field& Array2d<Field>::elem(unsigned int x, unsigned int y) const
{
	assert(x < dim1 && y < dim2);
	return data[y*dim1 + x];
}


template <class Field>
Array2d<Field>& Array2d<Field>::operator = (const Array2d<Field>& rhs)
{
	if(this == &rhs)
		return *this;

	if(rhs.dim1 != dim1 || rhs.dim2 != dim2)
	{
		resizeAndScrapData(rhs.dim1, rhs.dim2);
	}

	assert(dim1 == rhs.dim1 && dim2 == rhs.dim2);

	const unsigned int num_elems = dim1 * dim2;
	for(unsigned int i=0; i<num_elems; ++i)
		data[i] = rhs.data[i];

	return *this;
}

template <class Field>
bool Array2d<Field>::operator == (const Array2d<Field>& rhs) const
{
	if(rhs.dim1 != dim1 || rhs.dim2 != dim2)
		return false;

	const unsigned int num_elems = dim1 * dim2;
	for(unsigned int i=0; i<num_elems; ++i)
		if(data[i] != rhs.data[i])
			return false;

	return true;
}

template <class Field>
void Array2d<Field>::setAllElems(const Field& newval)
{
	const unsigned int num_elems = dim1 * dim2;
	for(unsigned int i=0; i<num_elems; ++i)
		data[i] = newval;
}


template <class Field>
void Array2d<Field>::resize(unsigned int newdim1, unsigned int newdim2)
{
	Field* newdata = new Field[newdim1 * newdim2];

	const unsigned int minx = myMin(newdim1, dim1);
	const unsigned int miny = myMin(newdim2, dim2);

	for(unsigned int y=0; y<miny; y++)
		for(unsigned int x=0; x<minx; x++)	
			newdata[x + y*newdim1] = data[x + y*dim1];

	delete[] data;
	data = newdata;

	dim1 = newdim1;
	dim2 = newdim2;
}



template <class Field>
void Array2d<Field>::resizeAndScrapData(unsigned int newdim1, unsigned int newdim2)
{
	delete[] data;
	data = new Field[newdim1 * newdim2];

	dim1 = newdim1;
	dim2 = newdim2;
}

template <class Field>
void Array2d<Field>::checkResize(unsigned int xindex, unsigned int yindex)
{
	if(xindex >= dim1 || yindex >= dim2)
		resize( myMax(xindex+1, dim1), myMax(yindex+1, dim2) );
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


#endif //__ARRAY2D__
