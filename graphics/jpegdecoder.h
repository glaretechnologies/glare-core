/*=====================================================================
jpegdecoder.h
-------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/Reference.h"
#include <vector>
#include <string>
class Map2D;
class Bitmap;


/*=====================================================================
JPEGDecoder
-----------

=====================================================================*/
class JPEGDecoder
{
public:
	~JPEGDecoder();


	static Reference<Map2D> decode(const std::string& indigo_base_dir, const std::string& path);

	static void save(const Reference<ImageMapUInt8>& image, const std::string& path);


	static void test(const std::string& indigo_base_dir);
private:
	JPEGDecoder();
};
