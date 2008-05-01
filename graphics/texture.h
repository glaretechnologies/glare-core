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


/*=====================================================================
Texture
-------

=====================================================================*/
class Texture : public Bitmap
{
public:
	/*=====================================================================
	Texture
	-------
	
	=====================================================================*/
	Texture();

	~Texture();

	// X and Y are normalised image coordinates.  col_out components will be in range [0, 1]
	void sampleTiled(double x, double y, Colour3d& col_out) const;

private:
	void sampleTiled3BytesPP(double x, double y, Colour3d& col_out) const;
	void sampleTiled1BytePP(double x, double y, Colour3d& col_out) const;
};


#endif //__TEXTURE_H_666_




