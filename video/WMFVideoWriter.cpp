/*=====================================================================
WMFVideoWriter.cpp
------------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:59:19 +1300
=====================================================================*/
#include "WMFVideoWriter.h"


#ifdef _WIN32


#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>



WMFVideoWriter::WMFVideoWriter(const std::string& URL, const VidParams& vid_params_)
:	com_inited(false),
	frame_index(0)
{
	vid_params = vid_params_;

	// Initialize the COM runtime.
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if(hr == RPC_E_CHANGED_MODE)
	{ 
		// COM has already been initialised in some other mode.
		// This is alright, but we don't want to call CoUninitialize in this case.
	}
	else
	{
		if(!SUCCEEDED(hr))
			throw Indigo::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

		com_inited = true;
	}

	// Initialize the Media Foundation platform.
	hr = MFStartup(MF_VERSION);
	if(!SUCCEEDED(hr))
		throw Indigo::Exception("MFStartup failed: " + PlatformUtils::COMErrorString(hr));

	// Create the writer.
	hr = MFCreateSinkWriterFromURL(StringUtils::UTF8ToPlatformUnicodeEncoding(URL).c_str(), NULL, NULL, &this->writer.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("MFCreateSinkWriterFromURL failed: " + PlatformUtils::COMErrorString(hr));

	// Set the output media type.
	ComObHandle<IMFMediaType> media_type_out;
	hr = MFCreateMediaType(&media_type_out.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("MFCreateMediaType failed: " + PlatformUtils::COMErrorString(hr));

	hr = media_type_out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = media_type_out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = media_type_out->SetUINT32(MF_MT_AVG_BITRATE, vid_params.bitrate); // Approximate data rate of the video stream, in bits per second.
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = media_type_out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeSize(media_type_out.ptr, MF_MT_FRAME_SIZE, vid_params.width, vid_params.height);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeRatio(media_type_out.ptr, MF_MT_FRAME_RATE, (uint32)(vid_params.fps * 1000000), 1000000);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeRatio(media_type_out.ptr, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = this->writer->AddStream(media_type_out.ptr, &this->stream_index);
	if(FAILED(hr))
		throw Indigo::Exception("Setting output media type failed: " + PlatformUtils::COMErrorString(hr));
	

	// Set the input media type.
	ComObHandle<IMFMediaType> media_type_in;
	hr = MFCreateMediaType(&media_type_in.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = media_type_in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = media_type_in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));

	hr = media_type_in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeSize(media_type_in.ptr, MF_MT_FRAME_SIZE, vid_params.width, vid_params.height);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeRatio(media_type_in.ptr, MF_MT_FRAME_RATE, (uint32)(vid_params.fps * 1000000), 1000000);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = MFSetAttributeRatio(media_type_in.ptr, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if(FAILED(hr))
		throw Indigo::Exception("Setting media type failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = this->writer->SetInputMediaType(stream_index, media_type_in.ptr, NULL);
	if(FAILED(hr))
		throw Indigo::Exception("Setting input media type failed: " + PlatformUtils::COMErrorString(hr));
	
	
	// Tell the sink writer to start accepting data.
	hr = this->writer->BeginWriting();
	if(FAILED(hr))
		throw Indigo::Exception("BeginWriting failed: " + PlatformUtils::COMErrorString(hr));
}


WMFVideoWriter::~WMFVideoWriter()
{
	this->writer.release();

	// Shut down Media Foundation.
	MFShutdown();

	if(com_inited)
		CoUninitialize();
}


void WMFVideoWriter::writeFrame(const uint8* source_data, size_t source_stride)
{
	const LONG width_B = 3 * vid_params.width;
	const DWORD buffer_size_B = width_B * vid_params.height;

	// Create a new memory buffer.
	ComObHandle<IMFMediaBuffer> buffer;
	HRESULT hr = MFCreateMemoryBuffer(buffer_size_B, &buffer.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("MFCreateMemoryBuffer failed: " + PlatformUtils::COMErrorString(hr));

	// Lock the buffer and copy the video frame to the buffer.
	BYTE* buffer_data = NULL;
	hr = buffer->Lock(&buffer_data, NULL, NULL);
	if(FAILED(hr))
		throw Indigo::Exception("Lock failed: " + PlatformUtils::COMErrorString(hr));

	// Copy and switch byte order to what WMF expects.
	for(uint32 y=0; y<vid_params.height; ++y)
	{
		for(uint32 x=0; x<vid_params.width; ++x)
		{
			buffer_data[vid_params.width * y * 3 + x * 3 + 0] = source_data[source_stride * y + (size_t)x * 3 + 2];
			buffer_data[vid_params.width * y * 3 + x * 3 + 1] = source_data[source_stride * y + (size_t)x * 3 + 1];
			buffer_data[vid_params.width * y * 3 + x * 3 + 2] = source_data[source_stride * y + (size_t)x * 3 + 0];
		}
	}

	if(buffer.ptr)
		buffer->Unlock();

	// Set the data length of the buffer.
	hr = buffer->SetCurrentLength(buffer_size_B);
	if(FAILED(hr))
		throw Indigo::Exception("SetCurrentLength failed: " + PlatformUtils::COMErrorString(hr));

	// Create a media sample and add the buffer to the sample.
	ComObHandle<IMFSample> sample;
	hr = MFCreateSample(&sample.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("MFCreateSample failed: " + PlatformUtils::COMErrorString(hr));
	
	hr = sample->AddBuffer(buffer.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("AddBuffer failed: " + PlatformUtils::COMErrorString(hr));
	

	// Set the time stamp and the duration.
	const UINT64 timestamp = (UINT64)(1.0e7 * frame_index / vid_params.fps);
	hr = sample->SetSampleTime(timestamp);
	if(FAILED(hr))
		throw Indigo::Exception("SetSampleTime failed: " + PlatformUtils::COMErrorString(hr));
	
	const UINT64 video_frame_duration = (UINT64)(1.0e7 / vid_params.fps);
	hr = sample->SetSampleDuration(video_frame_duration);
	if(FAILED(hr))
		throw Indigo::Exception("SetSampleDuration failed: " + PlatformUtils::COMErrorString(hr));
	

	// Send the sample to the Sink Writer.
	hr = this->writer->WriteSample(this->stream_index, sample.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("WriteSample failed: " + PlatformUtils::COMErrorString(hr));

	frame_index++;
}


void WMFVideoWriter::finalise()
{
	HRESULT hr = this->writer->Finalize();
	if(FAILED(hr))
		throw Indigo::Exception("Finalize failed: " + PlatformUtils::COMErrorString(hr));
}


#if 1 // BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"


void WMFVideoWriter::test()
{
	// Test making a video from a sequence of images read from disk.
	try
	{
		Reference<WMFVideoWriter> writer;

		const std::vector<std::string> files = FileUtils::getFilesInDirWithExtensionFullPaths("D:\\art\\chaotica\\anim_test2", "png");

		Timer timer;

		for(size_t i=0; i<files.size(); ++i)
		{
			conPrint("Encoding " + files[i] + "...");

			Map2DRef map = PNGDecoder::decode(files[i]);

			if(writer.isNull())
			{
				VidParams params;
				params.bitrate = 1000000;// 100000000;
				params.fps = 60;
				params.width  = (uint32)map->getMapWidth();
				params.height = (uint32)map->getMapHeight();
				writer = new WMFVideoWriter("D:\\indigo_temp\\movie_output\\frame_test_bitrate_1000000.mpg", params);
			}

			if(!map.isType<ImageMapUInt8>())
				failTest("Not ImageMapUInt8");

			ImageMapUInt8Ref imagemap = map.downcast<ImageMapUInt8>();

			writer->writeFrame(imagemap->getData(), imagemap->getWidth() * imagemap->getN());
		}

		writer->finalise();

		const double fps = files.size() / timer.elapsed();
		conPrint("FPS processed: " + toString(fps));
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	return;

	try
	{
		{
			//WMFVideoReader reader("D:\\video\\test5.avi");
			VidParams params;
			params.bitrate = 100000000;
			params.fps = 60;
			params.width = 800;
			params.height = 600;
			WMFVideoWriter writer("D:\\indigo_temp\\movie_output\\test.mpg", params);

			Timer timer;
			size_t frame_index = 0;
			for(int i=0; i<180; ++i)
			{
				ImageMapUInt8 map(params.width, params.height, 3);
				for(uint32 y=0; y<params.height; ++y)
				{
					for(uint32 x=0; x<params.width; ++x)
					{
						map.getPixel(x, y)[0] = (uint8)((float)x * 255.0f / params.width);
						map.getPixel(x, y)[1] = (uint8)0;
						map.getPixel(x, y)[2] = (uint8)0;
					}
				}

				const int ob_x = (int)(((float)i / 180) * params.width);
				for(uint32 y=100; y<110; ++y)
					for(int x=ob_x; x<ob_x + 10; ++x)
					{
						if(x < (int)params.width)
							map.getPixel(x, y)[1] = 255;
					}

				writer.writeFrame(map.getData(), map.getWidth() * 3);

				//BYTE* frame_buffer;
				//size_t stride_B;
				//reader.getAndLockNextFrame(frame_buffer, stride_B);

				//if(frame_buffer)
				//{
				//	const FormatInfo& format = reader.getCurrentFormat();

				//	/*ImageMapUInt8 map(format.im_width, format.im_height, 3);
				//	for(uint32 y=0; y<format.im_height; ++y)
				//	{
				//		BYTE* scanline = frame_buffer + (size_t)y * stride_B;
				//	
				//		for(uint32 x=0; x<format.im_width; ++x)
				//		{
				//			map.getPixel(x, y)[0] = scanline[x*4 + 2];
				//			map.getPixel(x, y)[1] = scanline[x*4 + 1];
				//			map.getPixel(x, y)[2] = scanline[x*4 + 0];
				//		}
				//	}
				//	PNGDecoder::write(map, "D:\\indigo_temp\\movie_output\\frame_" + toString(frame_index) + ".png");
				//	conPrint("Saved frame " + toString(frame_index));*/
				//}
				//else
				//{
				//	conPrint("Reached EOF.");
				//	break;
				//}

				//reader.unlockFrame();
				frame_index++;
			}

			writer.finalise();

			const double fps = frame_index / timer.elapsed();
			conPrint("FPS processed: " + toString(fps));
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif


#endif // _WIN32
