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
#include <math.h>
#include "jpegdecoder.h"
#include "tgadecoder.h"
#include "bmpdecoder.h"
#include "PNGDecoder.h"
#include "TIFFDecoder.h"


ImFormatDecoder::ImFormatDecoder()
{
	
}


ImFormatDecoder::~ImFormatDecoder()
{
	
}


void ImFormatDecoder::decodeImage(const std::string& path, Bitmap& bitmap_out) // throws ImFormatExcep on failure
{
	if(hasExtension(path, "jpg") || hasExtension(path, "jpeg"))
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
	}
}

