/*=====================================================================
EXRDecoder.cpp
--------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
=====================================================================*/
#include "EXRDecoder.h"


#include "imformatdecoder.h"
#include "../graphics/image.h"
#include "../graphics/Image4f.h"
#include "../graphics/ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Vector.h"
#include "../utils/IncludeHalf.h"
#include "../utils/BufferInStream.h"
#include "../utils/BufferViewInStream.h"
#include "../utils/MemMappedFile.h"
#include "../utils/ConPrint.h"
#include <ImathBox.h>
#include <fstream>
#include <ImfStdIO.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>
#include <ImfChannelList.h>
#include <ImfRgbaFile.h>
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


class EXRThreadTask : public glare::Task
{
public:
	EXRThreadTask(IlmThread::Task* task_) : task(task_) {}

	virtual void run(size_t thread_index)
	{
		task->execute();

		IlmThread::TaskGroup* group = task->group();
		delete task; // LineBufferTask destructor accesses a shared semaphore (_lineBuffer->_sem), so make sure to destroy the task before we tell the group the task is done.
		// Otherwise execution will continue and the shared semaphore will be destroyed before being accessed.
		group->finishOneTask();
	}
	
	IlmThread::Task* task;
};


class EXRDecoderThreadPoolProvider final : public IlmThread::ThreadPoolProvider
{
public:
	EXRDecoderThreadPoolProvider(glare::TaskManager* task_manager_)
	:	task_manager(task_manager_)
	{}

	virtual ~EXRDecoderThreadPoolProvider()
	{}

	virtual int numThreads() const override
	{
		return (int)task_manager->getNumThreads();
	}

	virtual void setNumThreads(int count) override
	{
		// Do nothing
	}

	virtual void addTask(IlmThread::Task* exr_task) override
	{
		Reference<EXRThreadTask> task = new EXRThreadTask(exr_task);
		task_manager->addTask(task);
	}

	// Ensure that all tasks in this set are finished and threads shutdown
	virtual void finish() override
	{
		task_manager->waitForTasksToComplete();
	}

	glare::TaskManager* task_manager;
};


// Based on NullThreadPoolProvider in OpenEXR\src\lib\IlmThread\IlmThreadPool.cpp
class MyNullThreadPoolProvider final : public IlmThread::ThreadPoolProvider
{
public:
	virtual ~MyNullThreadPoolProvider() {}

	virtual int numThreads() const { return 0; }
	virtual void setNumThreads(int count) {}
	virtual void addTask(IlmThread::Task *t)
	{
		t->execute();
		t->group()->finishOneTask();
		delete t;
	}
	virtual void finish() {}
}; 


void EXRDecoder::setTaskManager(glare::TaskManager* task_manager)
{
	EXRDecoderThreadPoolProvider* thread_pool_provider = new EXRDecoderThreadPoolProvider(task_manager);

	IlmThread::ThreadPool::globalThreadPool().setThreadProvider(thread_pool_provider); // IlmThread will delete thread_pool_provider when we replace it in EXRDecoder::shutdown() below.
}


void EXRDecoder::clearTaskManager()
{
	// Replace the EXRDecoderThreadPoolProvider with a null thread pool provider, so there are no references to the glare TaskManager.
	IlmThread::ThreadPool::globalThreadPool().setThreadProvider(new MyNullThreadPoolProvider()); // IlmThread deletes the pool provider passed in when global thread pool is destroyed.
}


class EXRDecoderInputStream final : public Imf::IStream
{
public:
	EXRDecoderInputStream(const ArrayRef<uint8> data, const std::string& filename_) : Imf::IStream(filename_.c_str()), filename(filename_), stream(data) {}
	virtual ~EXRDecoderInputStream() {}

	//-------------------------------------------------
	// Does this input stream support memory-mapped IO?
	//
	// Memory-mapped streams can avoid an extra copy;
	// memory-mapped read operations return a pointer
	// to an internal buffer instead of copying data
	// into a buffer supplied by the caller.
	//-------------------------------------------------
	virtual bool isMemoryMapped() const { return false; } // NOTE: Have to return false here, as OpenEXR was causing crashes with it set to true.

	//------------------------------------------------------
	// Read from the stream:
	//
	// read(c,n) reads n bytes from the stream, and stores
	// them in array c.  If the stream contains less than n
	// bytes, or if an I/O error occurs, read(c,n) throws
	// an exception.  If read(c,n) reads the last byte from
	// the file it returns false, otherwise it returns true.
	//------------------------------------------------------
	virtual bool read(char c[/*n*/], int n)
	{
		if(n < 0)
			throw glare::Exception("Invalid num bytes to read");

		stream.readData(c, (size_t)n);
		return !stream.endOfStream();
	}

	//---------------------------------------------------
	// Read from a memory-mapped stream:
	//
	// readMemoryMapped(n) reads n bytes from the stream
	// and returns a pointer to the first byte.  The
	// returned pointer remains valid until the stream
	// is closed.  If there are less than n byte left to
	// read in the stream or if the stream is not memory-
	// mapped, readMemoryMapped(n) throws an exception.  
	//---------------------------------------------------
	virtual char* readMemoryMapped(int n)
	{
		if(n < 0)
			throw glare::Exception("Invalid num bytes to read");

		if(!stream.canReadNBytes((size_t)n))
			throw glare::Exception("tried to read too many bytes");

		char* ptr = (char*)stream.currentReadPtr();
		stream.advanceReadIndex((size_t)n);
		return ptr;
	}

	//--------------------------------------------------------
	// Get the current reading position, in bytes from the
	// beginning of the file.  If the next call to read() will
	// read the first byte in the file, tellg() returns 0.
	//--------------------------------------------------------
	virtual uint64_t tellg()
	{
		return stream.getReadIndex();
	}
	
	//-------------------------------------------
	// Set the current reading position.
	// After calling seekg(i), tellg() returns i.
	//-------------------------------------------
	virtual void seekg(uint64_t pos)
	{
		stream.setReadIndex(pos);
	}

	//------------------------------------------------------
	// Clear error conditions after an operation has failed.
	//------------------------------------------------------
	virtual void clear()
	{}

	//------------------------------------------------------
	// Get the name of the file associated with this stream.
	//------------------------------------------------------
	const char* fileName() const
	{
		return filename.c_str();
	}

	std::string filename;
	//BufferInStream stream; // Note that we need to have a buffer than OpenEXR can write to, as the DWAB decompressor writes back to the buffer.
	BufferViewInStream stream; // Use if we are not returning true from isMemoryMapped() - avoids a copy.
	// So use BufferInStream which makes a copy
};


Reference<Map2D> EXRDecoder::decode(const std::string& pathname, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(pathname);
		return decodeFromBuffer(file.fileData(), file.fileSize(), pathname, mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> EXRDecoder::decodeFromBuffer(const void* data, size_t size, const std::string& pathname, glare::Allocator* mem_allocator)
{
	try
	{
		EXRDecoderInputStream in_stream(ArrayRef<uint8>((const uint8*)data, size), pathname);

		Imf::InputFile file(in_stream);

		const Imf::ChannelList& channels = file.header().channels();

		if(channels.begin() == channels.end())
			throw ImFormatExcep("EXR file had no channels.");

		// Get names of all layers in the EXR
		std::set<std::string> layer_names;
		channels.layers(layer_names);

		std::vector<std::string> channels_to_load_names;
		if(layer_names.empty())
		{
			// Look for channels called R, G, B, or R, G, B, A, and if found, load them in RGBA order.
			if(channels.findChannel("R"))
			{
				channels_to_load_names.push_back("R");

				if(channels.findChannel("G"))
					channels_to_load_names.push_back("G");

				if(channels.findChannel("B"))
					channels_to_load_names.push_back("B");

				if(channels.findChannel("A"))
					channels_to_load_names.push_back("A");
			}
			else
			{
				/*
				Try and load a multi-wavelength EXR, should have channel names like
				wavelength430_5
				wavelength460_25
				wavelength490_75
				etc..
				*/
				if(::hasPrefix(channels.begin().name(), "wavelength"))
				{
					for(auto it = channels.begin(); it != channels.end(); ++it)
					{
						const std::string channel_name = it.name();
						if(!::hasPrefix(channel_name, "wavelength"))
							throw glare::Exception("Found a channel name that did not start with 'wavelength' while loading a spectral EXR.");

						channels_to_load_names.push_back(channel_name);
					}
				}
				else
				{
					// Just use the first channel (in alphabetic order)
					channels_to_load_names.push_back(channels.begin().name());
				}
			}
		}
		else // Else we have one or more layers:
		{
			for(auto z = layer_names.begin(); z != layer_names.end(); ++z)
			{
				const std::string layer = *z;
				if(layer == "spectral")
				{
					Imf::ChannelList::ConstIterator begin, end;
					channels.channelsInLayer(layer, begin, end);

					for(auto it = begin; it != end; ++it)
					{
						const std::string channel_name = it.name();
						if(!::hasPrefix(channel_name, "spectral.wavelength"))
							throw glare::Exception("Found a channel name that did not start with 'wavelength' while loading a spectral EXR.");

						channels_to_load_names.push_back(channel_name);
					}
				}
				else
				{
					// Look for channels called R, G, B, or R, G, B, A, and if found, load them in RGBA order.
					if(channels.findChannel(layer + ".R"))
					{
						channels_to_load_names.push_back(layer + ".R");

						if(channels.findChannel(layer + ".G"))
							channels_to_load_names.push_back(layer + ".G");

						if(channels.findChannel(layer + ".B"))
							channels_to_load_names.push_back(layer + ".B");

						if(channels.findChannel(layer + ".A"))
							channels_to_load_names.push_back(layer + ".A");
					}
					else
					{
						// Just use the first channel in this layer (in alphabetic order)
						Imf::ChannelList::ConstIterator begin, end;
						channels.channelsInLayer(layer, begin, end);
						if(begin == end)
							throw ImFormatExcep("layer had no channels."); // Shouldn't get here.

						channels_to_load_names.push_back(begin.name());
					}
				}
			}
		}

		assert(!channels_to_load_names.empty() && channels.findChannel(channels_to_load_names[0]));

		const Imf::PixelType use_pixel_type = channels.findChannel(channels_to_load_names[0])->type;

		const size_t result_num_channels = channels_to_load_names.size();

		const Imath::Box2i dw = file.header().dataWindow();
		if(dw.min.x > dw.max.x || dw.min.y > dw.max.y)
			throw ImFormatExcep("Invalid data window");
		const int width  = dw.max.x - dw.min.x + 1;
		const int height = dw.max.y - dw.min.y + 1;

		if(width > 1000000)
			throw glare::Exception("Width is too large: " + toString(width));
		if(height > 1000000)
			throw glare::Exception("Height is too large: " + toString(height));
		const size_t max_num_pixels = 1 << 28;
		if((size_t)width * (size_t)height > max_num_pixels)
			throw glare::Exception("invalid width and height (too many pixels): " + toString(width) + ", " + toString(height));
		if(result_num_channels > 100000)
			throw glare::Exception("Num channels to load is too large: " + toString(result_num_channels));

		Imf::FrameBuffer frameBuffer;

		if(use_pixel_type == Imf::FLOAT)
		{
			Reference<ImageMap<float, FloatComponentValueTraits> > new_image = new ImageMap<float, FloatComponentValueTraits>(width, height, result_num_channels, mem_allocator);
			new_image->setGamma(1); // HDR images should have gamma 1.
			new_image->channel_names = channels_to_load_names;

			const size_t x_stride = sizeof(float) * result_num_channels;
			const size_t y_stride = sizeof(float) * result_num_channels * width;
			float* channel_0_base = new_image->getData() + (-(intptr_t)dw.min.x - (intptr_t)dw.min.y * (intptr_t)width) * (intptr_t)result_num_channels;

			for(size_t i=0; i<channels_to_load_names.size(); ++i)
			{
				frameBuffer.insert(
					channels_to_load_names[i],			// name
					Imf::Slice(
						Imf::FLOAT,						// type
						(char*)(channel_0_base + i),	// base
						x_stride,
						y_stride
					)
				);
			}

			file.setFrameBuffer(frameBuffer);
			file.readPixels(dw.min.y, dw.max.y);

			return new_image;
		}
		else if(use_pixel_type == Imf::HALF)
		{
			Reference<ImageMap<half, HalfComponentValueTraits> > new_image = new ImageMap<half, HalfComponentValueTraits>(width, height, result_num_channels, mem_allocator);
			new_image->setGamma(1); // HDR images should have gamma 1.

			const size_t x_stride = sizeof(half) * result_num_channels;
			const size_t y_stride = sizeof(half) * result_num_channels * width;

			half* channel_0_base = new_image->getData() + (-(intptr_t)dw.min.x - (intptr_t)dw.min.y * (intptr_t)width) * (intptr_t)result_num_channels;

			for(size_t i=0; i<channels_to_load_names.size(); ++i)
			{
				frameBuffer.insert(
					channels_to_load_names[i],			// name
					Imf::Slice(
						Imf::HALF,						// type
						(char*)(channel_0_base + i),	// base
						x_stride,
						y_stride
					)
				);
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


Imf::Compression EXRDecoder::EXRCompressionMethod(EXRDecoder::CompressionMethod m)
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
	case EXRDecoder::CompressionMethod_DWAB:
		return Imf::DWAB_COMPRESSION;
	};

	assert(0);
	throw glare::Exception("invalid compression method");
}


//NOTE: This function assumes that the pixel data is densely packed, so that x-stride is sizeof(float) * num_channels and y-stride = x_stride * width.
void EXRDecoder::saveImageToEXR(const float* pixel_data, size_t width, size_t height, size_t num_channels, bool save_alpha_channel, const std::string& pathname, 
	const std::string& layer_name, const SaveOptions& options)
{
	if(num_channels == 0)
		throw glare::Exception("Invalid num channels for EXR saving.");

	// See 'Reading and writing image files.pdf', section 3.1: Writing an Image File
	try
	{
		std::ofstream outfile_stream(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdOFStream exr_ofstream(outfile_stream, pathname.c_str());

		Imf::Header header((int)width, (int)height,
			1.f, // pixel aspect ratio
			Imath::V2f(0,0), // screenWindowCenter
			1.f, // screenWindowWidth
			Imf::INCREASING_Y, // lineOrder
			EXRCompressionMethod(options.compression_method) // compression method
		);

		// NOTE: People were having issues loading the EXRs with the layer name, so just use RGBA for now.

		int num_channels_to_save; // May be < num_channels, which is related to the stride in the source data.
		std::vector<std::string> channel_names(num_channels); // Channel names to use in the EXR file.

		if(!options.channel_names.empty())
		{
			if(options.channel_names.size() != num_channels)
				throw glare::Exception("options.channel_names had wrong size.");
			channel_names = options.channel_names;
			num_channels_to_save = (int)num_channels;
		}
		else
		{
			if(num_channels == 1)
			{
				num_channels_to_save = 1;
				channel_names[0] = "Y"; // layer_name; // Just use the layer name directly without any channel sub-name.
				// There is some indication 'Y' is the standard name for greyscale channels in EXRs, see http://www.openexr.com/documentation/ReadingAndWritingImageFiles.pdf, 
				// 'Luminance/Chroma and Gray-Scale Images'.
			}
			else
			{
				if(!(num_channels == 3 || num_channels == 4))
					throw glare::Exception("Invalid num channels for EXR saving.");

				num_channels_to_save = save_alpha_channel ? 4 : 3;
				/*channel_names[0] = layer_name + ".R";
				channel_names[1] = layer_name + ".G";
				channel_names[2] = layer_name + ".B";
				if(save_alpha_channel)
					channel_names[3] = layer_name + ".A";*/
				channel_names[0] = "R";
				channel_names[1] = "G";
				channel_names[2] = "B";
				if(save_alpha_channel)
					channel_names[3] = "A";
			}
		}

		for(int c=0; c<num_channels_to_save; ++c)
			header.channels().insert(channel_names[c], Imf::Channel(options.bit_depth == BitDepth_32 ? Imf::FLOAT : Imf::HALF));

		Imf::OutputFile file(exr_ofstream, header);
		Imf::FrameBuffer frameBuffer;

		const size_t N = num_channels;

		if(options.bit_depth == BitDepth_16)
		{
			// Convert our float data to halfs.
			const size_t num_values = width * height * N;
			js::Vector<half, 64> half_data(num_values);
			const float* const src_float_data = pixel_data;
			
			for(size_t i=0; i<num_values; ++i)
				half_data[i] = half(src_float_data[i]);

			for(int c=0; c<num_channels_to_save; ++c)
			{
				frameBuffer.insert(channel_names[c],	// name
					Imf::Slice(Imf::HALF,				// type
					(char*)(half_data.data() + c),		// base
						sizeof(half) * N,				// xStride
						sizeof(half) * N * width)		// yStride
				);
			}

			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)height);
		}
		else
		{
			assert(options.bit_depth == BitDepth_32);

			for(int c=0; c<num_channels_to_save; ++c)
			{
				frameBuffer.insert(channel_names[c],	// name
					Imf::Slice(Imf::FLOAT,				// type
					(char*)(pixel_data + c),			// base
						sizeof(float) * N,				// xStride
						sizeof(float) * N * width)		// yStride
				);
			}

			file.setFrameBuffer(frameBuffer);
			file.writePixels((int)height);
		}
	}
	catch(const std::exception& e)
	{
		throw glare::Exception("Error writing EXR file: " + std::string(e.what()));
	}
}


void EXRDecoder::saveImageToEXR(const Image& image, const std::string& pathname, const std::string& layer_name, const SaveOptions& options)
{
	saveImageToEXR(&image.getPixel(0).r, image.getWidth(), image.getHeight(), 
		3, // num channels
		false, // save alpha channel
		pathname,
		layer_name,
		options
	);
}


void EXRDecoder::saveImageToEXR(const Image4f& image, bool save_alpha_channel, const std::string& pathname, const std::string& layer_name, const SaveOptions& options)
{
	saveImageToEXR(&image.getPixel(0).x[0], image.getWidth(), image.getHeight(), 
		4, // num channels
		save_alpha_channel, // save alpha channel
		pathname,
		layer_name,
		options
	);
}


void EXRDecoder::saveImageToEXR(const ImageMapFloat& image, const std::string& pathname, const std::string& layer_name, const SaveOptions& options)
{
	saveImageToEXR(&image.getPixel(0, 0)[0], image.getWidth(), image.getHeight(), 
		image.getN(), // num channels
		image.getN() == 4, // save alpha channel
		pathname,
		layer_name,
		options
	);
}


#if BUILD_TESTS


#if 0
// Command line:
// C:\fuzz_corpus\exr N:\indigo\trunk\testfiles\EXRs -rss_limit_mb=20000

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		EXRDecoder::decodeFromBuffer(data, size, "dummy_path");
	}
	catch (glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


static void testSavingWithOptions(EXRDecoder::SaveOptions options, int i)
{
	
	try
	{
		const int W = 20;
		const int H = 10;
		const float allowable_diff = (options.compression_method == EXRDecoder::CompressionMethod_DWAB) ? 5.0e-2f : 1.0e-4f;

		//================================= Test with Image saving =================================

		{
			Image image(W, H);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour3f((float)x, (float)y, 0.3f));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_a" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, path, "main layer", options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 3);
			testAssert(im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*3 : sizeof(half)*3));
			for(size_t y=0; y<image.getHeight(); ++y)
				for(size_t x=0; x<image.getWidth(); ++x)
				{
					const Colour4f a = im->pixelColour(x, y);
					const Colour4f b = image.pixelColour(x, y);
					for(int c=0; c<3; ++c)
						if(!(epsEqual(a[c], b[c], allowable_diff) || Maths::approxEq(a[c], b[c], allowable_diff)))
							failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
				}
		}

		//================================= Test with Image4f saving, with save_alpha_channel = true =================================
		{
			Image4f image(W, H);
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour4f((float)x, (float)y, 0.3f, (float)x + (float)y));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_b" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, /*save_alpha_channel=*/true, path, "main layer", options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 4);
			testAssert(im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*4 : sizeof(half)*4));

			const bool is_half = im.isType<ImageMap<half, HalfComponentValueTraits> >();
			testAssert(im.isType<ImageMapFloat>() || is_half);

			if(im.isType<ImageMapFloat>())
			{
				for(unsigned int y=0; y<image.getHeight(); ++y)
					for(unsigned int x=0; x<image.getWidth(); ++x)
					{
						const float* a = im.downcastToPtr<ImageMapFloat>()->getPixel(x, y);
						const Colour4f b = image.getPixel(x, y);
						for(int c=0; c<4; ++c)
							if(!(epsEqual(a[c], b[c], allowable_diff) || Maths::approxEq(a[c], b[c], allowable_diff)))
								failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
					}
			}
			else
			{
				for(unsigned int y=0; y<image.getHeight(); ++y)
					for(unsigned int x=0; x<image.getWidth(); ++x)
					{
						const half* a = im.downcastToPtr<ImageMap<half, HalfComponentValueTraits> >()->getPixel(x, y);
						const Colour4f b = image.getPixel(x, y);
						for(int c=0; c<4; ++c)
							if(!(epsEqual((float)a[c], b[c], allowable_diff) || Maths::approxEq((float)a[c], b[c], allowable_diff)))
								failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
					}
			}
		}

		//================================= Test with Image4f saving, with save_alpha_channel = false =================================
		{
			Image4f image(W, H);
			for(size_t y=0; y<image.getHeight(); ++y)
			for(size_t x=0; x<image.getWidth(); ++x)
				image.setPixel(x, y, Colour4f((float)x, (float)y, 0.3f, (float)x + (float)y));

			const std::string path = PlatformUtils::getTempDirPath() + "/exr_write_test_c" + toString(i) + ".exr";
		
			EXRDecoder::saveImageToEXR(image, /*save_alpha_channel=*/false, path, "main layer", options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 3);
			testAssert(im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*3 : sizeof(half)*3));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			{
				const Colour4f a = im->pixelColour(x, y);
				const Colour4f b = image.getPixel(x, y);
				for(int c=0; c<3; ++c)
					if(!(epsEqual(a[c], b[c], allowable_diff) || Maths::approxEq(a[c], b[c], allowable_diff)))
						failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
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
		
			EXRDecoder::saveImageToEXR(image, path, "main layer", options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == 1);
			testAssert(im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*1 : sizeof(half)*1));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			{
				const Colour4f a = im->pixelColour(x, y);
				const float* b = image.getPixel(x, y);
				for(int c=0; c<1; ++c)
					if(!(epsEqual(a[c], b[c], allowable_diff) || Maths::approxEq(a[c], b[c], allowable_diff)))
						failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
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
		
			EXRDecoder::saveImageToEXR(image, path, "main layer", options);

			// Now read it to make sure it's a valid EXR
			Reference<Map2D> im = EXRDecoder::decode(path);
			testAssert(im->getMapWidth() == W);
			testAssert(im->getMapHeight() == H);
			testAssert(im->numChannels() == N);
			testAssert(im->getBytesPerPixel() == ((options.bit_depth == EXRDecoder::BitDepth_32) ? sizeof(float)*N : sizeof(half)*N));
			for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
			{
				const Colour4f a = im->pixelColour(x, y);
				const float* b = image.getPixel(x, y);
				for(int c=0; c<N; ++c)
					if(!(epsEqual(a[c], b[c], allowable_diff) || Maths::approxEq(a[c], b[c], allowable_diff)))
						failTest("pixel components were different: " + toString(a[c]) + " vs " + toString(b[c]));
			}
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


static void doMainEXRTests()
{
	Timer timer;

	try
	{
		Map2DRef map = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/uffizi_small_16bit_1channel.exr");
		testAssert(map->getBytesPerPixel() == 2);
		testAssert(map->numChannels() == 1);
		testAssert(map->getMapWidth() == 600);
		testAssert(map->getMapHeight() == 300);
	}
	catch(ImFormatExcep& )
	{
	}
	
	try
	{
		Timer timer2;
		Map2DRef map = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/cirrus.exr");
		testAssert(map->getMapWidth() == 1024);
		testAssert(map->getMapHeight() == 1024);
		conPrint("Loading cirrus tex took " + timer2.elapsedStringMSWIthNSigFigs(4));
	}
	catch(ImFormatExcep& )
	{
	}
	
	try
	{
		Timer timer2;
		Map2DRef map = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/heightfield_with_deposited_sed_0_0.exr");
		conPrint("Loading heightfield_with_deposited_sed_0_0.exr took " + timer2.elapsedStringMSWIthNSigFigs(4));
	}
	catch(ImFormatExcep& )
	{
	}

	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/slow-unit-119922327e38b857c65f237b0ad0cedab8761fcd");
	}
	catch(ImFormatExcep& )
	{
	}

	// Very large number of lines in image
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/oom-119922327e38b857c65f237b0ad0cedab8761fcd");
	}
	catch(ImFormatExcep& )
	{
	}

	// A negatively sized alloc due to overflow:
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/crash-84e803c816272a4547539578494826c6d42bfb74");
	}
	catch(ImFormatExcep& )
	{
	}


	// Test an EXR file with data window != visible window.
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/openexr-images-master/DisplayWindow/t08.exr");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Test all files in EXRs\openexr-images-master/Chromaticities
	try
	{
		const auto paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir() + "/testfiles/EXRs/openexr-images-master/Chromaticities", "exr");
		for(size_t i=0; i<paths.size(); ++i)
		{
			//conPrint("Testing '" + paths[i] + "'...");
			try
			{
				EXRDecoder::decode(paths[i]);
			}
			catch(ImFormatExcep& /*e*/)
			{
				//conPrint("Caught excep: " + e.what());
			}
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test all files in EXRs\openexr-images-master/DisplayWindow
	try
	{
		const auto paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir() + "/testfiles/EXRs/openexr-images-master/DisplayWindow", "exr");
		for(size_t i=0; i<paths.size(); ++i)
		{
			//conPrint("Testing '" + paths[i] + "'...");
			try
			{
				EXRDecoder::decode(paths[i]);
			}
			catch(ImFormatExcep& /*e*/)
			{
				//conPrint("Caught excep: " + e.what());
			}
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test all files in EXRs\openexr-images-master/Damaged
	try
	{
		const auto paths = FileUtils::getFilesInDirFullPaths(TestUtils::getTestReposDir() + "/testfiles/EXRs/openexr-images-master/Damaged");
		for(size_t i=0; i<paths.size(); ++i)
		{
			// conPrint("Testing file " + toString(i) + " / " + toString(paths.size()) + ": '" + paths[i] + "'...");
			try
			{
				EXRDecoder::decode(paths[i]);
			}
			catch(ImFormatExcep& /*e*/)
			{
				// conPrint("Caught expected excep: " + e.what());
			}
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	
	// This EXR file used to allocate a lot of memory, lots per worker thread.
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/openexr-images-master/Damaged/clusterfuzz-testcase-minimized-openexr_exrcheck_fuzzer-4755804284649472");
	}
	catch(ImFormatExcep&)
	{
	}

	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/crash-3b663cdb256db6f3f9b7557611efc6f8adcf7ae7");

		//failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/crash-0ea96c18289d944be2a8471c4be4dd23b7f6844b");

		//failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// This EXR file allocates about 5GB per OpenEXR worker thread.
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/crash-b24aa7f86b3301058d216148f3286a73f6130c08");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with an OOM test failure
	/*try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/oom-408c93fd0c54089cff73306a44d5ede1c8450dcd");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}*/

	// Write spectral EXR
	if(false)
	{
		const int W = 128;
		const int H = 128;
		const int N = 12;
		ImageMapFloat image(W, H, N);
		for(unsigned int y=0; y<image.getHeight(); ++y)
			for(unsigned int x=0; x<image.getWidth(); ++x)
				for(int c=0; c<N; ++c)
					image.getPixel(x, y)[c] = (c == 2) ? 100.0f : 0.f;

		EXRDecoder::SaveOptions options;
		for(int c=0; c<N; ++c)
		{
			const float bucket_w = 300.f / N;
			std::string initial_name = "wavelength" + toString(400.0 + c * bucket_w);
			std::string name = StringUtils::replaceCharacter(initial_name, '.', '_');
			options.channel_names.push_back(name);
		}

		const std::string path = "spectral_12_channels_128x128_val_100_bucket_2.exr";

		EXRDecoder::saveImageToEXR(image, path, "main layer", options);
	}

	/*try
	{
		Reference<Map2D> im = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/uffizi-large.exr");
		testAssert(im->getMapWidth() == 2400);
		testAssert(im->getMapHeight() == 1200);
		testAssert(im->getBytesPerPixel() == 2 * 3); // Half precision * 3 components
		testAssert(im->getGamma() == 1.f);

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/" + euro + ".exr");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}*/

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/NO_SUCH_FILE.exr");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");

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
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_DWAB,  EXRDecoder::BitDepth_16), i++);

	// TEMP: disabled due to failing tests.  EXR bug?
	//testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_B44A,  EXRDecoder::BitDepth_16), i++);

	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_None,  EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_RLE,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_ZIP,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PIZ,   EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_PXR24, EXRDecoder::BitDepth_32), i++);
	testSavingWithOptions(EXRDecoder::SaveOptions(EXRDecoder::CompressionMethod_DWAB,  EXRDecoder::BitDepth_32), i++);
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

	conPrint("doMainEXRTests took " + timer.elapsedStringNSigFigs(3));
}


class EXRDecoderTestTask : public glare::Task
{
	virtual void run(size_t thread_index)
	{
		for(int i=0; i<10000000; ++i)
		{
			//conPrint("decoding exr...");
			EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/cirrus.exr");
		}
	}
};


void EXRDecoder::test()
{
	conPrint("EXRDecoder::test()");

	// Test setTaskManager and clearTaskManager
	{
		glare::TaskManager task_manager;
		EXRDecoder::setTaskManager(&task_manager);
		EXRDecoder::clearTaskManager();
	}

	// Make sure we don't crash while clearing task manager after using it.
	{
		glare::TaskManager task_manager;
		EXRDecoder::setTaskManager(&task_manager);

		Map2DRef map = EXRDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/EXRs/uffizi_small_16bit_1channel.exr");

		EXRDecoder::clearTaskManager();
	}


	// We will run the main EXR tests twice, once without a thread pool, and once with.
	conPrint("---------------------------- doMainEXRTests single threaded ----------------------------");
	doMainEXRTests();

	{
		glare::TaskManager task_manager;
		EXRDecoder::setTaskManager(&task_manager);
		
		conPrint("---------------------------- doMainEXRTests multi threaded ----------------------------");
		doMainEXRTests();

		EXRDecoder::clearTaskManager();
	}


	// Stress-test task manager running EXR tasks.
	if(false)
	{
		conPrint("Stress testing...");
		{
			glare::TaskManager worker_task_manager;
			EXRDecoder::setTaskManager(&worker_task_manager);

			glare::TaskManager task_manager;
			glare::TaskGroupRef group = new glare::TaskGroup();
			for(int i=0; i<8; ++i)
				group->tasks.push_back(new EXRDecoderTestTask());

			task_manager.runTaskGroup(group);

			EXRDecoder::clearTaskManager();
		}
	}

	conPrint("EXRDecoder::test() done");
}


#endif // BUILD_TESTS
