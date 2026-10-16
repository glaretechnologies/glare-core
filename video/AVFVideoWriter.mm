/*=====================================================================
AVFVideoWriter.mm
-----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "AVFVideoWriter.h"


#ifdef OSX


#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include <cmath>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>


// This file uses manual reference counting (it is compiled without ARC).


static const std::string errorDesc(NSError* err)
{
	if(!err)
		return "";
	NSString* desc = [err localizedDescription];
	return desc ? std::string([desc UTF8String]) : "";
}


static const std::string exceptionDesc(NSException* exception)
{
	NSString* desc = [exception description];
	return desc ? std::string([desc UTF8String]) : "";
}


AVFVideoWriter::AVFVideoWriter(const std::string& path, const VidParams& vid_params_)
:	frame_index(0),
	vid_params(vid_params_),
	m_video_writer(nullptr),
	m_writer_input(nullptr),
	m_adaptor(nullptr)
{
	// AVAssetWriter fails if the target file already exists, so remove it first if present.
	try
	{
		if(FileUtils::fileExists(path))
			FileUtils::deleteFile(path);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}

	try
	{
		@autoreleasepool
		{
			@try
			{
				// Use an @available check to avoid warnings like: 'AVVideoCodecTypeHEVC' is only available on macOS 10.13 or newer [-Wunguarded-availability-new]
				if(@available(macOS 10.13, *))
				{
					NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];

					NSError* err = nil;
					AVAssetWriter* video_writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
					if(video_writer == nil)
						throw glare::Exception("Failed to create video writer: " + errorDesc(err));
					m_video_writer = video_writer;

					const AVVideoCodecType codec = (vid_params.standard == VidParams::CompressionStandard_HEVC) ? AVVideoCodecTypeHEVC : AVVideoCodecTypeH264;

					NSDictionary* compression_props = @{ AVVideoAverageBitRateKey : @(vid_params.bitrate) };

					NSDictionary* video_settings = @{
						AVVideoCodecKey : codec,
						AVVideoWidthKey : @(vid_params.width),
						AVVideoHeightKey : @(vid_params.height),
						AVVideoCompressionPropertiesKey : compression_props
					};

					AVAssetWriterInput* writer_input = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:video_settings];
					if(writer_input == nil)
						throw glare::Exception("Failed to create asset writer input.");
					m_writer_input = writer_input;

					// writeFrame() waits until the input is ready for more data, instead of the input dropping frames to keep up.
					writer_input.expectsMediaDataInRealTime = NO;

					// Frames are passed to the encoder in 32-bit BGRA pixel buffers from the adaptor's pool.
					NSDictionary* pixel_buffer_attributes = @{
						(NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
						(NSString*)kCVPixelBufferWidthKey : @(vid_params.width),
						(NSString*)kCVPixelBufferHeightKey : @(vid_params.height)
					};

					AVAssetWriterInputPixelBufferAdaptor* adaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc] initWithAssetWriterInput:writer_input sourcePixelBufferAttributes:pixel_buffer_attributes];
					if(adaptor == nil)
						throw glare::Exception("Failed to create input pixel buffer adaptor.");
					m_adaptor = adaptor;

					if(![video_writer canAddInput:writer_input])
						throw glare::Exception("Video writer can't add input.");
					[video_writer addInput:writer_input];

					if(![video_writer startWriting])
						throw glare::Exception("Failed to start writing video: " + errorDesc([video_writer error]));
					[video_writer startSessionAtSourceTime:kCMTimeZero];
				}
				else
				{
					throw glare::Exception("Video writing is not supported before macOS 10.13.");
				}
			}
			@catch(NSException* exception)
			{
				throw glare::Exception("Error while creating video writer: " + exceptionDesc(exception));
			}
		}
	}
	catch(glare::Exception&)
	{
		releaseObjects(); // The destructor isn't called if the constructor throws.
		throw;
	}
}


AVFVideoWriter::~AVFVideoWriter()
{
	releaseObjects();
}


void AVFVideoWriter::releaseObjects()
{
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	AVAssetWriterInputPixelBufferAdaptor* adaptor = (AVAssetWriterInputPixelBufferAdaptor*)m_adaptor;

	@autoreleasepool
	{
		// If finalise() wasn't called or failed, cancel writing, which stops the encoder.
		if(video_writer && (video_writer.status == AVAssetWriterStatusWriting))
			[video_writer cancelWriting];
	}

	[adaptor release];
	[writer_input release];
	[video_writer release];

	m_video_writer = nullptr;
	m_writer_input = nullptr;
	m_adaptor = nullptr;
}


void AVFVideoWriter::writeFrame(const uint8* source_data, size_t source_row_stride_B)
{
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	AVAssetWriterInputPixelBufferAdaptor* adaptor = (AVAssetWriterInputPixelBufferAdaptor*)m_adaptor;

	@autoreleasepool
	{
		@try
		{
			// Wait until the input can accept more data.
			while(!writer_input.readyForMoreMediaData)
			{
				if(video_writer.status != AVAssetWriterStatusWriting)
					throw glare::Exception("Video writer stopped while waiting to write frame: " + errorDesc([video_writer error]));
				PlatformUtils::Sleep(1);
			}

			CVPixelBufferPoolRef pool = adaptor.pixelBufferPool; // Created by startWriting.
			if(!pool)
				throw glare::Exception("Video writer has no pixel buffer pool: " + errorDesc([video_writer error]));

			CVPixelBufferRef pixel_buffer = nullptr;
			const CVReturn res = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &pixel_buffer);
			if(res != kCVReturnSuccess || !pixel_buffer)
				throw glare::Exception("Failed to create pixel buffer for video frame (error " + toString((int)res) + ").");

			// Convert from RGB to BGRA.  Both have the top row first.
			CVPixelBufferLockBaseAddress(pixel_buffer, 0);
			uint8* const dest_data = (uint8*)CVPixelBufferGetBaseAddress(pixel_buffer);
			const size_t dest_row_stride_B = CVPixelBufferGetBytesPerRow(pixel_buffer);
			for(uint32 y=0; y<vid_params.height; ++y)
			{
				const uint8* src_row = source_data + source_row_stride_B * y;
				uint8* dest_row = dest_data + dest_row_stride_B * y;
				for(uint32 x=0; x<vid_params.width; ++x)
				{
					dest_row[x * 4 + 0] = src_row[x * 3 + 2];
					dest_row[x * 4 + 1] = src_row[x * 3 + 1];
					dest_row[x * 4 + 2] = src_row[x * 3 + 0];
					dest_row[x * 4 + 3] = 255;
				}
			}
			CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);

			// Presentation time is frame_index / fps seconds.  Using a timescale of fps * 1000 makes this exact for integer and NTSC-style (e.g. 29.97) frame rates.
			const CMTime presentation_time = CMTimeMake((int64_t)frame_index * 1000, (int32_t)std::lround(vid_params.fps * 1000));

			const BOOL appended = [adaptor appendPixelBuffer:pixel_buffer withPresentationTime:presentation_time];
			CVPixelBufferRelease(pixel_buffer);
			if(!appended)
				throw glare::Exception("Failed to write video frame: " + errorDesc([video_writer error]));
		}
		@catch(NSException* exception)
		{
			throw glare::Exception("Error while writing video frame: " + exceptionDesc(exception));
		}
	}

	frame_index++;
}


void AVFVideoWriter::finalise()
{
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;

	@autoreleasepool
	{
		@try
		{
			[writer_input markAsFinished];

			// Block until writing has finished.
			dispatch_semaphore_t finished = dispatch_semaphore_create(0);
			[video_writer finishWritingWithCompletionHandler:^{
				dispatch_semaphore_signal(finished);
			}];
			dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);
			dispatch_release(finished);

			if(video_writer.status != AVAssetWriterStatusCompleted)
				throw glare::Exception("Error while finishing writing video: " + errorDesc([video_writer error]));
		}
		@catch(NSException* exception)
		{
			throw glare::Exception("Error while finalising video: " + exceptionDesc(exception));
		}
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"
#include "../graphics/ImageMap.h"


static void testWithStandard(VidParams::CompressionStandard standard, uint32 bitrate)
{
	conPrint("Writing video with " + std::string((standard == VidParams::CompressionStandard_H264) ? "H.264" : "HEVC") + " codec, bit rate: " + toString(bitrate) + " bits/s");
	try
	{
		VidParams params;
		params.bitrate = bitrate;
		params.fps = 60;
		params.width = 800;
		params.height = 600;
		params.standard = standard;
		AVFVideoWriter writer("test_" + std::string((standard == VidParams::CompressionStandard_H264) ? "H.264" : "HEVC") + "_" + toString(bitrate) + ".mp4", params);

		Timer timer;
		const int NUM_FRAMES = 400;
		for(int i=0; i<NUM_FRAMES; ++i)
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

			const int ob_x = (int)(((float)i / NUM_FRAMES) * params.width);
			for(uint32 y=100; y<110; ++y)
				for(int x=ob_x; x<ob_x + 10; ++x)
				{
					if(x < (int)params.width)
						map.getPixel(x, y)[1] = 255;
				}

			writer.writeFrame(map.getData(), map.getWidth() * 3);
		}

		writer.finalise();

		const double fps = NUM_FRAMES / timer.elapsed();
		conPrint("FPS processed: " + toString(fps));
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void AVFVideoWriter::test()
{
	testWithStandard(VidParams::CompressionStandard_H264, /*bitrate=*/10000);
	testWithStandard(VidParams::CompressionStandard_HEVC, /*bitrate=*/10000);
	testWithStandard(VidParams::CompressionStandard_H264, /*bitrate=*/100000000);
	testWithStandard(VidParams::CompressionStandard_HEVC, /*bitrate=*/100000000);
}


#endif // BUILD_TESTS


#endif // OSX
