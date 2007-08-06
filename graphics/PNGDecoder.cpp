/*=====================================================================
PNGDecoder.cpp
--------------
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "PNGDecoder.h"


#include "../lpng128/png.h"
#include "bitmap.h"
#include "imformatdecoder.h"
#include "../utils/stringutils.h"


PNGDecoder::PNGDecoder()
{
	
}


PNGDecoder::~PNGDecoder()
{
	
}


//typedef void (PNGAPI *png_error_ptr) PNGARG((png_structp, png_const_charp));
void pngdecoder_error_func(png_structp png, const char* msg)
{
	throw ImFormatExcep("LibPNG error: " + std::string(msg));

}

void pngdecoder_warning_func(png_structp png, const char* msg)
{
	throw ImFormatExcep("LibPNG warning: " + std::string(msg));
}

int offset = 0;//TEMP UNTHREADSAFE HACK

void user_read_data_proc(png_structp png_ptr, png_bytep data, png_size_t length)
{
	void* read_io_ptr = png_get_io_ptr(png_ptr);

	unsigned char* buf = (unsigned char*)read_io_ptr;

	memcpy(data, &buf[offset], length);

	offset += length;
}



void PNGDecoder::decode(const std::vector<unsigned char>& encoded_img, Bitmap& bitmap_out)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
        pngdecoder_error_func, pngdecoder_warning_func);

    if (!png_ptr)
        throw ImFormatExcep("Failed to create PNG struct.");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);

        throw ImFormatExcep("Failed to create PNG info struct.");
    }

    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

        throw ImFormatExcep("Failed to create PNG info struct.");
    }

	/// Set read function ///
	png_set_read_fn(png_ptr, (void*)&encoded_img[0], user_read_data_proc);

	//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	//png_byte* row_pointers = png_get_rows(png_ptr, info_ptr);

	png_read_info(png_ptr, info_ptr);

	//png_get_IHDR(png_ptr, info_ptr, &width, &height,
     //  &bit_depth, &color_type, &interlace_type,
    //   &compression_type, &filter_method);

	
    const int width = (int)png_get_image_width(png_ptr, info_ptr);
    const int height = (int)png_get_image_height(png_ptr, info_ptr);
	const unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	const unsigned int color_type = png_get_color_type(png_ptr, info_ptr);


	if(color_type == PNG_COLOR_TYPE_PALETTE)
	{
		 png_set_palette_to_rgb(png_ptr);
	}
	/*else if(color_type == PNG_COLOR_TYPE_RGB)
	{
		int a =9;
	}
	else if(color_type == PNG_COLOR_TYPE_RGB_ALPHA)
	{
		int a =9;
	}*/

	//PNG can have files with 16 bits per channel.  If you only can handle
	//8 bits per channel, this will strip the pixels down to 8 bit.
    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

	///Remove alpha channel///
	if(color_type & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png_ptr);

	if(width <= 0 || width >= 1000000)
		throw ImFormatExcep("invalid width: " + toString(width));
	if(height <= 0 || height >= 1000000)
		throw ImFormatExcep("invalid height: " + toString(height));

	if(bit_depth != 8 && bit_depth != 16)
		throw ImFormatExcep("Only PNGs with per-channel bit depth of 8 supported.");

	bitmap_out.resize(width, height);
	bitmap_out.setBytesPP(3);

	//png_byte* row_pointers[height];
	
	//png_read_image(png_ptr, row_pointers);
	
	for(int y=0; y<height; ++y)
	{
		png_read_row(png_ptr, bitmap_out.getPixel(0, y), NULL);
	}
}















