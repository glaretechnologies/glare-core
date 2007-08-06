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

#include "../maths/mathstypes.h"//for myMin
#include <algorithm>//for max
#include <assert.h>

#include <stdlib.h>//for NULL

//NOTE: optimise me

template <class Field>
class Array2d
{
public:
	inline Array2d();
	inline Array2d(int dim1, int dim2);
	inline Array2d(const Array2d& rhs);
	inline ~Array2d();

	inline Field& elem(int dim1, int dim2);
	inline const Field& elem(int dim1, int dim2) const;

	Array2d& operator = (const Array2d& rhs);
	bool operator == (const Array2d& rhs) const;

	void resize(int newdim1, int newdim2);
	inline void checkResize(int xindex, int yindex);

	inline int getWidth() const { return dim1; }
	inline int getHeight() const { return dim2; }

	inline const Field* getData() const { return data; }
	inline Field* getData() { return data; }

private:
	void resizeAndScrapData(int newdim1, int newdim2);

	
	Field* data;
	int dim1;
	int dim2;
};












template <class Field>
Array2d<Field>::Array2d()
:	data(NULL)
{
	dim1 = 0;
	dim2 = 0;
}

template <class Field>
Array2d<Field>::Array2d(int dim1_, int dim2_)
:	data(NULL)
{
	dim1 = dim1_;
	dim2 = dim2_;

	data = new Field[dim1*dim2];
}

template <class Field>
Array2d<Field>::Array2d(const Array2d& rhs)
{
	dim1 = rhs.dim1;
	dim2 = rhs.dim2;

	data = new Field[dim1*dim2];

	for(int x=0; x<dim1; x++)
		for(int y=0; y<dim2; y++)
		{
			elem(x,y) = rhs.elem(x,y);
		}
}


template <class Field>
Array2d<Field>::~Array2d()
{
	delete[] data;
}

template <class Field>
Field& Array2d<Field>::elem(int x, int y)
{
	assert(x >= 0 && x < dim1 && y >= 0 && y < dim2);
	return data[x + y*dim1];
}

template <class Field>
const Field& Array2d<Field>::elem(int x, int y) const
{
	assert(x >= 0 && x < dim1 && y >= 0 && y < dim2);
	return data[x + y*dim1];
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

	for(int x=0; x<dim1; x++)
		for(int y=0; y<dim2; y++)
		{
			elem(x,y) = rhs.elem(x,y);
		}
	return *this;
}

template <class Field>
bool Array2d<Field>::operator == (const Array2d<Field>& rhs) const
{
	if(rhs.dim1 != dim1 || rhs.dim2 != dim2)
		return false;

	for(int x=0; x<dim1; x++)
		for(int y=0; y<dim2; y++)
		{
			if(elem(x,y) != rhs.elem(x,y))
				return false;
		}

	return true;
}




template <class Field>
void Array2d<Field>::resize(int newdim1, int newdim2)
{
	Field* newdata = new Field[newdim1*newdim2];

	const int minx = myMin(newdim1, dim1);
	const int miny = myMin(newdim2, dim2);


	for(int x=0; x<minx; x++)
		for(int y=0; y<miny; y++)
		{
			newdata[x + y*newdim1] = data[x + y*dim1];
		}


	delete[] data;
	data = newdata;

	dim1 = newdim1;
	dim2 = newdim2;
}



template <class Field>
void Array2d<Field>::resizeAndScrapData(int newdim1, int newdim2)
{
	/*Field* newdata = new Field[newdim1 * newdim2];
	delete[] data;
	data = newdata;*/

	delete[] data;
	data = new Field[newdim1 * newdim2];

	dim1 = newdim1;
	dim2 = newdim2;
}

template <class Field>
void Array2d<Field>::checkResize(int xindex, int yindex)
{
	if(xindex >= dim1 || yindex >= dim2)
	{
		//NOTE: check whats up with max()

		resize( myMax(xindex+1, dim1), myMax(yindex+1, dim2) );
	}
}




#endif //__ARRAY2D__
