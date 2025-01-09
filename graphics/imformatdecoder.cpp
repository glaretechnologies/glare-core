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
#ifndef NO_JPG_SUPPORT
#include "jpegdecoder.h"
#endif
#include "tgadecoder.h"
#include "bmpdecoder.h"
#include "PNGDecoder.h"
#include "TIFFDecoder.h"
#ifndef NO_EXR_SUPPORT
#include "EXRDecoder.h"
#endif
#include "FloatDecoder.h"
#ifndef NO_GIF_SUPPORT
#include "GifDecoder.h"
#endif
#include "RGBEDecoder.h"
#ifndef NO_KTX_SUPPORT
#include "KTXDecoder.h"
#endif
#ifndef NO_BASIS_SUPPORT
#include "BasisDecoder.h"
#endif
#include "../graphics/Map2D.h"


Reference<Map2D> ImFormatDecoder::decodeImage(const std::string& indigo_base_dir, const std::string& path) // throws ImFormatExcep on failure
{
	if(false)
	{}
#ifndef NO_JPG_SUPPORT
	else if(hasExtension(path, "jpg") || hasExtension(path, "jpeg"))
	{
		return JPEGDecoder::decode(indigo_base_dir, path);
	}
#endif
#if IS_INDIGO
	else if(hasExtension(path, "tga"))
	{
		return TGADecoder::decode(path);
	}
	else if(hasExtension(path, "bmp"))
	{
		return BMPDecoder::decode(path);
	}
	else if(hasExtension(path, "tif") || hasExtension(path, "tiff"))
	{
		return TIFFDecoder::decode(path);
	}
	else if(hasExtension(path, "float"))
	{
		return FloatDecoder::decode(path);
	}
	else if(hasExtension(path, "hdr"))
	{
		return RGBEDecoder::decode(path);
	}
#endif
	else if(hasExtension(path, "png"))
	{
		return PNGDecoder::decode(path);
	}
#ifndef NO_EXR_SUPPORT
	else if(hasExtension(path, "exr"))
	{
		return EXRDecoder::decode(path);
	}
#endif
#ifndef NO_GIF_SUPPORT
	else if(hasExtension(path, "gif"))
	{
		return GIFDecoder::decode(path);
	}
#endif
#ifndef NO_KTX_SUPPORT
	else if(hasExtension(path, "ktx"))
	{
		return KTXDecoder::decode(path);
	}
	else if(hasExtension(path, "ktx2"))
	{
		return KTXDecoder::decodeKTX2(path);
	}
#endif
#if !IS_INDIGO
#ifndef NO_BASIS_SUPPORT
	else if(hasExtension(path, "basis"))
	{
		return BasisDecoder::decode(path);
	}
#endif
#endif
	else
	{
		throw ImFormatExcep("Unhandled image format ('" + getExtension(path) + "')");
	}
}


bool ImFormatDecoder::hasImageExtension(const std::string& path)
{
	return
		hasExtension(path, "jpg") || hasExtension(path, "jpeg") ||
		hasExtension(path, "tga") ||
		hasExtension(path, "bmp") ||
		hasExtension(path, "png") ||
		hasExtension(path, "tif") || hasExtension(path, "tiff") ||
		hasExtension(path, "exr") ||
		hasExtension(path, "float") ||
		hasExtension(path, "gif") ||
		hasExtension(path, "hdr") ||
		hasExtension(path, "ktx") || 
		hasExtension(path, "ktx2");
}
