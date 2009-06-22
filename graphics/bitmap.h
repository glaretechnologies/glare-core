/*=====================================================================
bitmap.h
--------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BITMAP_H_666_
#define __BITMAP_H_666_


#include <assert.h>
#include <vector>

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

	void resize(unsigned int newwidth, unsigned int newheight, unsigned int new_bytes_pp);

	//const unsigned char* getData() const { return data; }
	//unsigned char* getData(){ return data; }

	unsigned int getWidth() const { return width; }
	unsigned int getHeight() const { return height; }
	unsigned int getBytesPP() const { return bytespp; }

	inline unsigned char* rowPointer(unsigned int y);

	inline unsigned char* getPixelNonConst(unsigned int x, unsigned int y);
	inline const unsigned char* getPixel(unsigned int x, unsigned int y) const;

	inline unsigned char getPixelComp(unsigned int x, unsigned y, unsigned int c) const;
	inline void setPixelComp(unsigned int x, unsigned y, unsigned int c, unsigned char newval);

	void raiseToPower(float exponent);

	unsigned int checksum() const;

	void addImage(const Bitmap& img, int destx, int desty);
	void blendImage(const Bitmap& img, int destx, int desty);

private:
	std::vector<unsigned char> data;
	unsigned int width;
	unsigned int height;
	unsigned int bytespp;
};



unsigned char* Bitmap::getPixelNonConst(unsigned int x, unsigned int y)
{
	assert(x < width);
	assert(y < height);
	assert((y*width + x) * bytespp < data.size());

	return &data[(y*width + x) * bytespp];
}

unsigned char* Bitmap::rowPointer(unsigned int y)
{
	assert(y < height);

	return &data[y * width * bytespp];
}

const unsigned char* Bitmap::getPixel(unsigned int x, unsigned int y) const
{
	assert(x < width);
	assert(y < height);
	assert((y*width + x) * bytespp < data.size());

	//return data + (y*width + x) * bytespp;
	return &data[(y*width + x) * bytespp];
}


unsigned char Bitmap::getPixelComp(unsigned int x, unsigned y, unsigned int c) const
{
	assert(x < width);
	assert(y < height);
	assert(c < bytespp);

	return data[(y*width + x) * bytespp + c];
}


void Bitmap::setPixelComp(unsigned int x, unsigned y, unsigned int c, unsigned char newval)
{
	assert(x < width);
	assert(y < height);
	assert(c < bytespp);

	data[(y*width + x) * bytespp + c] = newval;
}



#endif //__BITMAP_H_666_




