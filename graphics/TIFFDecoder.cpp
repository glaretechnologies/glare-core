/*=====================================================================
TIFFDecoder.cpp
---------------
File created by ClassTemplate on Fri May 02 16:51:32 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "TIFFDecoder.h"


#include <tiffio.h>
#include "imformatdecoder.h"
#include "bitmap.h"
#include "texture.h"


TIFFDecoder::TIFFDecoder()
{
	
}


TIFFDecoder::~TIFFDecoder()
{
	
}


Reference<Map2D> TIFFDecoder::decode(const std::string& path)
{
	TIFF* tif = TIFFOpen(path.c_str(), "r");
    if(tif)
	{
		uint32 w, h, image_depth;

		uint16 bits_per_sample, samples_per_pixel;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
		TIFFGetField(tif, TIFFTAG_IMAGEDEPTH, &image_depth);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
		const size_t npixels = w * h;
		//raster = (uint32*) _TIFFmalloc(npixels * sizeof (uint32));
		const unsigned char* raster = (unsigned char*)_TIFFmalloc(npixels * sizeof (uint32));
		Texture* texture = new Texture();
		if(raster != NULL)
		{
			if(TIFFReadRGBAImage(tif, w, h, (uint32*)raster, 0) != 0)
			{
				if(samples_per_pixel == 1)
				{
					texture->resize(w, h, samples_per_pixel);
					for(unsigned int y=0; y<h; ++y)
						for(unsigned int x=0; x<w; ++x)
							texture->setPixelComp(x, y, 0, raster[(((h - y - 1) * w) + x) * 4]);
				}
				else
				{
					// Use 3 channels per pixel
					texture->resize(w, h, 3);
					for(unsigned int y=0; y<h; ++y)
						for(unsigned int x=0; x<w; ++x)
							for(unsigned int c=0; c<3; ++c)
								texture->setPixelComp(x, y, c, raster[(((h - y - 1) * w) + x) * 4 + c]);
				}

			}
			else
			{
				_TIFFfree((uint32*)raster);
				throw ImFormatExcep("Failed to read RGBA data from TIFF file.");
			}

			_TIFFfree((uint32*)raster);
		}
		else
			throw ImFormatExcep("Failed to alloc memory while reading from TIFF file.");

		TIFFClose(tif);

		return Reference<Map2D>(texture);
	}
	else
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}
