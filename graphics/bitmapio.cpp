/*=====================================================================
bitmapio.cpp
------------
File created by ClassTemplate on Fri Dec 10 04:15:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "bitmapio.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include "tgadecoder.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include <fstream>


BitmapIO::BitmapIO()
{
	
}


BitmapIO::~BitmapIO()
{
	
}


void BitmapIO::saveBitmapToDisk(const Bitmap& bitmap, const std::string& pathname)
{
	if(::hasExtension(pathname, "tga"))
	{
		std::vector<unsigned char> encoded_img;
		TGADecoder::encode(bitmap, encoded_img);

		std::ofstream outfile(pathname.c_str(), std::ios::binary);

		if(!outfile)
			throw BitmapIOExcep("Could not open '" + pathname + "' for writing.");

		outfile.write((const char*)&(*encoded_img.begin()), encoded_img.size());
	}
	else
	{
		assert(0);
		throw BitmapIOExcep("no handler for extension type found (img path='" + 
			pathname + "')");
	}

}


void BitmapIO::readBitmapFromDisk(const std::string& pathname, Bitmap& bitmap_out)
{
	//------------------------------------------------------------------------
	//read file
	//------------------------------------------------------------------------
	std::vector<unsigned char> encoded_img;

	try
	{
		FileUtils::readEntireFile(pathname, encoded_img);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw BitmapIOExcep("FileUtilsExcep: " + e.what());
	}

	//------------------------------------------------------------------------
	//decode
	//------------------------------------------------------------------------
	/*if(::hasExtension(pathname, "tga"))
	{
		TGADecoder::decode(encoded_img, bitmap_out);
	}
	else
	{
		assert(0);
		throw BitmapIOExcep("no handler for extension type found (img path='" + 
			pathname + "')");
	}*/

	try
	{
		ImFormatDecoder::decodeImage(encoded_img, pathname, bitmap_out);
	}
	catch(ImFormatExcep& e)
	{
		throw BitmapIOExcep("ImFormatExcep: " + e.what());
	}

}


