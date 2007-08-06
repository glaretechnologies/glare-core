/*=====================================================================
PNGDecoder.h
------------
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PNGDECODER_H_666_
#define __PNGDECODER_H_666_


#include <vector>
class Bitmap;



/*=====================================================================
PNGDecoder
----------

=====================================================================*/
class PNGDecoder
{
public:
	/*=====================================================================
	PNGDecoder
	----------
	
	=====================================================================*/
	PNGDecoder();

	~PNGDecoder();


	//these throw ImFormatExcep
	static void decode(const std::vector<unsigned char>& encoded_img, Bitmap& bitmap_out);


	//static void encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out);


};



#endif //__PNGDECODER_H_666_




