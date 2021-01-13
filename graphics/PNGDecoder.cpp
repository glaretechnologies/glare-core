/*=====================================================================
PNGDecoder.cpp
--------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Wed Jul 26 22:08:57 2006
=====================================================================*/
#include "PNGDecoder.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include <png.h>
#if !defined NO_LCMS_SUPPORT
#include <lcms2.h>
#endif


#ifndef PNG_ALLOW_BENIGN_ERRORS
#error PNG_ALLOW_BENIGN_ERRORS should be defined in preprocessor defs.
#endif
#ifndef PNG_INTEL_SSE
#error PNG_INTEL_SSE should be defined in preprocessor defs.
#endif


// See 'Precompute colour profile data' below.
static const size_t sRGB_profile_data_size = 592;
static const uint8 sRGB_profile_data[] = { 0, 0, 2, 80, 108, 99, 109, 115, 4, 48, 0, 0, 109, 110, 116, 114, 82, 71, 66, 32, 88, 89, 90, 32, 7, 227, 0, 5, 0, 28, 0, 6, 0, 20, 0, 39, 97, 99, 115, 112, 77, 83, 70, 84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 246, 214, 0, 1, 0, 0, 0, 0, 211, 45, 108, 99, 109, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 100, 101, 115, 99, 0, 0, 1, 8, 0, 0, 0, 56, 99, 112, 114, 116, 0, 0, 1, 64, 0, 0, 0, 78, 119, 116, 112, 116, 0, 0, 1, 144, 0, 0, 0, 20, 99, 104, 97, 100, 0, 0, 1, 164, 0, 0, 0, 44, 114,
	88, 89, 90, 0, 0, 1, 208, 0, 0, 0, 20, 98, 88, 89, 90, 0, 0, 1, 228, 0, 0, 0, 20, 103, 88, 89, 90, 0, 0, 1, 248, 0, 0, 0, 20, 114, 84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 103, 84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 98,
	84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 99, 104, 114, 109, 0, 0, 2, 44, 0, 0, 0, 36, 109, 108, 117, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 12, 101, 110, 85, 83, 0, 0, 0, 28, 0, 0, 0, 28, 0, 115, 0, 82, 0, 71, 0, 66, 0,
	32, 0, 98, 0, 117, 0, 105, 0, 108, 0, 116, 0, 45, 0, 105, 0, 110, 0, 0, 109, 108, 117, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 12, 101, 110, 85, 83, 0, 0, 0, 50, 0, 0, 0, 28, 0, 78, 0, 111, 0, 32, 0, 99, 0, 111, 0, 112, 0,
	121, 0, 114, 0, 105, 0, 103, 0, 104, 0, 116, 0, 44, 0, 32, 0, 117, 0, 115, 0, 101, 0, 32, 0, 102, 0, 114, 0, 101, 0, 101, 0, 108, 0, 121, 0, 0, 0, 0, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 246, 214, 0, 1, 0, 0, 0, 0, 211, 45, 115,
	102, 51, 50, 0, 0, 0, 0, 0, 1, 12, 74, 0, 0, 5, 227, 255, 255, 243, 42, 0, 0, 7, 155, 0, 0, 253, 135, 255, 255, 251, 162, 255, 255, 253, 163, 0, 0, 3, 216, 0, 0, 192, 148, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 111, 148, 0, 0, 56, 238, 0,
	0, 3, 144, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 36, 157, 0, 0, 15, 131, 0, 0, 182, 190, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 98, 165, 0, 0, 183, 144, 0, 0, 24, 222, 112, 97, 114, 97, 0, 0, 0, 0, 0, 3, 0, 0, 0, 2, 102, 102, 0,
	0, 242, 167, 0, 0, 13, 89, 0, 0, 19, 208, 0, 0, 10, 91, 99, 104, 114, 109, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 163, 215, 0, 0, 84, 123, 0, 0, 76, 205, 0, 0, 153, 154, 0, 0, 38, 102, 0, 0, 15, 92
};


// TRY:
// png_set_keep_unknown_chunks(png_ptr, 0, NULL, 0);
// or
// png_set_keep_unknown_chunks(png_ptr, 1, NULL, 0);


static void pngdecoder_error_func(png_structp /*png*/, const char* msg)
{
	throw ImFormatExcep("LibPNG error: " + std::string(msg));
}


static void pngdecoder_warning_func(png_structp /*png*/, const char* /*msg*/)
{
	//const std::string* path = (const std::string*)png->error_ptr;
	//conPrint("PNG Warning while loading file '" + *path + "': " + std::string(msg));
}


Reference<Map2D> PNGDecoder::decode(const std::string& path)
{
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	try
	{
		png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING, 
			(png_voidp)&path, // Pass a pointer to the path string as our user-data, so we can use it when printing a warning.
			pngdecoder_error_func, 
			pngdecoder_warning_func
		);
		if(!png_ptr)
			throw ImFormatExcep("Failed to create PNG struct.");

		// Make CRC errors into warnings.
		png_set_crc_action(png_ptr, PNG_CRC_WARN_USE, PNG_CRC_WARN_USE);

		info_ptr = png_create_info_struct(png_ptr);
		if(!info_ptr)
			throw ImFormatExcep("Failed to create PNG info struct.");

		// Open file and start reading from it.
		FileHandle fp(path, "rb");

		png_init_io(png_ptr, fp.getFile());

		png_read_info(png_ptr, info_ptr);


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


		// Set up the transformations that we want to run.
		png_set_palette_to_rgb(png_ptr);
		png_set_expand_gray_1_2_4_to_8(png_ptr);

		// Re-read info, which will be changed based on our transformations.
		png_read_update_info(png_ptr, info_ptr);

		const unsigned int width = png_get_image_width(png_ptr, info_ptr);
		const unsigned int height = png_get_image_height(png_ptr, info_ptr);
		const unsigned int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
		const unsigned int num_channels = png_get_channels(png_ptr, info_ptr);

		if(width >= 1000000)
			throw ImFormatExcep("invalid width: " + toString(width));
		if(height >= 1000000)
			throw ImFormatExcep("invalid height: " + toString(height));

		if(num_channels > 4)
			throw ImFormatExcep("Invalid num channels: " + toString(num_channels));

		if(bit_depth != 8 && bit_depth != 16)
			throw ImFormatExcep("Invalid bit depth found: " + toString(bit_depth));

		Reference<Map2D> map_2d;
		if(bit_depth == 8)
		{
			Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > image_map = new ImageMap<uint8_t, UInt8ComponentValueTraits>(width, height, num_channels);
			image_map->setGamma((float)use_gamma);

			std::vector<png_bytep> row_pointers(height);
			for(unsigned int y=0; y<height; ++y)
				row_pointers[y] = (png_bytep)image_map->getPixel(0, y);

			png_read_image(png_ptr, &row_pointers[0]);

			map_2d = image_map;
		}
		else if(bit_depth == 16)
		{
			// Swap to little-endian (Intel) byte order, from network byte order, which is what PNG uses.
			png_set_swap(png_ptr);

			Reference<ImageMap<uint16_t, UInt16ComponentValueTraits> > image_map = new ImageMap<uint16_t, UInt16ComponentValueTraits>(width, height, num_channels);
			image_map->setGamma((float)use_gamma);

			std::vector<png_bytep> row_pointers(height);
			for(unsigned int y=0; y<height; ++y)
				row_pointers[y] = (png_bytep)image_map->getPixel(0, y);

			png_read_image(png_ptr, &row_pointers[0]);

			map_2d = image_map;
		}
		else
		{
			assert(0);
		}

		// Free structures
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

		return map_2d;
	}
	catch(ImFormatExcep& e)
	{
		// Free any allocated libPNG structures, then re-throw the exception.
		if(png_ptr && info_ptr)
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		throw e;
	}
	catch(glare::Exception& e)
	{
		// Free any allocated libPNG structures.
		if(png_ptr && info_ptr)
			png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

		throw ImFormatExcep("Failed to open file '" + path + "' for reading: " + e.what());
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
	catch(glare::Exception& )
	{
		throw ImFormatExcep("Failed to open file '" + image_path + "' for reading.");
	}
}


void PNGDecoder::write(const Bitmap& bitmap, const std::map<std::string, std::string>& metadata, const std::string& pathname)
{
	write(bitmap.getPixel(0, 0), (unsigned int)bitmap.getWidth(), (unsigned int)bitmap.getHeight(), (unsigned int)bitmap.getBytesPP(), metadata, pathname);
}


void PNGDecoder::write(const Bitmap& bitmap, const std::string& pathname)
{
	std::map<std::string, std::string> metadata;
	write(bitmap, metadata, pathname);
}


void PNGDecoder::write(const ImageMap<uint8, UInt8ComponentValueTraits>& imagemap, const std::string& pathname) // Write with no metadata
{
	write(imagemap.getData(), (unsigned int)imagemap.getWidth(), (unsigned int)imagemap.getHeight(), (unsigned int)imagemap.getN(), pathname);
}


void PNGDecoder::write(const uint8* data, unsigned int W, unsigned int H, unsigned int N, const std::string& pathname) // Write with no metadata
{
	std::map<std::string, std::string> metadata;
	write(data, W, H, N, metadata, pathname);
}


// Write with metadata
void PNGDecoder::write(const uint8* data, unsigned int W, unsigned int H, unsigned int N, const std::map<std::string, std::string>& metadata, const std::string& pathname)
{
	png_struct* png = NULL;
	png_info* info = NULL;

	try
	{
		int colour_type;
		if(N == 1)
			colour_type = PNG_COLOR_TYPE_GRAY;
		else if(N == 2)
			colour_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		else if(N == 3)
			colour_type = PNG_COLOR_TYPE_RGB;
		else if(N == 4)
			colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
		else
			throw ImFormatExcep("Invalid bytes pp");
			

		// Open the file
		FileHandle fp(pathname, "wb");
		
		// Create and initialize the png_struct with the desired error handler functions.
		png = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, // error user pointer
			pngdecoder_error_func, // error func
			pngdecoder_warning_func // warning func
		);
		if(!png)
			throw ImFormatExcep("Failed to create PNG object.");

		info = png_create_info_struct(png);
		if(!info)
			throw ImFormatExcep("Failed to create PNG info object.");
		
		// set up the output control if you are using standard C stream
		png_init_io(png, fp.getFile());

		//------------------------------------------------------------------------
		// Set some image info
		//------------------------------------------------------------------------
		png_set_IHDR(png, info, (png_uint_32)W, (png_uint_32)H,
			8, // bit depth of each channel
			colour_type, // colour type
			PNG_INTERLACE_NONE, // interlace type
			PNG_COMPRESSION_TYPE_DEFAULT, // compression type
			PNG_FILTER_TYPE_DEFAULT // filter method
		);

		// Write an ICC sRGB colour profile.
		// NOTE: We could write an sRGB Chunk instead, see section '11.3.3.5 sRGB Standard RGB colour space' (http://www.libpng.org/pub/png/spec/iso/index-object.html#11iCCP)
		//
		// Checking embedded ICC profiles can be done with this online tool: 'Jeffrey's Image Metadata Viewer': http://exif.regex.info/exif.cgi
#if !defined NO_LCMS_SUPPORT
		if(N > 1) // It's not allowed to have a colour profile in a greyscale image.
		{
#define SAVE_PRECOMPUTED_SRGB_PROFILE 1
#if SAVE_PRECOMPUTED_SRGB_PROFILE
				// Use precomputed sRGB profile.
#if PNG_LIBPNG_VER >= 10634
				png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_const_bytep)sRGB_profile_data, sRGB_profile_data_size);
#else
				png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_charp)sRGB_profile_data, sRGB_profile_data_size);
#endif

#else // else if !SAVE_PRECOMPUTED_SRGB_PROFILE:

				// Else get Little CMS to spit out a fresh profile.
				cmsHPROFILE profile = cmsCreate_sRGBProfile();
				if(profile == NULL)
					throw ImFormatExcep("Failed to create colour profile.");

				// Get num bytes needed to store the encoded profile.
				cmsUInt32Number profile_size = 0;
				if(cmsSaveProfileToMem(profile, NULL, &profile_size) == FALSE)
					throw ImFormatExcep("Failed to save colour profile.");

				std::vector<uint8> buf(profile_size);

				// Now write the actual profile.
				if(cmsSaveProfileToMem(profile, &buf[0], &profile_size) == FALSE)
					throw ImFormatExcep("Failed to save colour profile.");

				cmsCloseProfile(profile);

#if PNG_LIBPNG_VER >= 10634
				png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_const_bytep)&buf[0], profile_size);
#else
				png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_charp)&buf[0], profile_size);
#endif

#endif // SAVE_PRECOMPUTED_SRGB_PROFILE

		}
#endif

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


		// Write info
		png_write_info(png, info);

		for(unsigned int y=0; y<H; ++y)
			png_write_row(
				png,
				(png_bytep)(data + y * W * N) // Pointer to row data
			);

		//------------------------------------------------------------------------
		//finish writing file
		//------------------------------------------------------------------------
		png_write_end(png, info);

		//------------------------------------------------------------------------
		//free structs
		//------------------------------------------------------------------------
		png_destroy_write_struct(&png, &info);
	}
	catch(ImFormatExcep& e)
	{
		// Free any allocated libPNG structures, then re-throw the exception.
		if(png && info)
			png_destroy_write_struct(&png, &info);
		throw e;
	}
	catch(glare::Exception& )
	{
		if(png && info)
			png_destroy_write_struct(&png, &info);
		throw ImFormatExcep("Failed to open '" + pathname + "' for writing.");
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "bitmap.h"


// Read a PNG from path and make sure it is the same as target_image.
static void readAndCompare(const std::string& path, const ImageMapUInt8& target_image)
{
	Reference<Map2D> im = PNGDecoder::decode(path);
	testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer()) != NULL);
	const ImageMapUInt8* im_map = im.downcastToPtr<ImageMapUInt8>();

	testAssert(*im_map == target_image);
}


void PNGDecoder::test()
{
	conPrint("PNGDecoder::test()");


	// Precompute colour profile data
#if 0
	if(true)
	{
		cmsHPROFILE profile = cmsCreate_sRGBProfile();
		if(profile == NULL)
			throw ImFormatExcep("Failed to create colour profile.");

		// Get num bytes needed to store the encoded profile.
		cmsUInt32Number profile_size = 0;
		if(cmsSaveProfileToMem(profile, NULL, &profile_size) == 0)
			throw ImFormatExcep("Failed to save colour profile.");

		std::vector<uint8> buf(profile_size);

		// Now write the actual profile.
		if(cmsSaveProfileToMem(profile, &buf[0], &profile_size) == 0)
			throw ImFormatExcep("Failed to save colour profile.");

		cmsCloseProfile(profile);

		std::string s = "static const size_t sRGB_profile_data_size = " + toString(profile_size) + ";\n";
		s += "static const uint8 sRGB_profile_data[] = { ";
		for(size_t i=0; i<buf.size(); ++i)
		{
			s += toString(buf[i]) + ((i + 1 < buf.size()) ? ", " : "");
			if(i % 60 == 0)
				s += "\n";
		}
		s += " }; \n";

		conPrint(s);

		assert(profile_size == sRGB_profile_data_size);

		std::vector<uint8> precomp_data(sRGB_profile_data, sRGB_profile_data + sRGB_profile_data_size);

		// NOTE: colour profile seems to have the current date in it, so bytes will not be exactly the same, see _cmsEncodeDateTimeNumber.

		//assert(precomp_data == buf);

		/*for(size_t i=0; i<buf.size(); ++i)
		{
			conPrint(toString(i) + ": " + toString(buf[i]) + " " + toString(precomp_data[i]));
			assert(buf[i] == precomp_data[i]);
		}*/
	}
#endif

	try
	{
		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test3.png";

		const int W = 20;
		const int H = 10;
		const int N = 3;
		Bitmap bitmap(20, 10, 3, NULL);
		for(int y=0; y<H; ++y)
			for(int x=0; x<W; ++x)
				for(int c=0; c<N; ++c)
					bitmap.getPixelNonConst(x, y)[c] = (uint8)((x + y + c) % 255);

		ImageMapUInt8Ref imagemap = bitmap.toImageMap();

		// Write and read the bitmap
		PNGDecoder::write(bitmap, path);
		readAndCompare(path, *imagemap);

		// Read and write the ImageMapUInt8
		PNGDecoder::write(*imagemap, path);
		readAndCompare(path, *imagemap);

		// Write as data ptr
		PNGDecoder::write(imagemap->getData(), W, H, N, path);
		readAndCompare(path, *imagemap);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	/*{
		Map2DRef map = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.png");

		Bitmap bitmap;
		bitmap.setFromImageMap(*map.downcast<ImageMapUInt8>());

		PNGDecoder::write(bitmap, "saved.png");
	}*/


	try
	{
		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/basn0g08_badcrc.png");

		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/pino.png");
		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/Fencing_Iron.png");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Test PNG files in PngSuite-2013jan13 (From http://www.schaik.com/pngsuite/)
	try
	{
		const std::string png_suite_dir = TestUtils::getTestReposDir() + "/testfiles/pngs/PngSuite-2013jan13";
		const std::vector<std::string> files = FileUtils::getFilesInDir(png_suite_dir);

		for(size_t i=0; i<files.size(); ++i)
		{
			const std::string path = png_suite_dir + "/" + files[i];

			if(::hasExtension(path, "png") && !::hasPrefix(files[i], "x"))
			{
				// conPrint("Processing '" + path + "'...");

				PNGDecoder::decode(path);
			}
		}

		// Test some particular files, see http://www.schaik.com/pngsuite/pngsuite_bas_png.html
		
		//==================== Test basic formats =====================
		// black & white
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g01.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 2 bit (4 level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g02.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 8 bit (256 level) grayscale 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g08.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 16 bit (64k level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g16.png");
			testAssert(map->getBytesPerPixel() == 2);
		}

		// 3x8 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn2c08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 3x16 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn2c16.png");
			testAssert(map->getBytesPerPixel() == 6);
		}

		// 1 bit (2 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn3p01.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit (256 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn3p08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit grayscale + 8 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn4a08.png");
			testAssert(map->getBytesPerPixel() == 2);
			testAssert(map->hasAlphaChannel());
		}

		// 16 bit grayscale + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn4a16.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x8 bits rgb color + 8 bit alpha-channel 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn6a08.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x16 bits rgb color + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn6a16.png");
			testAssert(map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}

		//==================== Test some interlaced files =====================
		// black & white
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g01.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 2 bit (4 level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g02.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 8 bit (256 level) grayscale 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g08.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 16 bit (64k level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g16.png");
			testAssert(map->getBytesPerPixel() == 2);
		}

		// 3x8 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi2c08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 3x16 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi2c16.png");
			testAssert(map->getBytesPerPixel() == 6);
		}

		// 1 bit (2 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi3p01.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit (256 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi3p08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit grayscale + 8 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi4a08.png");
			testAssert(map->getBytesPerPixel() == 2);
			testAssert(map->hasAlphaChannel());
		}

		// 16 bit grayscale + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi4a16.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x8 bits rgb color + 8 bit alpha-channel 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi6a08.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x16 bits rgb color + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi6a16.png");
			testAssert(map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		failTest(e.what());
	}



	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/NO_SUCH_FILE.png");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}


	// Try writing a bitmap object to a PNG file
	try
	{
		Bitmap bitmap(20, 10, 3, NULL);
		bitmap.zero();

		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test.png";
		write(bitmap, path);

		// Now read it to make sure it's a valid PNG
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == 20);
		testAssert(im->getMapHeight() == 10);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try writing an ImageMap object to a PNG file
	try
	{
		ImageMapUInt8 imagemap(20, 10, 3);
		imagemap.set(125);

		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test2.png";
		write(imagemap, path);

		// Now read it to make sure it's a valid PNG
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == 20);
		testAssert(im->getMapHeight() == 10);
		testAssert(im->getBytesPerPixel() == 3);
		testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer()) != NULL);
		testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer())->getPixel(5, 5)[0] == 125);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}



	// Try testing write failure - write to an invalid location
	try
	{
		ImageMapUInt8 imagemap(20, 10, 3);
		imagemap.set(125);

#if defined(_WIN32)
		const std::string path = "abc:/def.png";
#else
		const std::string path = "/abc/def.png";
#endif
		write(imagemap, path);

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}


	// Performance test
	if(false)
	{
		const int num_iters = 10;
		double min_time = 1.0e10;
		for(int z=0; z<num_iters; ++z)
		{
			Timer timer;
			const std::string path = TestUtils::getTestReposDir() + "/testscenes/zomb_dark_sphere_bug_scene/PACKED_0_PACKED_0_leather_white3.png";
			Reference<Map2D> im = decode(path);
			testAssert(im->getMapWidth() == 2189);
			min_time = myMin(min_time, timer.elapsed());
		}
		
		conPrint("Time to load PACKED_0_PACKED_0_leather_white3.png: " + doubleToStringNSigFigs(min_time, 4) + " s.");
	}


	conPrint("PNGDecoder::test() done.");
}


#endif // BUILD_TESTS
