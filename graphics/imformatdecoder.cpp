/*=====================================================================
imformatdecoder.cpp
-------------------
File created by ClassTemplate on Sat Apr 27 16:12:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "imformatdecoder.h"


#include "../utils/stringutils.h"
#include <stdlib.h>//for NULL
#include "bitmap.h"
#include <fstream>
#include "../maths/mathstypes.h"
#include "jpegdecoder.h"
#include "tgadecoder.h"
#include "bmpdecoder.h"
#include "PNGDecoder.h"
#include "TIFFDecoder.h"
#include "EXRDecoder.h"
#include "FloatDecoder.h"
#include "../graphics/Map2D.h"


ImFormatDecoder::ImFormatDecoder()
{
	
}


ImFormatDecoder::~ImFormatDecoder()
{
	
}


Reference<Map2D> ImFormatDecoder::decodeImage(const std::string& path) // throws ImFormatExcep on failure
{
	if(hasExtension(path, "jpg") || hasExtension(path, "jpeg"))
	{
		return JPEGDecoder::decode(path);
	}
	else if(hasExtension(path, "tga"))
	{
		return TGADecoder::decode(path);
	}
	else if(hasExtension(path, "bmp"))
	{
		return BMPDecoder::decode(path);
	}
	else if(hasExtension(path, "png"))
	{
		return PNGDecoder::decode(path);
	}
	else if(hasExtension(path, "tif") || hasExtension(path, "tiff"))
	{
		return TIFFDecoder::decode(path);
	}
	else if(hasExtension(path, "exr"))
	{
		return EXRDecoder::decode(path);
	}
	else if(hasExtension(path, "float"))
	{
		return FloatDecoder::decode(path);
	}
	else
	{
		throw ImFormatExcep("unhandled format ('" + path + "'");
	}

	/*if(hasExtension(path, "jpg") || hasExtension(path, "jpeg"))
	{
		JPEGDecoder::decode(path, bitmap_out);
	}
	else if(hasExtension(path, "tga"))
	{
		TGADecoder::decode(path, bitmap_out);
	}
	else if(hasExtension(path, "bmp"))
	{
		BMPDecoder::decode(path, bitmap_out);
	}
	else if(hasExtension(path, "png"))
	{
		PNGDecoder::decode(path, bitmap_out);
	}
	else if(hasExtension(path, "tif") || hasExtension(path, "tiff"))
	{
		TIFFDecoder::decode(path, bitmap_out);
	}
	else
	{
		throw ImFormatExcep("unhandled format ('" + path + "'");
	}*/
}

