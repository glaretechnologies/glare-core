/*=====================================================================
bmpdecoder.h
------------
File created by ClassTemplate on Mon May 02 22:00:30 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BMPDECODER_H_666_
#define __BMPDECODER_H_666_



#include <vector>
class Bitmap;

/*=====================================================================
BMPDecoder
----------

=====================================================================*/
class BMPDecoder
{
public:
	/*=====================================================================
	BMPDecoder
	----------
	
	=====================================================================*/

	~BMPDecoder();

	//these throw ImFormatExcep
	static void decode(const std::string& path, Bitmap& bitmap_out);


	static void encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out);

private:
	BMPDecoder();

};



#endif //__BMPDECODER_H_666_




