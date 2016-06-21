/*=====================================================================
TIFFDecoder.h
-------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Fri May 02 16:51:32 2008
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../graphics/ImageMap.h"
#include <string>
class Map2D;


/*=====================================================================
TIFFDecoder
-----------

=====================================================================*/
class TIFFDecoder
{
public:

	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);

	// throws ImFormatExcep
	template <class T, class Traits>
	static void write(const ImageMap<T, Traits>& bitmap, const std::string& path);


	// throws ImFormatExcep if could not open tiff for reading
	static bool isTiffCompressed(const std::string& path);


	static void test();
};
