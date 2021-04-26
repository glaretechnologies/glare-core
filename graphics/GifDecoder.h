/*=====================================================================
GifDecoder.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/ThreadSafeRefCounted.h"
#include <string>
#include <vector>
class Map2D;


/*=====================================================================
GIFDecoder
-------------------

=====================================================================*/
class GIFDecoder
{
public:
	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static Reference<Map2D> decodeImageSequence(const std::string& path);

	static void test();
private:

};
