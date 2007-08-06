/*=====================================================================
bitmap.h
--------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BITMAP_H_666_
#define __BITMAP_H_666_


#include <assert.h>


/*=====================================================================
Bitmap
------
8bit/component bitmap image.
=====================================================================*/
class Bitmap
{
public:
	/*=====================================================================
	Bitmap
	------
	srcdata may be NULL.
	=====================================================================*/
	Bitmap();
	Bitmap(int width, int height, int bytespp, const unsigned char* srcdata);

	~Bitmap();


	void takePointer(int width, int height, int bytespp, unsigned char* srcdata);

	const unsigned char* getData() const { return data; }
	unsigned char* getData(){ return data; }

	const int getWidth() const { return width; }
	const int getHeight() const { return height; }
	const int getBytesPP() const { return bytespp; }

	inline unsigned char* getPixel(int x, int y);
	inline const unsigned char* getPixel(int x, int y) const;

	void setBytesPP(const int new_bytes_pp);

	void resize(int newwidth, int newheight);

	void raiseToPower(float exponent);


private:
	unsigned char* data;
	int width;
	int height;
	int bytespp;
};



unsigned char* Bitmap::getPixel(int x, int y)
{
	assert(x >= 0 && x < width);
	assert(y >= 0 && y < height);

	return data + (y*width + x) * bytespp;
}
const unsigned char* Bitmap::getPixel(int x, int y) const
{
	assert(x >= 0 && x < width);
	assert(y >= 0 && y < height);

	return data + (y*width + x) * bytespp;
}



#endif //__BITMAP_H_666_




