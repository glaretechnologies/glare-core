/*=====================================================================
imformatdecoder.h
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../utils/Exception.h"
#include <string>
#include <vector>
class Map2D;


class ImFormatExcep : public glare::Exception
{
public:
	ImFormatExcep(const std::string& s_) : glare::Exception(s_) {}
	~ImFormatExcep(){}
};


/*=====================================================================
ImFormatDecoder
---------------

=====================================================================*/
class ImFormatDecoder
{
public:

	//static void decodeImage(const std::string& path, Bitmap& bitmap_out); // throws ImFormatExcep on failure

	static Reference<Map2D> decodeImage(const std::string& indigo_base_dir, const std::string& path);

	static bool hasImageExtension(const std::string& path);

private:
	ImFormatDecoder();
};
