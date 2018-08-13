/*=====================================================================
tgadecoder.h
------------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Mon Oct 04 23:30:26 2004
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include <vector>
#include <string>
class Map2D;
class Bitmap;


/*=====================================================================
TGADecoder
----------
See http://www.gamers.org/dEngine/quake3/TGA.txt
=====================================================================*/
class TGADecoder
{
public:
	TGADecoder();
	~TGADecoder();

	// throws ImFormatExcep
	static Reference<Map2D> decode(const std::string& path);

	static void test();
};

