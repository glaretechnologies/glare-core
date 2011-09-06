/*=====================================================================
EXRDecoder.cpp
--------------
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "EXRDecoder.h"


//#include "../graphics/FPImageMap16.h"
#include "../graphics/image.h"
#include "../graphics/ImageMap.h"
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include "imformatdecoder.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/Exception.h"
#include "../utils/platformutils.h"
#include <fstream>
#include <ImfStdIO.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <ImfChannelList.h>
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include <IlmThreadPool.h>


EXRDecoder::EXRDecoder()
{
	
}


EXRDecoder::~EXRDecoder()
{
	
}


Reference<Map2D> EXRDecoder::decode(const std::string& pathname)
{
#ifdef OPENEXR_SUPPORT
	try
	{
		// Add some threads to the ILMBase thread-pool object.
		// This greatly speeds up loading of large, compressed EXRs.
		IlmThread::ThreadPool::globalThreadPool().setNumThreads(PlatformUtils::getNumLogicalProcessors());


		std::ifstream infile(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdIFStream exr_ifstream(infile, pathname.c_str());
		

		Imf::InputFile file(exr_ifstream);

		Imf::PixelType use_pixel_type = Imf::FLOAT;

		for(Imf::ChannelList::ConstIterator i = file.header().channels().begin(); i != file.header().channels().end(); ++i)
		{
			const std::string channel_name = i.name();
			const Imf::Channel& channel = i.channel();

			const Imf::PixelType pixel_type = channel.type;

			use_pixel_type = pixel_type;
		}

		Imath::Box2i dw = file.header().dataWindow();
		const int width = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		Imf::FrameBuffer frameBuffer;

		if(use_pixel_type == Imf::FLOAT)
		{
			/*Image* new_image = new Image(width, height);

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)&new_image->getPixel(0).r,			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)&new_image->getPixel(0).g,			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)&new_image->getPixel(0).b,			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);*/

			ImageMap<float, FloatComponentValueTraits>* new_image = new ImageMap<float, FloatComponentValueTraits>(width, height, 3);
			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 0),			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 1),			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 2),			// base
				sizeof(Image::ColourType),				// xStride
				sizeof(Image::ColourType) * width)// yStride
				);

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return Reference<Map2D>(new_image);
		}
		else if(use_pixel_type == Imf::HALF)
		{
			/*FPImageMap16* new_image = new FPImageMap16(width, height);

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(&new_image->getData()[0]),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(&new_image->getData()[1]),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(&new_image->getData()[2]),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);*/

			ImageMap<half, HalfComponentValueTraits>* new_image = new ImageMap<half, HalfComponentValueTraits>(width, height, 3);

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 0),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 1),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 2),			// base
				sizeof(half)*3,				// xStride
				sizeof(half)*3 * width)// yStride
				);

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return Reference<Map2D>(new_image);
		}
		else
		{
			throw ImFormatExcep("EXR pixel type must be HALF or FLOAT.");
		}

/*
		Imf::RgbaInputFile file(exr_ifstream);

		const Imath::Box2i dw = file.dataWindow();

		const unsigned int filewidth = (unsigned int)(dw.max.x - dw.min.x + 1);
		const unsigned int fileheight = (unsigned int)(dw.max.y - dw.min.y + 1);

		if(filewidth < 0 || fileheight < 0)
			throw ImFormatExcep("invalid image dimensions");

		FPImageMap16* image = new FPImageMap16(filewidth, fileheight);

		std::vector<Imf::Rgba> data(filewidth * fileheight);

		file.setFrameBuffer(
			&data[0], // base
			1, // x stride
			filewidth // y stride
			);
		file.readPixels(dw.min.y, dw.max.y);

		unsigned int c=0;
		for(unsigned int y=0; y<fileheight; ++y)
			for(unsigned int x=0; x<filewidth; ++x)
			{
				image->getData()[c++] = data[y*filewidth + x].r;
				image->getData()[c++] = data[y*filewidth + x].g;
				image->getData()[c++] = data[y*filewidth + x].b;
			}

		return Reference<Map2D>(image);*/
	}
	catch(const std::exception& e)
	{
		throw ImFormatExcep("Error reading EXR file: " + std::string(e.what()));
	}

#else //OPENEXR_SUPPORT

	throw ImFormatExcep("OPENEXR_SUPPORT disabled.");

#endif //OPENEXR_SUPPORT
}


void EXRDecoder::saveImageTo32BitEXR(const Image& image, const std::string& pathname)
{
	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)image.getWidth(), (int)image.getHeight());
		header.channels().insert("R", Imf::Channel(Imf::FLOAT));
		header.channels().insert("G", Imf::Channel(Imf::FLOAT));
		header.channels().insert("B", Imf::Channel(Imf::FLOAT));
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		frameBuffer.insert("R",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).r,			// base
			sizeof(Image::ColourType),				// xStride
			sizeof(Image::ColourType) * image.getWidth())// yStride
			);
		frameBuffer.insert("G",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).g,			// base
			sizeof(Image::ColourType),				// xStride
			sizeof(Image::ColourType) * image.getWidth())// yStride
			);
		frameBuffer.insert("B",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).b,			// base
			sizeof(Image::ColourType),				// xStride
			sizeof(Image::ColourType) * image.getWidth())// yStride
			);
		file.setFrameBuffer(frameBuffer);
		file.writePixels((int)image.getHeight());
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}
