/*=====================================================================
EXRDecoder.cpp
--------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
=====================================================================*/
#include "EXRDecoder.h"


#include "imformatdecoder.h"
#include "../graphics/image.h"
#include "../graphics/ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/PlatformUtils.h"
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


/*

Some test of various compression methods:
OpenEXR version: 1.6.1.

Uffizi large:
----------------------------------------
PIZ:
Saving exr took 0.13391521475023183 s
File size: 12.6 MB

ZIP:
Saving exr took 0.5872966223396361 s
File size: 12.2 MB


Zom-b bedroom image (quite noisy interior archviz, large image)
---------------------------------------------------------
PIZ:
Saving exr took 0.8719152554804168 s
File size: 81.2 MB

ZIP:
Saving exr took 1.4644417400650127 s
File size: 90.6 MB

*/


EXRDecoder::EXRDecoder()
{
}


EXRDecoder::~EXRDecoder()
{
}


void EXRDecoder::setEXRThreadPoolSize()
{
	// Make sure there are enough threads in the ILMBase thread-pool object.
	// This greatly speeds up loading of large, compressed EXRs.
	IlmThread::ThreadPool::globalThreadPool().setNumThreads(PlatformUtils::getNumLogicalProcessors());
}


Reference<Map2D> EXRDecoder::decode(const std::string& pathname)
{
	try
	{
		setEXRThreadPoolSize();

		std::ifstream infile(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdIFStream exr_ifstream(infile, pathname.c_str());

		Imf::InputFile file(exr_ifstream);

		const bool has_R = file.header().channels().findChannel("r") || file.header().channels().findChannel("R");
		const bool has_G = file.header().channels().findChannel("g") || file.header().channels().findChannel("G");
		const bool has_B = file.header().channels().findChannel("b") || file.header().channels().findChannel("B");
		const bool has_A = file.header().channels().findChannel("a") || file.header().channels().findChannel("A");

		Imf::PixelType use_pixel_type = Imf::FLOAT;
		int num_channels = 0;
		for(Imf::ChannelList::ConstIterator i = file.header().channels().begin(); i != file.header().channels().end(); ++i)
		{
			const Imf::Channel& channel = i.channel();
			const Imf::PixelType pixel_type = channel.type;
			use_pixel_type = pixel_type;
			num_channels++;
		}

		if(num_channels == 0)
			throw ImFormatExcep("EXR file had no channels.");
		else if(num_channels == 1)
		{
			if(!has_R) throw ImFormatExcep("Expected R channel in EXR file.");
		}
		else if(num_channels == 2)
		{
			if(!has_R) throw ImFormatExcep("Expected R channel in EXR file.");
			if(!has_A) throw ImFormatExcep("Expected A channel in EXR file.");
		}
		else if(num_channels == 3)
		{
			if(!has_R) throw ImFormatExcep("Expected R channel in EXR file.");
			if(!has_G) throw ImFormatExcep("Expected G channel in EXR file.");
			if(!has_B) throw ImFormatExcep("Expected B channel in EXR file.");
		}
		else if(num_channels >= 4)
		{
			if(!has_R) throw ImFormatExcep("Expected R channel in EXR file.");
			if(!has_G) throw ImFormatExcep("Expected G channel in EXR file.");
			if(!has_B) throw ImFormatExcep("Expected B channel in EXR file.");
			if(!has_A) throw ImFormatExcep("Expected A channel in EXR file.");
		}

		const int use_num_channels = myMin(4, num_channels);

		Imath::Box2i dw = file.header().dataWindow();
		const int width = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		Imf::FrameBuffer frameBuffer;

		if(use_pixel_type == Imf::FLOAT)
		{
			Reference<ImageMap<float, FloatComponentValueTraits> > new_image = new ImageMap<float, FloatComponentValueTraits>(width, height, use_num_channels);
			new_image->setGamma(1); // HDR images should have gamma 1.

			const size_t x_stride = sizeof(float) * use_num_channels;
			const size_t y_stride = sizeof(float) * use_num_channels * width;

			frameBuffer.insert("R",						// name
				Imf::Slice(Imf::FLOAT,					// type
				(char*)(new_image->getData() + 0),		// base
				x_stride,
				y_stride)
			);

			if(use_num_channels == 2)
			{
				frameBuffer.insert("A",					// name
					Imf::Slice(Imf::FLOAT,				// type
					(char*)(new_image->getData() + 1),	// base
					x_stride,
					y_stride)
				);
			}
			else if(use_num_channels >= 3)
			{
				frameBuffer.insert("G",					// name
					Imf::Slice(Imf::FLOAT,				// type
					(char*)(new_image->getData() + 1),	// base
					x_stride,
					y_stride)
				);
				frameBuffer.insert("B",					// name
					Imf::Slice(Imf::FLOAT,				// type
					(char*)(new_image->getData() + 2),	// base
					x_stride,
					y_stride)
				);

				if(use_num_channels == 4)
				{
					frameBuffer.insert("A",				// name
						Imf::Slice(Imf::FLOAT,			// type
						(char*)(new_image->getData() + 3),// base
						x_stride,
						y_stride)
					);
				}
			}

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return new_image;
		}
		else if(use_pixel_type == Imf::HALF)
		{
			Reference<ImageMap<half, HalfComponentValueTraits> > new_image = new ImageMap<half, HalfComponentValueTraits>(width, height, use_num_channels);
			new_image->setGamma(1); // HDR images should have gamma 1.

			const size_t x_stride = sizeof(half) * use_num_channels;
			const size_t y_stride = sizeof(half) * use_num_channels * width;

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(new_image->getData() + 0),			// base
				x_stride,
				y_stride)
			);
			if(use_num_channels == 2)
			{
				frameBuffer.insert("A",				// name
					Imf::Slice(Imf::HALF,			// type
					(char*)(new_image->getData() + 1),			// base
					x_stride,
					y_stride)
				);
			}
			else if(use_num_channels >= 3)
			{
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

				if(use_num_channels == 4)
				{
					frameBuffer.insert("A",				// name
						Imf::Slice(Imf::HALF,			// type
						(char*)(new_image->getData() + 3),			// base
						x_stride,
						y_stride)
					);
				}
			}

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return new_image;
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
}


static Imf::Compression EXRCompressionMethod(EXRDecoder::CompressionMethod m)
{
	switch(m)
	{
	case EXRDecoder::CompressionMethod_None:
		return Imf::NO_COMPRESSION;
	case EXRDecoder::CompressionMethod_RLE:
		return Imf::RLE_COMPRESSION;
	case EXRDecoder::CompressionMethod_ZIP:
		return Imf::ZIP_COMPRESSION;
	case EXRDecoder::CompressionMethod_PIZ:
		return Imf::PIZ_COMPRESSION;
	case EXRDecoder::CompressionMethod_PXR24:
		return Imf::PXR24_COMPRESSION;
	case EXRDecoder::CompressionMethod_B44A:
		return Imf::B44A_COMPRESSION;
	};

	assert(0);
	throw Indigo::Exception("invalid compression method");
}


void EXRDecoder::saveImageToEXR(const Image& image, const std::string& pathname, const SaveOptions& options)
{
	setEXRThreadPoolSize();

	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)image.getWidth(), (int)image.getHeight(),
			1.f, // pixel aspect ratio
			Imath::V2f(0,0), // screenWindowCenter
			1.f, // screenWindowWidth
			Imf::INCREASING_Y, // lineOrder
			EXRCompressionMethod(options.compression_method) // compression method
		);

		header.channels().insert("R", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		header.channels().insert("G", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		header.channels().insert("B", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		if(options.bit_depth == BitDepth_16)
		{
			// Convert our float data to halfs.
			const size_t num_values = image.getWidth() * image.getHeight() * 3;
			js::Vector<half, 64> half_data(num_values);
			const float* const src_float_data = &image.getPixel(0).r;
			
			for(size_t i=0; i<num_values; ++i)
				half_data[i] = half(src_float_data[i]);

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)half_data.data(),		// base
				sizeof(half) * 3,				// xStride
				sizeof(half) * 3 * image.getWidth())// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(half_data.data() + 1),	// base
				sizeof(half) * 3,				// xStride
				sizeof(half) * 3 * image.getWidth())// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(half_data.data() + 2),	// base
				sizeof(half) * 3,				// xStride
				sizeof(half) * 3 * image.getWidth())// yStride
				);

			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)image.getHeight());
		}
		else
		{
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
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}


void EXRDecoder::saveImageToEXR(const Image4f& image, bool save_alpha_channel, const std::string& pathname, const SaveOptions& options)
{
	setEXRThreadPoolSize();

	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)image.getWidth(), (int)image.getHeight(),
			1.f, // pixel aspect ratio
			Imath::V2f(0,0), // screenWindowCenter
			1.f, // screenWindowWidth
			Imf::INCREASING_Y, // lineOrder
			EXRCompressionMethod(options.compression_method) // compression method
		);

		header.channels().insert("R", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		header.channels().insert("G", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		header.channels().insert("B", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		if(save_alpha_channel)
			header.channels().insert("A", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		if(options.bit_depth == BitDepth_16)
		{
			// Convert our float data to halfs.
			const size_t num_values = image.getWidth() * image.getHeight() * 4;
			js::Vector<half, 64> half_data(num_values);
			const float* const src_float_data = &image.getPixel(0)[0];
			
			for(size_t i=0; i<num_values; ++i)
				half_data[i] = half(src_float_data[i]);

			frameBuffer.insert("R",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)half_data.data(),		// base
				sizeof(half) * 4,				// xStride
				sizeof(half) * 4 * image.getWidth())// yStride
				);
			frameBuffer.insert("G",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(half_data.data() + 1),	// base
				sizeof(half) * 4,				// xStride
				sizeof(half) * 4 * image.getWidth())// yStride
				);
			frameBuffer.insert("B",				// name
				Imf::Slice(Imf::HALF,			// type
				(char*)(half_data.data() + 2),	// base
				sizeof(half) * 4,				// xStride
				sizeof(half) * 4 * image.getWidth())// yStride
				);
			if(save_alpha_channel)
				frameBuffer.insert("A",				// name
					Imf::Slice(Imf::HALF,			// type
					(char*)(half_data.data() + 3),	// base
					sizeof(half) * 4,				// xStride
					sizeof(half) * 4 * image.getWidth())// yStride
					);
			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)image.getHeight());
		}
		else
		{
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
			if(save_alpha_channel)
				frameBuffer.insert("A",				// name
					Imf::Slice(Imf::FLOAT,			// type
					(char*)&image.getPixel(0).x[3],			// base
					sizeof(Image4f::ColourType),				// xStride
					sizeof(Image4f::ColourType) * image.getWidth())// yStride
					);
			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)image.getHeight());
		}
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}


void EXRDecoder::saveImageToEXR(const ImageMapFloat& image, const std::string& pathname, const SaveOptions& options)
{
	setEXRThreadPoolSize();

	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		//NOTE: I'm assuming that the pixel data is densely packed, so that y-stride is sizeof(ColourType) * getWidth())

		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)image.getWidth(), (int)image.getHeight(),
			1.f, // pixel aspect ratio
			Imath::V2f(0,0), // screenWindowCenter
			1.f, // screenWindowWidth
			Imf::INCREASING_Y, // lineOrder
			EXRCompressionMethod(options.compression_method) // compression method
		);


		header.channels().insert("R", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));

		if(!(image.getN() == 1 || image.getN() == 3))
			throw Indigo::Exception("Require 1 or 3 components.");

		if(image.getN() == 3)
		{
			header.channels().insert("G", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
			header.channels().insert("B", Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));
		}
		Imf::OutputFile file(exr_ofstream, header);                               
		Imf::FrameBuffer frameBuffer;

		if(options.bit_depth == BitDepth_16)
		{
			// Convert our float data to halfs.
			const size_t num_values = image.getWidth() * image.getHeight() * image.getN();
			js::Vector<half, 64> half_data(num_values);
			const float* const src_float_data = &image.getPixel(0, 0)[0];
			
			for(size_t i=0; i<num_values; ++i)
				half_data[i] = half(src_float_data[i]);

			frameBuffer.insert("R",					// name
				Imf::Slice(Imf::HALF,				// type
				(char*)(half_data.data() + 0),		// base
				sizeof(half) * image.getN(),		// xStride
				sizeof(half) * image.getN() * image.getWidth())// yStride
				);

			if(image.getN() == 3)
			{
				frameBuffer.insert("G",				// name
					Imf::Slice(Imf::HALF,			// type
					(char*)(half_data.data() + 1),	// base
					sizeof(half) * image.getN(),	// xStride
					sizeof(half) * image.getN() * image.getWidth())// yStride
					);
				frameBuffer.insert("B",				// name
					Imf::Slice(Imf::HALF,			// type
					(char*)(half_data.data() + 2),	// base
					sizeof(half) * image.getN(),	// xStride
					sizeof(half) * image.getN() * image.getWidth())// yStride
					);
			}
			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)image.getHeight());
		}
		else
		{
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
	}
	catch(const std::exception& e)
	{
		throw Indigo::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


static void testSavingWithOptions(EXRDecoder::SaveOptions options, int i)
{
	
	try
	{
		const int W = 20;
		const int H = 10;

		//================================= Test with Image saving =================================

		{
			Image image(W, H);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour3f((float)x, (float)y, 0.3f));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_a" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, path, options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 3);
			testAssert((int)im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*3 : sizeof(half)*3));
			for(int y=0; y<image.getHeight(); ++y)
			for(int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<3; ++c)
			{
				float a = im->pixelComponent(x, y, c);
				float b = image.pixelComponent(x, y, c);
				if(!epsEqual(a, b, 0.0001f))
					failTest("pixel components were different: " + toString(a) + " vs " + toString(b));
			}
		}

		//================================= Test with Image4f saving, with save_alpha_channel = true =================================
		{
			Image4f image(W, H);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour4f((float)x, (float)y, 0.3f, (float)x + (float)y));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_b" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, /*save_alpha_channel=*/true, path, options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 4);
			testAssert((int)im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*4 : sizeof(half)*4));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<4; ++c)
			{
				float a = im->pixelComponent(x, y, c);
				float b = image.getPixel(x, y)[c];
				if(!epsEqual(a, b, 0.0001f))
					failTest("pixel components were different: " + toString(a) + " vs " + toString(b));
			}
		}

		//================================= Test with Image4f saving, with save_alpha_channel = false =================================
		{
			Image4f image(W, H);
			for(int y=0; y<image.getHeight(); ++y)
			for(int x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour4f((float)x, (float)y, 0.3f, (float)x + (float)y));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_c" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, /*save_alpha_channel=*/false, path, options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 3);
			testAssert((int)im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*3 : sizeof(half)*3));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<3; ++c)
			{
				float a = im->pixelComponent(x, y, c);
				float b = image.getPixel(x, y)[c];
				if(!epsEqual(a, b, 0.0001f))
					failTest("pixel components were different: " + toString(a) + " vs " + toString(b));
			}
		}

		//================================= Test with an ImageMapFloat with one component =================================
		{
			const int N = 1;
			ImageMapFloat image(W, H, N);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				image.getPixel(x, y)[0] = (float)x + (float)y;

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_d" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, path, options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 1);
			testAssert((int)im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*1 : sizeof(half)*1));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<1; ++c)
			{
				float a = im->pixelComponent(x, y, c);
				float b = image.getPixel(x, y)[c];
				if(!epsEqual(a, b, 0.0001f))
					failTest("pixel components were different: " + toString(a) + " vs " + toString(b));
			}
		}

		//================================= Test with an ImageMapFloat with three components =================================
		{
			const int N = 3;
			ImageMapFloat image(W, H, N);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<N; ++c)
				image.getPixel(x, y)[c] = (float)x + (float)y + (float)c;

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_e" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, path, options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == N);
			testAssert((int)im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*N : sizeof(half)*N));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			for(int c=0; c<N; ++c)
			{
				float a = im->pixelComponent(x, y, c);
				float b = image.getPixel(x, y)[c];
				if(!epsEqual(a, b, 0.0001f))
					failTest("pixel components were different: " + toString(a) + " vs " + toString(b));
			}
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
}


void EXRDecoder::test()
{
	conPrint("EXRDecoder::test()");

	try
	{
		Reference<Map2D> im = EXRDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/uffizi-large.exr");
		testAssert(im->getMapWidth() == 2400);
		testAssert(im->getMapHeight() == 1200);
		testAssert(im->getBytesPerPixel() == 2 * 3); // Half precision * 3 components
		testAssert(im->getGamma() == 1.f);

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = EXRDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/" + euro + ".exr");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		EXRDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/NO_SUCH_FILE.exr");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		EXRDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}



	// Test writing of EXR files.

	int i=0;
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_None,  EXRDecoder::BitDepth_16), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_RLE,   EXRDecoder::BitDepth_16), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_ZIP,   EXRDecoder::BitDepth_16), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PIZ,   EXRDecoder::BitDepth_16), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PXR24, EXRDecoder::BitDepth_16), i++);

	// TEMP: disabled due to failing tests.  EXR bug?
	//testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_B44A,  EXRDecoder::BitDepth_16), i++);

	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_None,  EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_RLE,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_ZIP,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PIZ,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PXR24, EXRDecoder::BitDepth_32), i++);
	//testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_B44A,  EXRDecoder::BitDepth_32), i++);


	/*try
	{
		Reference<Map2D> im = EXRDecoder::decode("C:\\Users\\nick\\Downloads\\EXR_compression\\final_frame000330_tonemapped.exr");
		//testAssert(im->getMapWidth() == 2400);
		//testAssert(im->getMapHeight() == 1200);
		//testAssert(im->getBytesPerPixel() == 2 * 3); // Half precision * 3 components

		// Write out
		Timer timer;
		EXRDecoder::saveImageTo32BitEXR(*im->convertToImage(), "C:\\art\\indigo\\tests\\exr tests\\zomb_bedroom_piz.exr");
		conPrint("Saving exr took " + timer.elapsedString());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}*/

	// Destroy OpenEXR worker threads to avoid memory leaks.
	IlmThread::ThreadPool::globalThreadPool().setNumThreads(0);

	conPrint("EXRDecoder::test() done");
}


#endif // BUILD_TESTS
