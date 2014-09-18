/*=====================================================================
EXRDecoder.h
------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "Image4f.h"
#include "../utils/Reference.h"
#include <string>
class Map2D;
class Image;


/*=====================================================================
EXRDecoder
----------
Reading and writing of images in EXR format.
=====================================================================*/
class EXRDecoder
{
public:
	EXRDecoder();
	~EXRDecoder();


	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);


	// throws Indigo::Exception
	static void saveImageTo32BitEXR(const Image& image, const std::string& pathname);
	static void saveImageTo32BitEXR(const Image4f& image, bool save_alpha_channel, const std::string& pathname);
	static void saveImageTo32BitEXR(const ImageMapFloat& image, const std::string& pathname);
	
	static void test();

private:
	static void setEXRThreadPoolSize();
};
