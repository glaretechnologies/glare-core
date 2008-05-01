/*=====================================================================
tgadecoder.h
------------
File created by ClassTemplate on Mon Oct 04 23:30:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TGADECODER_H_666_
#define __TGADECODER_H_666_



#include <vector>
class Bitmap;


/*=====================================================================
TGADecoder
----------

=====================================================================*/
class TGADecoder
{
public:
	/*=====================================================================
	TGADecoder
	----------
	
	=====================================================================*/
	~TGADecoder();

	//these throw ImFormatExcep
	static void decode(const std::string& path, Bitmap& bitmap_out);


	static void encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out);

private:
	TGADecoder();

};



#endif //__TGADECODER_H_666_




