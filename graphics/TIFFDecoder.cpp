/*=====================================================================
TIFFDecoder.cpp
---------------
File created by ClassTemplate on Fri May 02 16:51:32 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "TIFFDecoder.h"


#include <tiffio.h>
#include "imformatdecoder.h"
#include "../utils/stringutils.h"
#include "../graphics/ImageMap.h"


TIFFDecoder::TIFFDecoder()
{
}


TIFFDecoder::~TIFFDecoder()
{
}


Reference<Map2D> TIFFDecoder::decode(const std::string& path)
{
#if defined(_WIN32) || defined(_WIN64)
	TIFF* tif = TIFFOpenW(StringUtils::UTF8ToWString(path).c_str(), "r");
#else
	TIFF* tif = TIFFOpen(path.c_str(), "r");
#endif
    if(tif)
	{
		uint32 w, h, image_depth;

		uint16 bits_per_sample, samples_per_pixel;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
		TIFFGetField(tif, TIFFTAG_IMAGEDEPTH, &image_depth);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
	
		if(samples_per_pixel < 1 || samples_per_pixel > 4)
			throw ImFormatExcep("Tiff file has unsupported number of channels: " + toString(samples_per_pixel));
		if(bits_per_sample != 8 && bits_per_sample != 16)
			throw ImFormatExcep("Tiff file has unsupported number of bits per sample: " + toString(bits_per_sample));

		uint32 rows_per_strip = 0;
		TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
	
		const tstrip_t num_strips = TIFFNumberOfStrips(tif);

		Reference<Map2D> result_tex;

		if(bits_per_sample == 8)
		{
			std::vector<unsigned char> buf(TIFFStripSize(tif));

			Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture( new ImageMap<uint8_t, UInt8ComponentValueTraits>(
				w, h, samples_per_pixel
			));

			for(tstrip_t strip = 0; strip < num_strips; strip++)
			{
				const tsize_t num_bytes = TIFFReadEncodedStrip(tif, strip, &buf[0], (tsize_t)buf.size());
				if(num_bytes == -1)
					throw ImFormatExcep("TIFFReadEncodedStrip failed.");

				int i=0;
				for(uint32 row=0; row<rows_per_strip; ++row)
				{
					const uint32 y = strip * rows_per_strip + row;
					if(y < h)
					{
						for(uint32 x=0; x<w; ++x)
							for(uint32 c=0; c<samples_per_pixel; ++c)
								texture->getPixel(x, y)[c] = buf[i++]; // NOTE: Is there row padding?
					}
				}
			}

			result_tex = texture;
		}
		else if(bits_per_sample == 16)
		{
			std::vector<uint16_t> buf(TIFFStripSize(tif) / sizeof(uint16_t));

			Reference<ImageMap<uint16_t, UInt16ComponentValueTraits> > texture( new ImageMap<uint16_t, UInt16ComponentValueTraits>(
				w, h, samples_per_pixel
			));

			for(tstrip_t strip = 0; strip < num_strips; strip++)
			{
				const tsize_t num_bytes = TIFFReadEncodedStrip(tif, strip, &buf[0], (tsize_t)buf.size() * sizeof(uint16_t));
				if(num_bytes == -1)
					throw ImFormatExcep("TIFFReadEncodedStrip failed.");

				int i=0;
				for(uint32 row=0; row<rows_per_strip; ++row)
				{
					const uint32 y = strip * rows_per_strip + row;
					if(y < h)
					{
						for(uint32 x=0; x<w; ++x)
							for(uint32 c=0; c<samples_per_pixel; ++c)
								texture->getPixel(x, y)[c] = buf[i++]; // NOTE: Is there row padding?
					}
				}
			}

			result_tex = texture;
		}
		else
		{
			assert(!"Invalid bits_per_sample");
		}

		TIFFClose(tif);
		return result_tex;
	}
	else // else if !tif
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}
