/*=====================================================================
jpegdecoder.cpp
---------------
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "jpegdecoder.h"


#include "../graphics/texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "imformatdecoder.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include <jpeglib.h>


//#if JPEG_LIB_VERSION < 70
//#error Please compile with Libjpeg 7.0
//#endif


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

	(*cinfo->err->format_message) (cinfo, buffer);
	
	throw ImFormatExcep("Error while reading JPEG file: " + std::string(buffer));
}


Reference<Map2D> JPEGDecoder::decode(/*const std::vector<unsigned char>& srcdata, */const std::string& path)
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
	FILE* infile = FileUtils::openFile(path, "rb");
	if(!infile)
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");

	// Specifiy file data source
	jpeg_stdio_src(&cinfo, infile);


	//------------------------------------------------------------------------
	//read header
	//------------------------------------------------------------------------
	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);

	if(!(cinfo.num_components == 1 || cinfo.num_components == 3))
		throw ImFormatExcep("Only 1 or 3 component JPEGs are currently supported.");

	//img_out.setBytesPP(cinfo.num_components);
	//img_out.resize(cinfo.output_width, cinfo.output_height);

	Texture* texture = new Texture();

	texture->resize(cinfo.output_width, cinfo.output_height, cinfo.num_components);


	//------------------------------------------------------------------------
	//alloc row buffer
	//------------------------------------------------------------------------
	const unsigned int row_stride = cinfo.output_width * cinfo.output_components;

	// Make a one-row-high sample array that will go away when done with image
	JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	//------------------------------------------------------------------------
	//read data
	//------------------------------------------------------------------------
	/* Step 6: while (scan lines remain to be read)
             jpeg_read_scanlines(...);

	Here we use the library's state variable cinfo.output_scanline as the
	loop counter, so that we don't have to keep track ourselves.
	*/
	int y = 0;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		/* jpeg_read_scanlines expects an array of pointers to scanlines.
		* Here the array is only one element long, but you could ask for
		* more than one scanline at a time if that's more convenient.
		*/
		jpeg_read_scanlines(&cinfo, buffer, 1);
		/* Assume put_scanline_someplace wants a pointer and sample count. */
		//put_scanline_someplace(buffer[0], row_stride);

		memcpy(texture->rowPointer(y), buffer[0], row_stride);
		++y;
  }

	/* Step 7: Finish decompression */

	jpeg_finish_decompress(&cinfo);
	/* We can ignore the return value since suspension is not possible
	* with the stdio data source.
	*/

	/* Step 8: Release JPEG decompression object */

	/* This is an important step since it will release a good deal of memory. */
	jpeg_destroy_decompress(&cinfo);

	/* After finish_decompress, we can close the input file.
	* Here we postpone it until after no more JPEG errors are possible,
	* so as to simplify the setjmp error logic above.  (Actually, I don't
	* think that jpeg_destroy can do an error exit, but why assume anything...)
	*/
	fclose(infile);

	return Reference<Map2D>(texture);
}


/*
void JPEGDecoder::decode(const std::vector<unsigned char>& srcdata, const std::string& path, Bitmap& img_out)
{
	::fatalError("JPEGDecoder::decode: LIBJPEG_SUPPORT disabled.");
}
*/
