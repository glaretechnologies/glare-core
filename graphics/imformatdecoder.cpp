/*=====================================================================
imformatdecoder.cpp
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "imformatdecoder.h"


#include "../utils/StringUtils.h"
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
#include "GifDecoder.h"
#include "RGBEDecoder.h"
#include "KTXDecoder.h"
#include "../graphics/Map2D.h"


Reference<Map2D> ImFormatDecoder::decodeImage(const std::string& indigo_base_dir, const std::string& path) // throws ImFormatExcep on failure
{
	if(hasExtension(path, "jpg") || hasExtension(path, "jpeg"))
	{
		return JPEGDecoder::decode(indigo_base_dir, path);
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
	else if(hasExtension(path, "gif"))
	{
		return GIFDecoder::decode(path);
	}
	else if(hasExtension(path, "hdr"))
	{
		return RGBEDecoder::decode(path);
	}
	else if(hasExtension(path, "ktx"))
	{
		return KTXDecoder::decode(path);
	}
	else if(hasExtension(path, "ktx2"))
	{
		return KTXDecoder::decodeKTX2(path);
	}
	else
	{
		throw ImFormatExcep("Unhandled image format ('" + getExtension(path) + "')");
	}
}
