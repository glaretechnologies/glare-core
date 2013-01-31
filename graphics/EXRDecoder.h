/*=====================================================================
EXRDecoder.h
------------
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __EXRDECODER_H_666_
#define __EXRDECODER_H_666_


#include "ImageMap.h"
#include "Image4f.h"
#include <vector>
#include <map>
#include <string>
#include "../utils/Reference.h"
class Map2D;
class Bitmap;
class Image;


/*=====================================================================
EXRDecoder
----------

=====================================================================*/
class EXRDecoder
{
public:
	/*=====================================================================
	EXRDecoder
	----------
	
	=====================================================================*/
	EXRDecoder();

	~EXRDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);


	// throws Indigo::Exception
	static void saveImageTo32BitEXR(const Image& image, const std::string& pathname);
	static void saveImageTo32BitEXR(const Image4f& image, const std::string& pathname);
	static void saveImageTo32BitEXR(const ImageMapFloat& image, const std::string& pathname);
};



#endif //__EXRDECODER_H_666_




