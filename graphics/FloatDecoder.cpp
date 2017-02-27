/*=====================================================================
FloatDecoder.cpp
----------------
File created by ClassTemplate on Wed Jul 30 07:25:31 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "FloatDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/FileUtils.h"
#include "../utils/MemMappedFile.h"
#include "../maths/mathstypes.h"


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
		MemMappedFile file(pathname);

		// Since there is no header, and assuming the image is square, we can work out the size
		const size_t pixel_size = sizeof(float) * 3;
		const size_t num_pixels = file.fileSize() / pixel_size;
		
		const size_t width = (size_t)sqrt((double)num_pixels);

		if((width == 0) || (width * width != num_pixels))
			throw ImFormatExcep("Image has invalid size");

		ImageMapFloatRef image = new ImageMapFloat((unsigned int)width, (unsigned int)width, 3);

		std::memcpy(image->getPixel(0, 0), file.fileData(), width * width * pixel_size);

		return image;
	}
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep("Error reading .float file: " + std::string(e.what()));
	}
}
