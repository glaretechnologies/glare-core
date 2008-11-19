/*=====================================================================
texture.h
---------
File created by ClassTemplate on Mon May 02 21:31:01 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TEXTURE_H_666_
#define __TEXTURE_H_666_


#include "bitmap.h"
#include "colour3.h"
#include "Map2D.h"


/*=====================================================================
Texture
-------

=====================================================================*/
class Texture : public Bitmap, public Map2D
{
public:
	/*=====================================================================
	Texture
	-------
	
	=====================================================================*/
	Texture();

	~Texture();

	// X and Y are normalised image coordinates.  col_out components will be in range [0, 1]
	//void sampleTiled(double x, double y, Colour3d& col_out) const;

	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	virtual Value scalarSampleTiled(Coord x, Coord y) const;

	virtual unsigned int getWidth() const { return Bitmap::getWidth(); }
	virtual unsigned int getHeight() const  { return Bitmap::getHeight(); }

	static void test();

private:
	void sampleTiled3BytesPP(Coord x, Coord y, Colour3<Value>& col_out) const;
	Value sampleTiled1BytePP(Coord x, Coord y) const;
};


#endif //__TEXTURE_H_666_




