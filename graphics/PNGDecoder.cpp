/*=====================================================================
PNGDecoder.cpp
--------------
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "PNGDecoder.h"


#include <png.h>
//#include "bitmap.h"
#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../indigo/globals.h"


#ifndef PNG_ALLOW_BENIGN_ERRORS
#error PNG_ALLOW_BENIGN_ERRORS should be defined in preprocessor defs.
#endif




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
	const std::string* path = (const std::string*)png->error_ptr;
	conPrint("PNG Warning while loading file '" + *path + "': " + std::string(msg));
}


Reference<Map2D> PNGDecoder::decode(const std::string& path)
{
	try
	{
		png_structp png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING, 
			(png_voidp)&path, // Pass a pointer to the path string as out user-data, so we can use it when printing a warning.
			pngdecoder_error_func, 
			pngdecoder_warning_func
			);

		if (!png_ptr)
			throw ImFormatExcep("Failed to create PNG struct.");

		// Make CRC errors into warnings.
		png_set_crc_action(png_ptr, PNG_CRC_WARN_USE, PNG_CRC_WARN_USE);

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
		FileHandle fp(path, "rb");

		png_init_io(png_ptr, fp.getFile());

		png_read_info(png_ptr, info_ptr);

		const unsigned int width = png_get_image_width(png_ptr, info_ptr);
		const unsigned int height = png_get_image_height(png_ptr, info_ptr);
		const unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
		const unsigned int color_type = png_get_color_type(png_ptr, info_ptr);
		const unsigned int num_channels = png_get_channels(png_ptr, info_ptr);

		// Work out gamma
		double use_gamma = 2.2;

		int intent = 0;
		if(png_get_sRGB(png_ptr, info_ptr, &intent))
		{
			// There is a sRGB chunk in this file, so it uses the sRGB colour space,
			// which in turn implies a specific gamma value.
			use_gamma = 2.2;
		}
		else
		{
			// Read gamma
			double file_gamma = 0;
			if(png_get_gAMA(png_ptr, info_ptr, &file_gamma))
			{
				// There was gamma info in the file.
				// File gamma is < 1, e.g. 0.45
				use_gamma = 1.0 / file_gamma;
			}
		}


		unsigned int bitmap_num_bytes_pp = num_channels;

		if(color_type == PNG_COLOR_TYPE_PALETTE)
		{
			 png_set_palette_to_rgb(png_ptr);

			 assert(bitmap_num_bytes_pp == 1);
			 bitmap_num_bytes_pp = 3; // We are converting to 3 bytes per pixel
		}

		//conPrint("bitmap_num_bytes_pp before alpha stripping: " + toString(bitmap_num_bytes_pp));
		
		///Remove alpha channel///
		if(color_type & PNG_COLOR_MASK_ALPHA)
		{
			//png_set_strip_alpha(png_ptr);

			// We either had Grey + alpha, or RGB + alpha
			//assert(bitmap_num_bytes_pp == 2 || bitmap_num_bytes_pp == 4);
			//bitmap_num_bytes_pp--;
		}

		//conPrint("bitmap_num_bytes_pp after stripping: " + toString(bitmap_num_bytes_pp));

		if(width >= 1000000)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("invalid width: " + toString(width));
		}
		if(height >= 1000000)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("invalid height: " + toString(height));
		}

		if(bit_depth != 8 && bit_depth != 16)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("Only PNGs with per-channel bit depths of 8 and 16 supported.");
		}

		// num_channels == 4 case should be stripped alpha case.
		if(!(num_channels == 1 || num_channels == 2 || num_channels == 3 || num_channels == 4))
			throw ImFormatExcep("PNG had " + toString(num_channels) + " channels, only 1, 2, 3 or 4 channels supported.");

		Map2D* map_2d = NULL;
		if(bit_depth == 8)
		{
			ImageMap<uint8_t, UInt8ComponentValueTraits>* image_map = new ImageMap<uint8_t, UInt8ComponentValueTraits>(width, height, bitmap_num_bytes_pp);
			image_map->setGamma((float)use_gamma);

			for(unsigned int y=0; y<height; ++y)
				png_read_row(png_ptr, (png_bytep)image_map->getPixel(0, y), NULL);

			// Read in actual image data
			//for(unsigned int y=0; y<height; ++y)
			//	png_read_row(png_ptr, texture->rowPointer(y), NULL);

			map_2d = image_map;
		}
		else if(bit_depth == 16)
		{
			// Swap to little-endian (Intel) byte order, from network byte order, which is what PNG uses.
			png_set_swap(png_ptr);

			ImageMap<uint16_t, UInt16ComponentValueTraits>* image_map = new ImageMap<uint16_t, UInt16ComponentValueTraits>(width, height, bitmap_num_bytes_pp);
			image_map->setGamma((float)use_gamma);

			// Read in actual image data
			for(unsigned int y=0; y<height; ++y)
				png_read_row(png_ptr, (png_bytep)image_map->getPixel(0, y), NULL);

			map_2d = image_map;
		}
		else
		{
			assert(0);
		}

	
		// Read the info at the end of the PNG file
		png_read_end(png_ptr, end_info);

		// Free structures
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

		return Reference<Map2D>(map_2d);
	}
	catch(Indigo::Exception& )
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}

void PNGDecoder::decode(const std::string& path, Bitmap& bitmap_out)
{
	try
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
		FileHandle fp(path, "rb");

		png_init_io(png_ptr, fp.getFile());

		png_read_info(png_ptr, info_ptr);

		const unsigned int width = png_get_image_width(png_ptr, info_ptr);
		const unsigned int height = png_get_image_height(png_ptr, info_ptr);
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

		if(width >= 1000000)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("invalid width: " + toString(width));
		}
		if(height >= 1000000)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("invalid height: " + toString(height));
		}

		if(bit_depth != 8 && bit_depth != 16)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			throw ImFormatExcep("Only PNGs with per-channel bit depth of 8 supported.");
		}

		// num_channels == 4 case should be stripped alpha case.
		if(!(num_channels == 1 || num_channels == 3 || num_channels == 4))
			throw ImFormatExcep("PNG had " + toString(num_channels) + " channels, only 1, 3 or 4 channels supported.");

		bitmap_out.resize(width, height, bitmap_num_bytes_pp);

		// Read in actual image data
		for(unsigned int y=0; y<height; ++y)
			png_read_row(png_ptr, bitmap_out.rowPointer(y), NULL);

		// Read the info at the end of the PNG file
		png_read_end(png_ptr, end_info);

		// Free structures
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	}
	catch(Indigo::Exception&)
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}


const std::map<std::string, std::string> PNGDecoder::getMetaData(const std::string& image_path)
{
	try
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


		FileHandle fp(image_path, "rb");

		png_init_io(png_ptr, fp.getFile());

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

		return metadata;
	}
	catch(Indigo::Exception& )
	{
		throw ImFormatExcep("Failed to open file '" + image_path + "' for reading.");
	}
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
	try
	{
		int colour_type;
		if(bitmap.getBytesPP() == 3)
			colour_type = PNG_COLOR_TYPE_RGB;
		else if(bitmap.getBytesPP() == 4)
			colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
		else
			throw ImFormatExcep("Invalid bytes pp");
			

		// Open the file
		FileHandle fp(pathname, "wb");
		
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
		png_init_io(png, fp.getFile());

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


		for(unsigned int y=0; y<bitmap.getHeight(); ++y)
		{
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
	}
	catch(Indigo::Exception& )
	{
		throw ImFormatExcep("Failed to open '" + pathname + "' for writing.");
	}
}
