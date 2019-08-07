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


// We use libjpeg-turbo for loading and saving JPEGs.
// Libjpeg-turbo should be version 2.0.0.  See building_indigo.txt.
static_assert(LIBJPEG_TURBO_VERSION_NUMBER == 2000000, "LIBJPEG_TURBO_VERSION_NUMBER == 2000000");


// See 'Precompute colour profile data' code in PNGDecoder.cpp.
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


// RAII wrapper for jpeg_decompress_struct, so jpeg_destroy() will always be called, even if an exception is thrown.
class JpegDecompress
{
public:
	JpegDecompress() {}
	~JpegDecompress() { jpeg_destroy((j_common_ptr)&cinfo); }

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
			const size_t H = texture->getHeight();
			std::vector<uint8_t*> scanline_ptrs(H);
			for(size_t i=0; i<H; ++i)
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


// RAII wrapper for jpeg_compress_struct, so jpeg_destroy() will always be called, even if an exception is thrown.
class JpegCompress
{
public:
	JpegCompress() {}
	~JpegCompress() { jpeg_destroy((j_common_ptr)&cinfo); }

	struct jpeg_compress_struct cinfo;
};


// Saves a JPEG in the sRGB colour space. (embeds an ICC sRGB colour profile)
void JPEGDecoder::save(const Reference<ImageMapUInt8>& image, const std::string& path, const SaveOptions& options)
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
		cinfo.input_components = (int)image->getN();	// # of color components per pixel
		cinfo.in_color_space = image->getN() >= 3 ? JCS_RGB : JCS_GRAYSCALE; // colorspace of input image

		jpeg_set_defaults(&cinfo); // Set default parameters for compression.
		jpeg_set_quality(&cinfo, options.quality, TRUE);

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
#define SAVE_PRECOMPUTED_SRGB_PROFILE 1
#if SAVE_PRECOMPUTED_SRGB_PROFILE

			// ICC_HEADER_SIZE: The ICC signature is 'ICC_PROFILE' (with null terminator) + 2 bytes.
			// See http://www.color.org/specification/ICC1v43_2010-12.pdf  (section B.4 Embedding ICC profiles in JPEG files)
			// and http://repositorium.googlecode.com/svn-history/r164/trunk/FreeImage/Source/PluginJPEG.cpp

			const size_t ICC_HEADER_SIZE = 14;
			std::vector<uint8> buf(ICC_HEADER_SIZE + sRGB_profile_data_size);

			const char* header_str = "ICC_PROFILE";
			const size_t HEADER_STR_LEN_WITH_NULL = 12;
			std::memcpy(&buf[0], header_str, HEADER_STR_LEN_WITH_NULL);

			assert(sRGB_profile_data_size <= 65519);
			buf[HEADER_STR_LEN_WITH_NULL + 0] = 1; // Sequence number (starts at 1).
			buf[HEADER_STR_LEN_WITH_NULL + 1] = 1; // Number of markers (1).

			std::memcpy(&buf[ICC_HEADER_SIZE], sRGB_profile_data, sRGB_profile_data_size); // Now write the actual profile.

			const int ICC_MARKER = JPEG_APP0 + 2; // ICC profile marker
			jpeg_write_marker(&cinfo, ICC_MARKER, &buf[0], (unsigned int)buf.size());

#else // SAVE_PRECOMPUTED_SRGB_PROFILE

			cmsHPROFILE profile = cmsCreate_sRGBProfile();
			if(profile == NULL)
				throw ImFormatExcep("Failed to create colour profile.");

			// Get num bytes needed to store the encoded profile.
			cmsUInt32Number profile_size = 0;
			if(cmsSaveProfileToMem(profile, NULL, &profile_size) == FALSE)
				throw ImFormatExcep("Failed to save colour profile.");

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

#endif // SAVE_PRECOMPUTED_SRGB_PROFILE
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
#include "../utils/PlatformUtils.h"


void JPEGDecoder::test(const std::string& indigo_base_dir)
{
	conPrint("JPEGDecoder::test()");

	try
	{
		// Perf test
		if(true)
		{
			{
				Timer timer;
				Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/world.200401.3x5400x2700.jpg");
				testAssert(im->getMapWidth() == 5400 && im->getMapHeight() == 2700);
				conPrint("Elapsed time for 'world.200401.3x5400x2700.jpg': " + timer.elapsedStringNSigFigs(5));
			}
			{
				Timer timer;
				Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/brickwork_normal-map.jpg");
				testAssert(im->getMapWidth() == 512 && im->getMapHeight() == 512);
				conPrint("Elapsed time for 'brickwork_normal-map.jpg':   " + timer.elapsedStringNSigFigs(5));
			}
			{
				Timer timer;
				Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/preview_squaretile.jpg");
				testAssert(im->getMapWidth() == 400 && im->getMapHeight() == 400);
				conPrint("Elapsed time for 'preview_squaretile.jpg':     " + timer.elapsedStringNSigFigs(5));
			}
		}



		const std::string tempdir = PlatformUtils::getTempDirPath() + "/jpeg_temp_testing_dir";
		if(FileUtils::fileExists(tempdir))
			FileUtils::deleteDirectoryRecursive(tempdir);
		FileUtils::createDirIfDoesNotExist(tempdir);
	

		const std::string save_path = tempdir + "/saved.jpg";

		// Try loading a JPEG using the RGB colour space
		conPrint("test 1");
		try
		{
			Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.jpg");
			testAssert(im->getMapWidth() == 1080);
			testAssert(im->getMapHeight() == 768);
			testAssert(im->getBytesPerPixel() == 3);

			// Try saving it.
		
			testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
			JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path, JPEGDecoder::SaveOptions());

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
		conPrint("test 2");
		try
		{
			Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_CMYK.jpg");
			testAssert(im->getMapWidth() == 1080);
			testAssert(im->getMapHeight() == 768);
			testAssert(im->getBytesPerPixel() == 3);

			// Try saving it.
			testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
			JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path, JPEGDecoder::SaveOptions());

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
		conPrint("test 3");
		try
		{
			Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.jpg");
			testAssert(im->getMapWidth() == 1080);
			testAssert(im->getMapHeight() == 768);
			testAssert(im->getBytesPerPixel() == 1);

			// Try saving it.
			testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
			JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path, JPEGDecoder::SaveOptions());

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
		conPrint("test 4");
		try
		{
			Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.bmp");
			failTest("Shouldn't get here.");
		}
		catch(ImFormatExcep&)
		{}

		// Try loading an absent file
		conPrint("test 5");
		try
		{
			Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/not a file.bmp");
			failTest("Shouldn't get here.");
		}
		catch(ImFormatExcep&)
		{}

		// Try saving an image that can't be saved as a valid JPEG. (5 components)
		conPrint("test 6");
		try
		{
			Reference<ImageMapUInt8> im = new ImageMapUInt8(10, 10, 5); // 5 components

			// Try saving it.
			testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
			JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path, JPEGDecoder::SaveOptions());

			failTest("Expected failure.");
		}
		catch(ImFormatExcep&)
		{
		}

		// Try saving an image that can't be saved as a valid JPEG. (zero width and height)
		conPrint("test 7");
		try
		{
			Reference<ImageMapUInt8> im = new ImageMapUInt8(0, 0, 3); // 5 components

			// Try saving it.
			testAssert(dynamic_cast<const ImageMapUInt8*>(im.getPointer()) != NULL);
			JPEGDecoder::save(im.downcast<ImageMapUInt8>(), save_path, JPEGDecoder::SaveOptions());

			failTest("Expected failure.");
		}
		catch(ImFormatExcep&)
		{
		}

	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
