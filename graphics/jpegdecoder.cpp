/*=====================================================================
jpegdecoder.cpp
---------------
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "jpegdecoder.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "imformatdecoder.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/MemMappedFile.h"
#include "../utils/timer.h"
#include "../graphics/ImageMap.h"
#include <jpeglib.h>
#include <lcms2.h>

/*
//#if JPEG_LIB_VERSION < 70
//#error Please compile with Libjpeg 7.0
//#endif
*/

JPEGDecoder::JPEGDecoder()
{
}


JPEGDecoder::~JPEGDecoder()
{
}


static void my_error_exit(j_common_ptr cinfo)
{
	// Format message into buffer.
	char buffer[JMSG_LENGTH_MAX];
	memset(buffer, 0, sizeof(buffer));

	(*cinfo->err->format_message) (cinfo, buffer);
	
	throw ImFormatExcep("Error while reading JPEG file: " + std::string(buffer));
}


//static double total_jpeg_decoding_time = 0;


Reference<Map2D> JPEGDecoder::decode(const std::string& indigo_base_dir, const std::string& path)
{
	//conPrint("JPEGDecoder::decode(), path: " + path);
	//Timer timer;
	try
	{
		//------------------------------------------------------------------------
		//set error handling
		//------------------------------------------------------------------------
		struct jpeg_error_mgr error_manager;

		// Fill in standard error-handling methods
		jpeg_std_error(&error_manager);

		// Override error_exit;
		error_manager.error_exit = my_error_exit;

		//------------------------------------------------------------------------
		//Init main struct
		//------------------------------------------------------------------------
		struct jpeg_decompress_struct cinfo;

		// Set error manager
		cinfo.err = &error_manager;

		jpeg_create_decompress(&cinfo);

		//------------------------------------------------------------------------
		//Open file
		//------------------------------------------------------------------------
		FileHandle infile(path, "rb");

		// Specify file data source
		jpeg_stdio_src(&cinfo, infile.getFile());


		// Make libjpeg save the 'special markers'
		//jpeg_save_markers(&cinfo, JPEG_COM, 0xFFFF);
		//for(int i=0; i<16; ++i)
		//	jpeg_save_markers(&cinfo, JPEG_APP0 + i, 0xFFFF);


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
		
		// Release JPEG decompression object
		jpeg_destroy_decompress(&cinfo);

		//conPrint("   Finished, elapsed: " + timer.elapsedString() + ", total time: " + doubleToStringNDecimalPlaces(total_jpeg_decoding_time, 3) + "s");
		//total_jpeg_decoding_time += timer.elapsed();

		return texture;
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
	// Try loading a JPEG using the RGB colour space
	try
	{
		Reference<Map2D> im = JPEGDecoder::decode(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.jpg");
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
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	
}


#endif // BUILD_TESTS

