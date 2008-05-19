/*=====================================================================
PNGDecoder.cpp
--------------
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "PNGDecoder.h"


#include <png.h>
#include "bitmap.h"
#include "texture.h"
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

/*
static int offset = 0;//TEMP UNTHREADSAFE HACK

void user_read_data_proc(png_structp png_ptr, png_bytep data, png_size_t length)
{
	void* read_io_ptr = png_get_io_ptr(png_ptr);

	unsigned char* buf = (unsigned char*)read_io_ptr;

	memcpy(data, &buf[offset], length);

	offset += length;
}
*/


Reference<Map2D> PNGDecoder::decode(const std::string& path)
{
	png_structp png_ptr = png_create_read_struct(
		PNG_LIBPNG_VER_STRING, 
		(png_voidp)NULL,
        pngdecoder_error_func, 
		pngdecoder_warning_func
		);

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

	// Open file and start reading from it.
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
		 throw ImFormatExcep("Failed to open file '" + path + "' for reading.");

	png_init_io(png_ptr, fp);

	png_read_info(png_ptr, info_ptr);

    const int width = (int)png_get_image_width(png_ptr, info_ptr);
    const int height = (int)png_get_image_height(png_ptr, info_ptr);
	const unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	const unsigned int color_type = png_get_color_type(png_ptr, info_ptr);
	const unsigned int num_channels = png_get_channels(png_ptr, info_ptr);

	unsigned int bitmap_num_bytes_pp = num_channels;

	if(color_type == PNG_COLOR_TYPE_PALETTE)
	{
		 png_set_palette_to_rgb(png_ptr);

		 assert(bitmap_num_bytes_pp == 1);
		 bitmap_num_bytes_pp = 3; // We are converting to 3 bytes per pixel
	}

	 

	// actually pretty much every colour type is supported :)
	//if(!(color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_RGB))
	//	throw ImFormatExcep("PNG has unsupported colour type.");

	//PNG can have files with 16 bits per channel.  If you only can handle
	//8 bits per channel, this will strip the pixels down to 8 bit.
    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

	///Remove alpha channel///
	if(color_type & PNG_COLOR_MASK_ALPHA)
	{
        png_set_strip_alpha(png_ptr);

		assert(bitmap_num_bytes_pp == 4);
		bitmap_num_bytes_pp = 3; // We are converting to 3 bytes per pixel
	}

	if(width <= 0 || width >= 1000000)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("invalid width: " + toString(width));
	}
	if(height <= 0 || height >= 1000000)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("invalid height: " + toString(height));
	}

	if(bit_depth != 8 && bit_depth != 16)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("Only PNGs with per-channel bit depth of 8 supported.");
	}

	// num_channels == 4 case should be stripped alpha case.
	if(!(num_channels == 1 || num_channels == 3 || num_channels == 4))
		throw ImFormatExcep("PNG had " + toString(num_channels) + " channels, only 1, 3 or 4 channels supported.");

	Texture* texture = new Texture();
	texture->resize(width, height, bitmap_num_bytes_pp);

	// Read in actual image data
	for(int y=0; y<height; ++y)
		png_read_row(png_ptr, texture->rowPointer(y), NULL);

	// Read the info at the end of the PNG file
	png_read_end(png_ptr, end_info);

	// Free structures
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	// Close file
	fclose(fp);

	return Reference<Map2D>(texture);
}

void PNGDecoder::decode(const std::string& path, Bitmap& bitmap_out)
{
		png_structp png_ptr = png_create_read_struct(
		PNG_LIBPNG_VER_STRING, 
		(png_voidp)NULL,
        pngdecoder_error_func, 
		pngdecoder_warning_func
		);

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

	// Open file and start reading from it.
	FILE* fp = fopen(path.c_str(), "rb");
	if(!fp)
		 throw ImFormatExcep("Failed to open file '" + path + "' for reading.");

	png_init_io(png_ptr, fp);

	png_read_info(png_ptr, info_ptr);

    const int width = (int)png_get_image_width(png_ptr, info_ptr);
    const int height = (int)png_get_image_height(png_ptr, info_ptr);
	const unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	const unsigned int color_type = png_get_color_type(png_ptr, info_ptr);
	const unsigned int num_channels = png_get_channels(png_ptr, info_ptr);

	unsigned int bitmap_num_bytes_pp = num_channels;

	if(color_type == PNG_COLOR_TYPE_PALETTE)
	{
		 png_set_palette_to_rgb(png_ptr);

		 assert(bitmap_num_bytes_pp == 1);
		 bitmap_num_bytes_pp = 3; // We are converting to 3 bytes per pixel
	}

	 

	// actually pretty much every colour type is supported :)
	//if(!(color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_RGB))
	//	throw ImFormatExcep("PNG has unsupported colour type.");

	//PNG can have files with 16 bits per channel.  If you only can handle
	//8 bits per channel, this will strip the pixels down to 8 bit.
    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

	///Remove alpha channel///
	if(color_type & PNG_COLOR_MASK_ALPHA)
	{
        png_set_strip_alpha(png_ptr);

		assert(bitmap_num_bytes_pp == 4);
		bitmap_num_bytes_pp = 3; // We are converting to 3 bytes per pixel
	}

	if(width <= 0 || width >= 1000000)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("invalid width: " + toString(width));
	}
	if(height <= 0 || height >= 1000000)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("invalid height: " + toString(height));
	}

	if(bit_depth != 8 && bit_depth != 16)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		throw ImFormatExcep("Only PNGs with per-channel bit depth of 8 supported.");
	}

	// num_channels == 4 case should be stripped alpha case.
	if(!(num_channels == 1 || num_channels == 3 || num_channels == 4))
		throw ImFormatExcep("PNG had " + toString(num_channels) + " channels, only 1, 3 or 4 channels supported.");

	bitmap_out.resize(width, height, bitmap_num_bytes_pp);

	// Read in actual image data
	for(int y=0; y<height; ++y)
		png_read_row(png_ptr, bitmap_out.rowPointer(y), NULL);

	// Read the info at the end of the PNG file
	png_read_end(png_ptr, end_info);

	// Free structures
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	// Close file
	fclose(fp);
}


const std::map<std::string, std::string> PNGDecoder::getMetaData(const std::string& image_path)
{
	png_structp png_ptr = png_create_read_struct(
		PNG_LIBPNG_VER_STRING, 
		(png_voidp)NULL,
        pngdecoder_error_func, 
		pngdecoder_warning_func
		);

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


	FILE* fp = fopen(image_path.c_str(), "rb");
	if(!fp)
		 throw ImFormatExcep("Failed to open file '" + image_path + "' for reading."); 

	png_init_io(png_ptr, fp);

	/// Set read function ///
	//png_set_read_fn(png_ptr, (void*)&encoded_img[0], user_read_data_proc);

	//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	//png_byte* row_pointers = png_get_rows(png_ptr, info_ptr);

	png_read_info(png_ptr, info_ptr);


	png_textp text_ptr;
	const png_uint_32 num_comments = png_get_text(png_ptr, info_ptr, &text_ptr, NULL);


	std::map<std::string, std::string> metadata;
	for(png_uint_32 i=0; i<num_comments; ++i)
		metadata[std::string(text_ptr[i].key)] = std::string(text_ptr[i].text);



	// Read the info at the end of the PNG file
	//png_read_end(png_ptr, end_info);

	// Free structures
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	// Close file
	fclose(fp);

	return metadata;
}


static void error_func(png_structp png, const char* msg)
{
	throw ImFormatExcep("LibPNG error: " + std::string(msg));

}

static void warning_func(png_structp png, const char* msg)
{
	throw ImFormatExcep("LibPNG warning: " + std::string(msg));
}


void PNGDecoder::write(const Bitmap& bitmap, const std::map<std::string, std::string>& metadata, const std::string& pathname)
{
	int colour_type;
	if(bitmap.getBytesPP() == 3)
		colour_type = PNG_COLOR_TYPE_RGB;
	else if(bitmap.getBytesPP() == 4)
		colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
	else
		throw ImFormatExcep("Invalid bytes pp");
		

	// Open the file
	FILE* fp = fopen(pathname.c_str(), "wb");
	if(fp == NULL)
		throw ImFormatExcep("Failed to open '" + pathname + "' for writing.");
	
	//Create and initialize the png_struct with the desired error handler functions.
	png_struct* png = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		NULL, //const_cast<Image*>(this), // error user pointer
		error_func, // error func
		warning_func // warning func
		);

	if(!png)
		throw ImFormatExcep("Failed to create PNG object.");

	png_info* info = png_create_info_struct(png);

	if(!info)
	{
		png_destroy_write_struct(&png, (png_infop*)NULL);//free png struct

		throw ImFormatExcep("Failed to create PNG info object.");
	}
	
	// set up the output control if you are using standard C stream
	png_init_io(png, fp);

	//------------------------------------------------------------------------
	// Set some image info
	//------------------------------------------------------------------------
	png_set_IHDR(png, info, bitmap.getWidth(), bitmap.getHeight(),
       8, // bit depth of each channel
	   colour_type, // colour type
	   PNG_INTERLACE_NONE, // interlace type
	   PNG_COMPRESSION_TYPE_BASE,// PNG_COMPRESSION_TYPE_DEFAULT, //compression type
	   PNG_FILTER_TYPE_BASE // PNG_FILTER_TYPE_DEFAULT);//filter method
	   );

	//------------------------------------------------------------------------
	// Write metadata pairs if present
	//------------------------------------------------------------------------
	if(metadata.size() > 0)
	{
		png_text* txt = new png_text[metadata.size()];
		memset(txt, 0, sizeof(png_text)*metadata.size());
		int c = 0;
		for(std::map<std::string, std::string>::const_iterator i = metadata.begin(); i != metadata.end(); ++i)
		{
			txt[c].compression = PNG_TEXT_COMPRESSION_NONE;
			txt[c].key = (png_charp)(*i).first.c_str();
			txt[c].text = (png_charp)(*i).second.c_str();
			txt[c].text_length = (*i).second.length();
			c++;
		}
		png_set_text(png, info, txt, (int)metadata.size());
		delete[] txt;
	}

	//png_set_gAMA(png_ptr, info_ptr, 2.3);

	// Write info
	png_write_info(png, info);


	//------------------------------------------------------------------------
	//write image rows
	//------------------------------------------------------------------------
	//std::vector<unsigned char> rowdata(bitmap.getWidth() * bitmap.getBytesPP());

	for(unsigned int y=0; y<bitmap.getHeight(); ++y)
	{
		//------------------------------------------------------------------------
		// copy floating point data to 8bpp format
		//------------------------------------------------------------------------
		//for(int x=0; x<bitmap.getWidth(); ++x)
		//	for(int i=0; i<bitmap.getBytesPP(); ++i)
		//		rowdata[x * bitmap.getBytesPP() + i] = (unsigned char)(bitmap.getPixel(x, y)[i] * 255.0f);

		//------------------------------------------------------------------------
		// write it
		//------------------------------------------------------------------------
		//png_bytep row_pointer = (png_bytep)bitmap.getPixel(0, y);  //(png_bytep)&(*rowdata.begin());

		png_write_row(
			png, 
			(png_bytep)bitmap.getPixel(0, y) // Pointer to row data
			);
	}

	//------------------------------------------------------------------------
	//finish writing file
	//------------------------------------------------------------------------
	png_write_end(png, info);

	//------------------------------------------------------------------------
	//free structs
	//------------------------------------------------------------------------
	png_destroy_write_struct(&png, &info);

	//------------------------------------------------------------------------
	//close the file
	//------------------------------------------------------------------------
	fclose(fp);

}








