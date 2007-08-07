/*=====================================================================
jpegdecoder.cpp
---------------
File created by ClassTemplate on Sat Apr 27 16:22:59 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "jpegdecoder.h"

#include "../graphics/bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "imformatdecoder.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"

//TEMP NO JPEG SUPPORT #define LIBJPEG_SUPPORT 1

#ifdef LIBJPEG_SUPPORT
extern "C"
{
#include <jpeglib.h>
}
#endif LIBJPEG_SUPPORT

JPEGDecoder::JPEGDecoder()
{
}


JPEGDecoder::~JPEGDecoder()
{
}


#ifdef LIBJPEG_SUPPORT
static void my_error_exit(j_common_ptr cinfo)
{
	const std::string errormsg = cinfo->err->last_jpeg_message >= 0 ? std::string(cinfo->err->jpeg_message_table[0]) : "Unkown error.";
	throw ImFormatExcep("Error while reading JPEG file: " + errormsg);
}


void JPEGDecoder::decode(const std::vector<unsigned char>& srcdata, const std::string& path, Bitmap& img_out)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	
	//------------------------------------------------------------------------
	//set error handling
	//------------------------------------------------------------------------
	//cinfo.err = jpeg_std_error(&jerr.pub);
	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = my_error_exit;

	//------------------------------------------------------------------------
	//Init main struct
	//------------------------------------------------------------------------
	jpeg_create_decompress(&cinfo);

	//------------------------------------------------------------------------
	//Open file
	//------------------------------------------------------------------------
	FILE* infile = fopen(path.c_str(), "rb");
	if(!infile)
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");

	
	
	jpeg_stdio_src(&cinfo, infile);


	//------------------------------------------------------------------------
	//read header
	//------------------------------------------------------------------------
	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);

	if(cinfo.num_components != 3)
		throw ImFormatExcep("Only 3 component JPEGs are currently supported.");

	img_out.setBytesPP(3);
	img_out.resize(cinfo.output_width, cinfo.output_height);


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

		memcpy(img_out.getPixel(0, y), buffer[0], row_stride);
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
}


#else //LIBJPEG_SUPPORT

void JPEGDecoder::decode(const std::vector<unsigned char>& srcdata, const std::string& path, Bitmap& img_out)
{
	::fatalError("JPEGDecoder::decode: LIBJPEG_SUPPORT disabled.");
}

#endif //LIBJPEG_SUPPORT




#if 0


//I got this off the FLIPCODE IOTD.  thanks FLipCode!!!, thanks John!!!
//  ... and I suppose thanks Intel :) -- nickc

//############################################################################
//##                                                                        ##
//##  JPEG.H                                                                ##
//##                                                                        ##
//##  Wrapper class to compress or decompress a jpeg from a block of memory ##
//##  using the Intel Jpeg Library.                                         ##
//##  OpenSourced 2/4/2000 by John W. Ratcliff                              ##
//##                                                                        ##
//##  No warranty expressed or implied.  Released as part of the triangle   ##
//##  throughput testbed project.                                           ##
//############################################################################
//##                                                                        ##
//##  Contact John W. Ratcliff at jratcliff@verant.com                      ##
//############################################################################


void JPEGDecoder::decode(const std::vector<unsigned char>& srcdata, Bitmap& img_out)
{
	int width, height, bytes_pp;

	void* decodedimage = ReadImage(width, height, bytes_pp, &(*srcdata.begin()), 
		srcdata.size());

	if(!decodedimage)
		throw ImFormatExcep("Jpeg decoding failed.");


	img_out.takePointer(width, height, bytes_pp, (unsigned char*)decodedimage);
}



/*void* JPEGDecoder::decode(const void* image, int numimagebytes, //size of encoded image
					int& bpp_out, int& width_out, int& height_out)
{
	void* decodedimage = ReadImage(width_out, height_out, bpp_out, image, numimagebytes);

	bpp_out *= 8;//NOTE: CHECKME

	return decodedimage;
}*/



// read image into this buffer.
void * JPEGDecoder::ReadImage(int &width,
                       int &height,
                       int &nchannels,
                       const void *buffer,
                       int sizebytes)
{
  JPEG_CORE_PROPERTIES jcprops;

  if ( ijlInit(&jcprops) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }

  jcprops.JPGBytes = (unsigned char *) buffer;
  jcprops.JPGSizeBytes = sizebytes;
  jcprops.jquality = 100;

  if ( ijlRead(&jcprops,IJL_JBUFF_READPARAMS) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }

  width  = jcprops.JPGWidth;
  height = jcprops.JPGHeight;
  IJLIOTYPE mode;

  mode = IJL_JBUFF_READWHOLEIMAGE;
  nchannels = jcprops.JPGChannels;
  unsigned char * pixbuff = new unsigned char[width*height*nchannels];
  if ( !pixbuff )
  {
    ijlFree(&jcprops);
    return 0;
  }

  jcprops.DIBWidth  = width;
  jcprops.DIBHeight = height;
  jcprops.DIBChannels = nchannels;
  jcprops.DIBPadBytes = 0;
  jcprops.DIBBytes = (unsigned char *)pixbuff;

  if ( jcprops.JPGChannels == 3 )
  {
    jcprops.DIBColor = IJL_RGB;
    jcprops.JPGColor = IJL_YCBCR;
    jcprops.JPGSubsampling = IJL_411;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }
  else
  {
    jcprops.DIBColor = IJL_G;
    jcprops.JPGColor = IJL_G;
    jcprops.JPGSubsampling = (IJL_JPGSUBSAMPLING) 0;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }

  if ( ijlRead(&jcprops, mode) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }

  if ( ijlFree(&jcprops) != IJL_OK ) return 0;

  return (void *)pixbuff;
}


void * JPEGDecoder::Compress(const void *source,
                      int width,
                      int height,
                      int bpp,
                      int &len,
                      int quality)
{
  JPEG_CORE_PROPERTIES jcprops;

  if ( ijlInit(&jcprops) != IJL_OK )
  {
    ijlFree(&jcprops);
    return false;
  }

  jcprops.DIBWidth    = width;
  jcprops.DIBHeight   = height;
  jcprops.JPGWidth    = width;
  jcprops.JPGHeight   = height;
  jcprops.DIBBytes    = (unsigned char *) source;
  jcprops.DIBPadBytes = 0;
  jcprops.DIBChannels = bpp;
  jcprops.JPGChannels = bpp;

  if ( bpp == 3 )
  {
    jcprops.DIBColor = IJL_RGB;
    jcprops.JPGColor = IJL_YCBCR;
    jcprops.JPGSubsampling = IJL_411;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }
  else
  {
    jcprops.DIBColor = IJL_G;
    jcprops.JPGColor = IJL_G;
    jcprops.JPGSubsampling = (IJL_JPGSUBSAMPLING) 0;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }

  int size = width*height*bpp;

  unsigned char * buffer = new unsigned char[size];

  jcprops.JPGSizeBytes = size;
  jcprops.JPGBytes     = buffer;

  jcprops.jquality = quality;


  if ( ijlWrite(&jcprops,IJL_JBUFF_WRITEWHOLEIMAGE) != IJL_OK )
  {
    ijlFree(&jcprops);
    delete buffer;
    return 0;
  }


  if ( ijlFree(&jcprops) != IJL_OK )
  {
    delete buffer;
    return 0;
  }

  len = jcprops.JPGSizeBytes;
  return buffer;
}


#endif