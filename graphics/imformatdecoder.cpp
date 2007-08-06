/*=====================================================================
imformatdecoder.cpp
-------------------
File created by ClassTemplate on Sat Apr 27 16:12:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "imformatdecoder.h"

#include "../utils/stringutils.h"
#include <stdlib.h>//for NULL
//#include "globals.h"
#include "bitmap.h"
#include <fstream>
#include <math.h>

//TEMP HACK:
#ifdef WIN32
#define IMFMTDECDR_JPEG_SUPPORT
#endif

#ifdef IMFMTDECDR_JPEG_SUPPORT
#include "jpegdecoder.h"
#endif

#include "tgadecoder.h"
#include "bmpdecoder.h"
#include "PNGDecoder.h"


//#include "graphicsdll/mmgrwrapper.h"


ImFormatDecoder::ImFormatDecoder()
{
	
}


ImFormatDecoder::~ImFormatDecoder()
{
	
}

int largestPowOf2LessEqualThan(int x)
{
	for(int i=1; i<31; ++i)
	{
		if(1 << i > x)
			return 1 << (i-1);
	}

	return 1;
}


//returns NULL on failure
/*void* ImFormatDecoder::decodeImage(const void* image, int image_size, const char* image_name, 
		int& bpp_out, int& width_out, int& height_out)
{

	const std::string imagename(image_name);

	void* imageout = NULL;

	//jpeg 2000?
#ifdef IMFMTDECDR_JPEG_SUPPORT
	if(hasExtension(imagename, "jpg") || hasExtension(imagename, "jpeg"))
	{
		imageout = JPEGDecoder::decode(image, image_size, bpp_out, width_out, height_out);
	}
#endif
	
	if(hasExtension(imagename, "tga"))
	{

		//imageout = TGADecoder::decode(image, image_size, bpp_out, width_out, height_out);
	}

	return imageout;
}*/


void ImFormatDecoder::decodeImage(const std::vector<unsigned char>& encoded_image, 
		const std::string& imagename, Bitmap& bitmap_out)
{

	//jpeg 2000?
#ifdef IMFMTDECDR_JPEG_SUPPORT
	if(hasExtension(imagename, "jpg") || hasExtension(imagename, "jpeg"))
	{
		JPEGDecoder::decode(encoded_image, imagename, bitmap_out);
	}
	else
#endif
	
	if(hasExtension(imagename, "tga"))
	{
		TGADecoder::decode(encoded_image, bitmap_out);
	}
	else if(hasExtension(imagename, "bmp"))
	{
		BMPDecoder::decode(encoded_image, bitmap_out);
	}
	else if(hasExtension(imagename, "png"))
	{
		PNGDecoder::decode(encoded_image, bitmap_out);
	}
	else
	{
		throw ImFormatExcep("unhandled format ('" + imagename + "'");
	}

}

	//returns newly alloced image, or NULL if not resized
/*void* ImFormatDecoder::resizeImageToPow2(const void* image, int width, int height, int bytespp, 
							int& width_out, int& height_out)
{
	if(!image)
		return NULL;

	//assert(bytespp == 3);

	//-----------------------------------------------------------------
	//resize to power of 2 dims
	//-----------------------------------------------------------------
	if(largestPowOf2LessEqualThan(width_out) != width_out || 
								largestPowOf2LessEqualThan(height_out) != height_out)
	{
		//::debugPrint("Warning: texture '" + std::string(image_name) + 
		//		"' has non power-of-2 dimensions: " + ::toString(width_out) + "*" + ::toString(height_out) + ". Tex will be crudely resized.");

		void* newimage = resizeToPowerOf2Dims(image, width, height, bytespp, width_out, height_out);

		return newimage;
	}
	else
	{
		width_out = width;
		height_out = height;
		return NULL;
	}
}*/

/*void ImFormatDecoder::freeImage(void* image)//for dlls, just delete[]s image
{
	delete[] image;
}*/


bool isPowerOf2(int x)
{
	assert(x >= 0);

	for(int i=0; i<31; ++i)//go thru all possible powers of 2
	{
		if(x == 1 << i)
			return true;
	}

	return false;
}

bool ImFormatDecoder::powerOf2Dims(int width, int height)
{
	return isPowerOf2(width) && isPowerOf2(height);
}


void ImFormatDecoder::resizeToPowerOf2Dims(const Bitmap& srcimage, Bitmap& image_out)
{
	image_out.resize(
		largestPowOf2LessEqualThan(srcimage.getWidth()),
		largestPowOf2LessEqualThan(srcimage.getHeight()));

	if(image_out.getWidth() == srcimage.getWidth() && image_out.getHeight() == srcimage.getHeight())
	{
		::memcpy(image_out.getData(), srcimage.getData(), srcimage.getWidth()*srcimage.getHeight()*srcimage.getBytesPP());
		return;
	}

	//NEWCODE: do bilinear filtering...
	
	const float widthf = (float)srcimage.getWidth();
	const float newwidthf = (float)image_out.getWidth();
	const float heightf = (float)srcimage.getHeight();
	const float newheightf = (float)image_out.getHeight();

	for(int y=0; y<image_out.getHeight(); ++y)
	{
		const float ideal_srcy = (float)y / newheightf * heightf;

		//casting truncates to integer part by default.
		assert((int)ideal_srcy == (int)floorf(ideal_srcy));
		int low_srcy = (int)ideal_srcy;//floorf(ideal_srcy);
		int high_srcy = low_srcy + 1;

		//clamp src coords to edges
		if(low_srcy < 0)
			low_srcy = 0;
		if(high_srcy >= srcimage.getHeight())
			high_srcy = srcimage.getHeight() - 1;



		const float low_y_coeff = 1.0f - (ideal_srcy - floorf(ideal_srcy));
		const float high_y_coeff = 1.0f - low_y_coeff;

		assert(low_y_coeff >= 0.0f && low_y_coeff <= 1.0f);
		assert(high_y_coeff >= 0.0f && high_y_coeff <= 1.0f);

		for(int x=0; x<image_out.getWidth(); ++x)
		{

			const float ideal_srcx = (float)x / newwidthf * widthf;
			

			//casting truncates to integer part by default.
			int low_srcx = (int)ideal_srcx;//floorf(ideal_srcx);

			assert((int)ideal_srcx == (int)floorf(ideal_srcx));

			int high_srcx = low_srcx + 1;

			//clamp src coords to edges
			if(low_srcy < 0)
				low_srcy = 0;
			if(high_srcy >= srcimage.getHeight())
				high_srcy = srcimage.getHeight() - 1;


			const float low_x_coeff = 1.0f - (ideal_srcx - floorf(ideal_srcx));
			const float high_x_coeff = 1.0f - low_x_coeff;

			assert(low_x_coeff >= 0.0f && low_x_coeff <= 1.0f);
			assert(high_x_coeff >= 0.0f && high_x_coeff <= 1.0f);


			//unsigned char* srcpixel = (unsigned char*)image + (srcx + srcy*width)*bytespp;
			
			unsigned char* destpixel = image_out.getData() + (x + y*image_out.getWidth())*image_out.getBytesPP();

			for(int i=0; i<image_out.getBytesPP(); ++i)
			{
				float destvalue = low_x_coeff * low_y_coeff * 
							(float)*((unsigned char*)srcimage.getData() + 
							(low_srcx + low_srcy*srcimage.getWidth())*srcimage.getBytesPP() + i);
				
				destvalue += high_x_coeff * low_y_coeff * 
							(float)*((unsigned char*)srcimage.getData() + 
							(high_srcx + low_srcy*srcimage.getWidth())*srcimage.getBytesPP() + i);
				
				destvalue += low_x_coeff * high_y_coeff * 
							(float)*((unsigned char*)srcimage.getData() + 
							(low_srcx + high_srcy*srcimage.getWidth())*srcimage.getBytesPP() + i);
				
				destvalue += high_x_coeff * high_y_coeff * 
							(float)*((unsigned char*)srcimage.getData() + 
							(high_srcx + high_srcy*srcimage.getWidth())*srcimage.getBytesPP() + i);

				destpixel[i] = (unsigned char)destvalue;
			}
		}
	}






}

//returns pointer to newly alloced buffer
//does bilinear filtering.
//this is totally unoptimised and slow
/*void* ImFormatDecoder::resizeToPowerOf2Dims(const void* image, int width, int height, int bytespp, 
												int& newwidth, int& newheight)
{
	newwidth = largestPowOf2LessEqualThan(width);
	newheight = largestPowOf2LessEqualThan(height);

	unsigned char* newimage = new unsigned char[width*height*bytespp];

	if(newwidth == width && newheight == height)
	{
		::memcpy(newimage, image, width*height*bytespp);
		return newimage;
	}

	//NEWCODE: do bilinear filtering...
	
	const float heightf = (float)height;
	const float newheightf = (float)newheight;
	const float widthf = (float)width;
	const float newwidthf = (float)newwidth;

	for(int y=0; y<newheight; ++y)
	{
		const float ideal_srcy = (float)y / newheightf * heightf;

		//casting truncates to integer part by default.
		assert((int)ideal_srcy == (int)floorf(ideal_srcy));
		int low_srcy = (int)ideal_srcy;//floorf(ideal_srcy);
		int high_srcy = low_srcy + 1;

		//clamp src coords to edges
		if(low_srcy < 0)
			low_srcy = 0;
		if(high_srcy >= height)
			high_srcy = height - 1;



		const float low_y_coeff = 1.0f - (ideal_srcy - floorf(ideal_srcy));
		const float high_y_coeff = 1.0f - low_y_coeff;

		assert(low_y_coeff >= 0.0f && low_y_coeff <= 1.0f);
		assert(high_y_coeff >= 0.0f && high_y_coeff <= 1.0f);

		for(int x=0; x<newwidth; ++x)
		{

			const float ideal_srcx = (float)x / newwidthf * widthf;
			

			//casting truncates to integer part by default.
			int low_srcx = (int)ideal_srcx;//floorf(ideal_srcx);

			assert((int)ideal_srcx == (int)floorf(ideal_srcx));

			int high_srcx = low_srcx + 1;

			//clamp src coords to edges
			if(low_srcy < 0)
				low_srcy = 0;
			if(high_srcy >= height)
				high_srcy = height - 1;


			const float low_x_coeff = 1.0f - (ideal_srcx - floorf(ideal_srcx));
			const float high_x_coeff = 1.0f - low_x_coeff;

			assert(low_x_coeff >= 0.0f && low_x_coeff <= 1.0f);
			assert(high_x_coeff >= 0.0f && high_x_coeff <= 1.0f);


			//unsigned char* srcpixel = (unsigned char*)image + (srcx + srcy*width)*bytespp;
			
			unsigned char* destpixel = newimage + (x + y*newwidth)*bytespp;

			for(int i=0; i<bytespp; ++i)
			{
				float destvalue = low_x_coeff * low_y_coeff * 
							(float)*((unsigned char*)image + (low_srcx + low_srcy*width)*bytespp + i);
				
				destvalue += high_x_coeff * low_y_coeff * 
							(float)*((unsigned char*)image + (high_srcx + low_srcy*width)*bytespp + i);
				
				destvalue += low_x_coeff * high_y_coeff * 
							(float)*((unsigned char*)image + (low_srcx + high_srcy*width)*bytespp + i);
				
				destvalue += high_x_coeff * high_y_coeff * 
							(float)*((unsigned char*)image + (high_srcx + high_srcy*width)*bytespp + i);

				destpixel[i] = (unsigned char)destvalue;
			}
		}
	}

	return newimage;
*/
	/*for(int y=0; y<newheight; ++y)
	{
		int srcy = (int)((float)y/(float)newheight * (float)height);
		if(srcy >= height)
			srcy = height - 1;

		for(int x=0; x<newwidth; ++x)
		{
			int srcx = (int)((float)x/(float)newwidth * (float)width);
			if(srcx >= width)
				srcx = width - 1;

			unsigned char* srcpixel = (unsigned char*)image + (srcx + srcy*width)*bytespp;
			unsigned char* destpixel = newimage + (x + y*newwidth)*bytespp;

			for(int i=0; i<bytespp; ++i)
			{
				destpixel[i] = srcpixel[i];
			}

			//*destpixel++ = *srcpixel++;
			//*destpixel++ = *srcpixel++;
			//*destpixel = *srcpixel;
		}
	}

	return newimage;*/
/*
}*/


/*void ImFormatDecoder::saveImageToDisk(const std::string& pathname, const Bitmap& bitmap)
{
	if(::hasExtension(pathname, "jpg"))
	{
		int length;
		std::auto_ptr<char> compressed_data((char*)JPEGDecoder::Compress(bitmap.getData(), 
			bitmap.getWidth(), bitmap.getHeight(), bitmap.getBytesPP(), length));

		std::ofstream file(pathname.c_str(), std::ios::binary);

		file.write(compressed_data.get(), length);
	}

}*/

