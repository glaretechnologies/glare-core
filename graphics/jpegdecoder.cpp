/*=====================================================================
jpegdecoder.cpp
---------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
=====================================================================*/
#include "jpegdecoder.h"


#include "ImageMap.h"
#include "image.h"
#include "bitmap.h"
#include "imformatdecoder.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Timer.h"
#include <jpeglib.h>
#include <lcms2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
//#if JPEG_LIB_VERSION < 70
//#error Please compile with Libjpeg 7.0
//#endif
*/


JPEGDecoder::JPEGDecoder()
{}


JPEGDecoder::~JPEGDecoder()
{}


static void my_error_exit(j_common_ptr cinfo)
{
	// Format message into buffer.
	char buffer[JMSG_LENGTH_MAX];
	memset(buffer, 0, sizeof(buffer));

	(*cinfo->err->format_message) (cinfo, buffer);
	
	throw ImFormatExcep("Error while reading JPEG file: " + std::string(buffer));
}


static void my_emit_message(j_common_ptr cinfo, int msg_level)
{}

static void my_output_message(j_common_ptr cinfo)
{}


//static double total_jpeg_decoding_time = 0;


// RAII wrapper for jpeg_decompress_struct, so jpeg_destroy_decompress() will always be called, even if an exception is thrown.
class JpegDecompress
{
public:
	JpegDecompress() {}
	~JpegDecompress() { jpeg_destroy_decompress(&cinfo); }

	struct jpeg_decompress_struct cinfo;
};


Reference<Map2D> JPEGDecoder::decode(const std::string& indigo_base_dir, const std::string& path)
{
	//conPrint("JPEGDecoder::decode(), path: " + path);
	//Timer timer;

	try
	{
		JpegDecompress decompress;
		jpeg_decompress_struct& cinfo = decompress.cinfo;

		//------------------------------------------------------------------------
		//set error handling
		//------------------------------------------------------------------------
		struct jpeg_error_mgr error_manager;
		jpeg_std_error(&error_manager); // Initialise.  NOTE: Assuming this won't throw an exception.
		error_manager.emit_message = my_emit_message;
		error_manager.error_exit = my_error_exit;
		error_manager.output_message = my_output_message;
		
		cinfo.err = &error_manager; // Set error manager

		//------------------------------------------------------------------------
		//Init main struct
		//------------------------------------------------------------------------
		jpeg_create_decompress(&cinfo);

		//------------------------------------------------------------------------
		//Open file
		//------------------------------------------------------------------------
		FileHandle infile(path, "rb");

		// Specify file data source
		jpeg_stdio_src(&cinfo, infile.getFile());


		//------------------------------------------------------------------------
		//read header
		//------------------------------------------------------------------------
		jpeg_read_header(&cinfo, TRUE);

		// Go through markers until we find the icc profile
		//jpeg_saved_marker_ptr marker = cinfo.marker_list;
		//while(marker != NULL)
		//{
		//	//conPrint(marker->);

		//	// ICC_PROFILE
		//	if(marker->data_length >= 11 && strncmp((const char*)marker->data, "ICC_PROFILE", 11) == 0)
		//	{
		//		conPrint("Found ICC profile");
		//		hInProfile = cmsOpenProfileFromMem(marker->data + 14, marker->data_length);
		//		break;
		//	}

		//	marker = marker->next;
		//}

		/*printVar(cinfo.jpeg_color_space);
		if(cinfo.jpeg_color_space == JCS_RGB)
			conPrint("JCS_RGB");
		else if(cinfo.jpeg_color_space == JCS_YCbCr)
			conPrint("JCS_YCbCr");
		if(cinfo.jpeg_color_space == JCS_CMYK)
			conPrint("JCS_CMYK");
		else if(cinfo.jpeg_color_space == JCS_YCCK)
			conPrint("JCS_YCCK");*/

		// If the JPEG colour space is JCS_YCCK (Used by Photoshop for CMYK files), then get libjpeg to convert it to CMYK.  It can't convert to RGB, we have to do that ourselves.
		if(cinfo.jpeg_color_space == JCS_YCCK)
			cinfo.out_color_space = JCS_CMYK;

		jpeg_start_decompress(&cinfo);

		if(!(cinfo.num_components == 1 || cinfo.num_components == 3 || cinfo.num_components == 4))
			throw ImFormatExcep("Invalid num components " + toString(cinfo.num_components) + ": Only 1, 3 or 4 component JPEGs are currently supported.");

		// Num components to use for our returned image.  JPEG never has alpha so we have a max of 3 channels.
		const int final_num_components = myMin(3, cinfo.num_components);

		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture( new ImageMap<uint8_t, UInt8ComponentValueTraits>(
			cinfo.output_width, cinfo.output_height, final_num_components
		));

		// Read gamma.  NOTE: this seems to just always be 1.
		// We need to extract the ICC profile.
		//const float gamma = cinfo.output_gamma;
		//conPrint("JPEG output gamma: " + toString(gamma));

		//------------------------------------------------------------------------
		//alloc row buffer
		//------------------------------------------------------------------------
		const unsigned int row_stride = cinfo.output_width * cinfo.output_components;
		const int final_W_pixels = cinfo.output_width;

		if(cinfo.out_color_space == JCS_CMYK)
		{
			// Make a one-row-high sample array that will go away when done with image
			std::vector<uint8_t> buffer(row_stride);
			uint8_t* scanline_ptrs[1] = { &buffer[0] };

			std::vector<uint8_t> modified_buffer(row_stride); // Used for storing inverted CMYK colours.

			// Make a little CMS transform
			MemMappedFile in_profile_file(indigo_base_dir + "/data/ICC_profiles/USWebCoatedSWOP.icc");
			cmsHPROFILE hInProfile = cmsOpenProfileFromMem(in_profile_file.fileData(), (cmsUInt32Number)in_profile_file.fileSize());
			if(!hInProfile)
				throw ImFormatExcep("Failed to create in-profile");

			MemMappedFile out_profile_file(indigo_base_dir + "/data/ICC_profiles/sRGB_v4_ICC_preference.icc");
			cmsHPROFILE hOutProfile = cmsOpenProfileFromMem(out_profile_file.fileData(), (cmsUInt32Number)out_profile_file.fileSize());
			if(!hOutProfile)
				throw ImFormatExcep("Failed to create out-profile");

			cmsHTRANSFORM hTransform = cmsCreateTransform(
				hInProfile, 
				TYPE_CMYK_8, 
				hOutProfile, 
				TYPE_RGB_8, 
				INTENT_PERCEPTUAL, 
				0
			);
			if(!hTransform)
				throw ImFormatExcep("Failed to create transform");

			cmsCloseProfile(hInProfile); 
			cmsCloseProfile(hOutProfile); 

			int y = 0;
			while (cinfo.output_scanline < cinfo.output_height)
			{
				jpeg_read_scanlines(&cinfo, scanline_ptrs, 1);

				for(int x=0; x<final_W_pixels * 4; ++x)
					modified_buffer[x] = 255 - buffer[x];

				// Transform scanline from CMYK to sRGB
				cmsDoTransform(
					hTransform, 
					&modified_buffer[0], // input buffer
					texture->getPixel(0, y), //  output buffer,
					cinfo.output_width // Size in pixels
				);
			
				++y;
			}

			cmsDeleteTransform(hTransform); 
		}
		else
		{
			if(cinfo.num_components == 4)
				throw ImFormatExcep("Invalid num components " + toString(cinfo.num_components) + " for non-CMYK colour space");

			// Build array of scanline pointers
			const unsigned int H = texture->getHeight();
			std::vector<uint8_t*> scanline_ptrs(H);
			for(unsigned int i=0; i<H; ++i)
				scanline_ptrs[i] = texture->getPixel(0, i);


			// Read scanlines.  jpeg_read_scanlines doesn't do all scanlines at once so need to loop until done.
			while(cinfo.output_scanline < cinfo.output_height)
			{
				jpeg_read_scanlines(&cinfo, &scanline_ptrs[cinfo.output_scanline], (JDIMENSION)H - cinfo.output_scanline);
			}

#if IMAGE_MAP_TILED
#error TODO
#endif
		}

		jpeg_finish_decompress(&cinfo);
		
		//conPrint("   Finished, elapsed: " + timer.elapsedString() + ", total time: " + doubleToStringNDecimalPlaces(total_jpeg_decoding_time, 3) + "s");
		//total_jpeg_decoding_time += timer.elapsed();

		return texture;
	}
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


// RAII wrapper for jpeg_compress_struct, so jpeg_destroy_compress() will always be called, even if an exception is thrown.
class JpegCompress
{
public:
	JpegCompress() {}
	~JpegCompress() { jpeg_destroy_compress(&cinfo); }

	struct jpeg_compress_struct cinfo;
};


// Saves a JPEG with 95% image quality, and in the sRGB colour space. (embeds an ICC sRGB colour profile)
void JPEGDecoder::save(const Reference<ImageMapUInt8>& image, const std::string& path)
{
	try
	{
		// Open file to write to.
		FileHandle file_handle(path, "wb");

		JpegCompress compress;
		jpeg_compress_struct& cinfo = compress.cinfo;
	
		// Set up error handler
		struct jpeg_error_mgr error_manager;
		jpeg_std_error(&error_manager); // Initialise.  NOTE: Assuming this won't throw an exception.
		error_manager.emit_message = my_emit_message;
		error_manager.error_exit = my_error_exit;
		error_manager.output_message = my_output_message;
		
		cinfo.err = &error_manager; // Set error manager


		jpeg_create_compress(&cinfo); // Create compress struct.  Should come after we set the error handler above.

		jpeg_stdio_dest(&cinfo, file_handle.getFile());
		
		cinfo.image_width = (JDIMENSION)image->getWidth(); // image width and height, in pixels
		cinfo.image_height = (JDIMENSION)image->getHeight();
		cinfo.input_components = image->getN();	// # of color components per pixel
		cinfo.in_color_space = image->getN() >= 3 ? JCS_RGB : JCS_GRAYSCALE; // colorspace of input image

		jpeg_set_defaults(&cinfo); // Set default parameters for compression.
		jpeg_set_quality(&cinfo, 95, TRUE);

		// Build array of scanline pointers
		const unsigned int H = (unsigned int)image->getHeight();
		std::vector<uint8_t*> scanline_ptrs(H);
		for(unsigned int i=0; i<H; ++i)
			scanline_ptrs[i] = (uint8_t*)image->getPixel(0, i);

		jpeg_start_compress(&cinfo, TRUE);

		// Save sRGB Profile
		// "You can write special markers immediately following the datastream header by
		// calling jpeg_write_marker() after jpeg_start_compress() and before the first
		// call to jpeg_write_scanlines()" - libjpeg.txt
		{
			cmsHPROFILE profile = cmsCreate_sRGBProfile();
			if(profile == NULL)
				throw ImFormatExcep("Failed to create colour profile.");

			// Get num bytes needed to store the encoded profile.
			cmsUInt32Number profile_size = 0;
			if(cmsSaveProfileToMem(profile, NULL, &profile_size) == FALSE)
				throw ImFormatExcep("Failed to save colour profile.");

			// ICC_HEADER_SIZE: The ICC signature is 'ICC_PROFILE' (with null terminator) + 2 bytes.
			// See http://www.color.org/specification/ICC1v43_2010-12.pdf  (section B.4 Embedding ICC profiles in JPEG files)
			// and http://repositorium.googlecode.com/svn-history/r164/trunk/FreeImage/Source/PluginJPEG.cpp
			
			const size_t ICC_HEADER_SIZE = 14;
			std::vector<uint8> buf(ICC_HEADER_SIZE + profile_size);

			const char* header_str = "ICC_PROFILE";
			const size_t HEADER_STR_LEN_WITH_NULL = 12;
			std::memcpy(&buf[0], header_str, HEADER_STR_LEN_WITH_NULL);

			// Just assume the profile size is <= 65519 (max size).
			assert(profile_size <= 65519);
			buf[HEADER_STR_LEN_WITH_NULL + 0] = 1; // Sequence number (starts at 1).
			buf[HEADER_STR_LEN_WITH_NULL + 1] = 1; // Number of markers (1).

			// Now write the actual profile.
			if(cmsSaveProfileToMem(profile, &buf[ICC_HEADER_SIZE], &profile_size) == FALSE)
				throw ImFormatExcep("Failed to save colour profile.");

			cmsCloseProfile(profile); 

			const int ICC_MARKER = JPEG_APP0 + 2; // ICC profile marker
			jpeg_write_marker(&cinfo, ICC_MARKER, &buf[0], (unsigned int)buf.size());
		}

		while (cinfo.next_scanline < cinfo.image_height) {
			jpeg_write_scanlines(&cinfo, &scanline_ptrs[cinfo.next_scanline], (JDIMENSION)H - cinfo.next_scanline);
		}

		jpeg_finish_compress(&cinfo);
	}
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void JPEGDecoder::test(const std::string& indigo_base_dir)
{
	conPrint("JPEGDecoder::test()");

	const std::string tempdir = "jpeg_temp_testing_dir";
	if(FileUtils::fileExists(tempdir))
		FileUtils::deleteDirectoryRecursive(tempdir);
	FileUtils::createDirIfDoesNotExist(tempdir);

	const std::string save_path = tempdir + "/saved.jpg";

	// Try loading a JPEG using the RGB colour space
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.jpg");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);

		// Try saving it.
		
		testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
		JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path);

		// Load it again to check it is valid.
		im = JPEGDecoder::decode(indigo_base_dir, save_path);
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Try loading a JPEG using the CMYK colour space
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_CMYK.jpg");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);

		// Try saving it.
		testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
		JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path);

		// Load it again to check it is valid.
		im = JPEGDecoder::decode(indigo_base_dir, save_path);
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try loading a greyscale JPEG
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.jpg");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 1);

		// Try saving it.
		testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
		JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path);

		// Load it again to check it is valid.
		im = JPEGDecoder::decode(indigo_base_dir, save_path);
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 1);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try loading an invalid file
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.bmp");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try loading an absent file
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/not a file.bmp");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try saving an image that can't be saved as a valid JPEG. (5 components)
	try
	{
		Reference<ImageMapUInt8> im = new ImageMapUInt8(10, 10, 5); // 5 components

		// Try saving it.
		testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
		JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path);

		failTest("Expected failure.");
	}
	catch(ImFormatExcep&)
	{
	}

	// Try saving an image that can't be saved as a valid JPEG. (zero width and height)
	try
	{
		Reference<ImageMapUInt8> im = new ImageMapUInt8(0, 0, 3); // 5 components

		// Try saving it.
		testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
		JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path);

		failTest("Expected failure.");
	}
	catch(ImFormatExcep&)
	{
	}
}


#endif // BUILD_TESTS
