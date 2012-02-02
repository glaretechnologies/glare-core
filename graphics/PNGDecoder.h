/*=====================================================================
PNGDecoder.h
------------
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PNGDECODER_H_666_
#define __PNGDECODER_H_666_


#include <vector>
#include <map>
#include <string>
#include "../utils/reference.h"
class Map2D;
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
	static Reference<Map2D> decode(const std::string& path);
	
	static void decode(const std::string& path, Bitmap& bitmap_out);

	// throws ImFormatExcep
	static void write(const Bitmap& bitmap, const std::map<std::string, std::string>& metadata, const std::string& path);
	static void write(const Bitmap& bitmap, const std::string& path); // Write with no metadata

	//static void encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out);

	static const std::map<std::string, std::string> getMetaData(const std::string& image_path);
};



#endif //__PNGDECODER_H_666_




