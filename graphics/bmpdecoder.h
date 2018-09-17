/*=====================================================================
bmpdecoder.h
------------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Mon May 02 22:00:30 2005
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include <string>
#include <vector>
class Map2D;


/*=====================================================================
BMPDecoder
----------

=====================================================================*/
class BMPDecoder
{
public:
	// Throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static void test();
};
