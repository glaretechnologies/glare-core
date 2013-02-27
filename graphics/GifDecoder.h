/*=====================================================================
GifDecoder.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-02-21 14:06:25 +0000
=====================================================================*/
#pragma once


#include <string>
#include "../utils/Reference.h"
class Map2D;


/*=====================================================================
GIFDecoder
-------------------

=====================================================================*/
class GIFDecoder
{
public:
	GIFDecoder();
	~GIFDecoder();


	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static void test();
private:

};

