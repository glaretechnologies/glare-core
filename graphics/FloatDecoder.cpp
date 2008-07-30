/*=====================================================================
FloatDecoder.cpp
----------------
File created by ClassTemplate on Wed Jul 30 07:25:31 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "FloatDecoder.h"


#include "../utils/FileUtils.h"
#include <vector>
#include <math.h>
#include "imformatdecoder.h"
#include "image.h"


FloatDecoder::FloatDecoder()
{
	
}


FloatDecoder::~FloatDecoder()
{
	
}


Reference<Map2D> FloatDecoder::decode(const std::string& pathname)
{
	try
	{
		std::vector<unsigned char> data;
		FileUtils::readEntireFile(pathname, data);

		// Since there is no header, and assuming the image is square, we can work out the size
		const unsigned int pixel_size = sizeof(float) * 3;
		const unsigned int num_pixels = data.size() / pixel_size;
		
		const int width = (unsigned int)sqrt((double)num_pixels);

		if(width * width != num_pixels)
			throw ImFormatExcep("Image has invalid size");

		Image* image = new Image(width, width);

		for(int y=0; y<width; ++y)
			for(int x=0; x<width; ++x)
			{
				const int srcy = width - 1 - y; // invert the image at load time
				const int src = (x + y*width) * 3;

				image->setPixel(x, y, Colour3f(
					((float*)(&data[0]))[src],
					((float*)(&data[0]))[src + 1],
					((float*)(&data[0]))[src + 2]
				));
			}

		return Reference<Map2D>(image);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw ImFormatExcep("Error reading .float file: " + std::string(e.what()));
	}
}





