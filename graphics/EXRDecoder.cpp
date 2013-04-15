/*=====================================================================
EXRDecoder.cpp
--------------
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "EXRDecoder.h"


#include "imformatdecoder.h"
#include "../graphics/image.h"
#include "../graphics/ImageMap.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/Exception.h"
#include "../utils/platformutils.h"
#include <ImfRgbaFile.h>
#include <ImathBox.h>
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

		bool has_alpha = false;

		for(Imf::ChannelList::ConstIterator i = file.header().channels().begin(); i != file.header().channels().end(); ++i)
		{
			const std::string channel_name = i.name();
			if(channel_name == "a" || channel_name == "A")
				has_alpha = true;
			const Imf::Channel& channel = i.channel();

			const Imf::PixelType pixel_type = channel.type;

			use_pixel_type = pixel_type;
		}

		const int use_num_channels = has_alpha ? 4 : 3;

		Imath::Box2i dw = file.header().dataWindow();
		const int width = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		Imf::FrameBuffer frameBuffer;

		if(use_pixel_type == Imf::FLOAT)
		{
			ImageMap<float, FloatComponentValueTraits>* new_image = new ImageMap<float, FloatComponentValueTraits>(width, height, use_num_channels);
			new_image->setGamma(1); // HDR images should have gamma 1.

			const size_t x_stride = sizeof(float) * use_num_channels;
			const size_t y_stride = sizeof(float) * use_num_channels * width;

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 0),			// base
				x_stride,
				y_stride)
			);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 1),			// base
				x_stride,
				y_stride)
			);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)(new_image->getData() + 2),			// base
				x_stride,
				y_stride)
			);

			if(has_alpha)
			{
				frameBuffer.insert("A",				// name
					Imf::Slice(Imf::FLOAT,			// type
					(char*)(new_image->getData() + 3),			// base
					x_stride,
					y_stride)
				);
			}

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return Reference<Map2D>(new_image);
		}
		else if(use_pixel_type == Imf::HALF)
		{
			ImageMap<half, HalfComponentValueTraits>* new_image = new ImageMap<half, HalfComponentValueTraits>(width, height, use_num_channels);
			new_image->setGamma(1); // HDR images should have gamma 1.

			const size_t x_stride = sizeof(half) * use_num_channels;
			const size_t y_stride = sizeof(half) * use_num_channels * width;

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 0),			// base
				x_stride,
				y_stride)
			);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 1),			// base
				x_stride,
				y_stride)
			);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 2),			// base
				x_stride,
				y_stride)
			);

			if(has_alpha)
			{
				frameBuffer.insert("A",				// name
					Imf::Slice(Imf::HALF,			// type
					(char*)(new_image->getData() + 3),			// base
					x_stride,
					y_stride)
				);
			}

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return Reference<Map2D>(new_image);
		}
		else
		{
			throw ImFormatExcep("EXR pixel type must be HALF or FLOAT.");
		}
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


void EXRDecoder::saveImageTo32BitEXR(const Image4f& image, const std::string& pathname)
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
		header.channels().insert("A", Imf::Channel(Imf::FLOAT));
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		frameBuffer.insert("R",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).x[0],			// base
			sizeof(Image4f::ColourType),				// xStride
			sizeof(Image4f::ColourType) * image.getWidth())// yStride
			);
		frameBuffer.insert("G",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).x[1],			// base
			sizeof(Image4f::ColourType),				// xStride
			sizeof(Image4f::ColourType) * image.getWidth())// yStride
			);
		frameBuffer.insert("B",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).x[2],			// base
			sizeof(Image4f::ColourType),				// xStride
			sizeof(Image4f::ColourType) * image.getWidth())// yStride
			);
		frameBuffer.insert("A",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0).x[3],			// base
			sizeof(Image4f::ColourType),				// xStride
			sizeof(Image4f::ColourType) * image.getWidth())// yStride
			);
		file.setFrameBuffer(frameBuffer);
		file.writePixels((int)image.getHeight());
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}


//NOTE: Untested.
void EXRDecoder::saveImageTo32BitEXR(const ImageMapFloat& image, const std::string& pathname)
{
	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)image.getWidth(), (int)image.getHeight());


		header.channels().insert("R", Imf::Channel(Imf::FLOAT));

		if(!(image.getN() == 1 || image.getN() == 3))
			throw Indigo::Exception("Require 1 or 3 components.");

		if(image.getN() == 3)
		{
			header.channels().insert("G", Imf::Channel(Imf::FLOAT));
			header.channels().insert("B", Imf::Channel(Imf::FLOAT));
		}
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		frameBuffer.insert("R",				// name
			Imf::Slice(Imf::FLOAT,			// type
			(char*)&image.getPixel(0, 0)[0],			// base
			image.getBytesPerPixel(),				// xStride
			image.getBytesPerPixel() * image.getWidth())// yStride
			);

		if(image.getN() == 3)
		{
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)&image.getPixel(0, 0)[1],			// base
				image.getBytesPerPixel(),				// xStride
				image.getBytesPerPixel() * image.getWidth())// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::FLOAT,			// type
				(char*)&image.getPixel(0, 0)[2],			// base
				image.getBytesPerPixel(),				// xStride
				image.getBytesPerPixel() * image.getWidth())// yStride
				);
		}
		file.setFrameBuffer(frameBuffer);
		file.writePixels((int)image.getHeight());
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}
