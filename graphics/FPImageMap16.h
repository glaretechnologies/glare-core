/*=====================================================================
FPImageMap16.h
--------------
File created by ClassTemplate on Sun May 18 21:48:49 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FPIMAGEMAP16_H_666_
#define __FPIMAGEMAP16_H_666_


#include "Map2D.h"
#include <half.h>
//#include "../utils/array2d.h"
#include <vector>


/*=====================================================================
FPImageMap16
------------
Tri-component 16-bit floating point image.
=====================================================================*/
class FPImageMap16 : public Map2D
{
public:
	/*=====================================================================
	FPImageMap16
	------------
	
	=====================================================================*/
	FPImageMap16(unsigned int width, unsigned int height);

	~FPImageMap16();

	// X and Y are normalised image coordinates.
	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	virtual Value scalarSampleTiled(Coord x, Coord y) const;


	virtual unsigned int getWidth() const { return width; }
	virtual unsigned int getHeight() const { return height; }

	std::vector<half>& getData() { return data; }

private:
	inline const half* getPixel(unsigned int x, unsigned int y) const;

	unsigned int width, height;
	//Array2d<half> data;
	std::vector<half> data;
};


const half* FPImageMap16::getPixel(unsigned int x, unsigned int y) const
{
	return &data[(x + width * y) * 3];
}


#endif //__FPIMAGEMAP16_H_666_




