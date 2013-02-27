/*=====================================================================
RGBEDecoder.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at Tue Feb 26 22:12:22 +0100 2013
=====================================================================*/
#pragma once


#include <string>
#include "../utils/Reference.h"
#include "../utils/FileHandle.h"
class Map2D;


/*=====================================================================
RGBEHeaderData
-------------------
The header data parsed by the decoder.
=====================================================================*/
class RGBEHeaderData
{
public:
	RGBEHeaderData();

	std::string format;
	float exposure;
	float gamma;
	int width;
	int height;
};


/*=====================================================================
RGBEDecoder
-------------------
Decodes RGBE (.hdr) files.
=====================================================================*/
class RGBEDecoder
{
public:
	RGBEDecoder();
	~RGBEDecoder();

	// throws ImFormatExcep on failure
	static Reference<Map2D> decode(const std::string& path);

	static void test();

private:
	static const RGBEHeaderData parseHeaderString(const std::string& header); // throws ImFormatExcep
	static void readHeaderString(FileHandle& file, std::string& s_out); // throws ImFormatExcep

};



