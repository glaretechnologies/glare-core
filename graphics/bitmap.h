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
	
	Bitmap();

	/*=====================================================================
	Bitmap
	------
	srcdata may be NULL.
	=====================================================================*/
	Bitmap(unsigned int width, unsigned int height, unsigned int bytespp, const unsigned char* srcdata);

	~Bitmap();


	void takePointer(unsigned int width, unsigned int height, unsigned int bytespp, unsigned char* srcdata);

	const unsigned char* getData() const { return data; }
	unsigned char* getData(){ return data; }

	const unsigned int getWidth() const { return width; }
	const unsigned int getHeight() const { return height; }
	const unsigned int getBytesPP() const { return bytespp; }

	inline unsigned char* getPixel(unsigned int x, unsigned int y);
	inline const unsigned char* getPixel(unsigned int x, unsigned int y) const;

	void setBytesPP(const unsigned int new_bytes_pp);

	void resize(unsigned int newwidth, unsigned int newheight);

	void raiseToPower(float exponent);

	unsigned int checksum() const;


private:
	unsigned char* data;
	unsigned int width;
	unsigned int height;
	unsigned int bytespp;
};



unsigned char* Bitmap::getPixel(unsigned int x, unsigned int y)
{
	assert(x >= 0 && x < width);
	assert(y >= 0 && y < height);

	return data + (y*width + x) * bytespp;
}
const unsigned char* Bitmap::getPixel(unsigned int x, unsigned int y) const
{
	assert(x >= 0 && x < width);
	assert(y >= 0 && y < height);

	return data + (y*width + x) * bytespp;
}



#endif //__BITMAP_H_666_




