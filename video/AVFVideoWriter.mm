/*=====================================================================
AVFVideoWriter.cpp
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "AVFVideoWriter.h"


#ifdef OSX


#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/ConPrint.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/NSURL.h>


static const std::string errorDesc(NSError* err)
{
	if(!err)
		return "";
	NSString* desc = [err localizedDescription];
	return desc ? std::string([desc UTF8String]) : "";
}


AVFVideoWriter::AVFVideoWriter(const std::string& URL, const VidParams& vid_params_)
:	frame_index(0),
	vid_params(vid_params_)
{
	// AVAssetWriter fails if the target file already exists, so remove it first if present.
	try
	{
		if(FileUtils::fileExists(URL))
			FileUtils::deleteFile(URL);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
	
	NSString* s = [NSString stringWithUTF8String:URL.c_str()];
	NSURL* url = [[NSURL alloc] initFileURLWithPath: s];
	
	NSError* err = nil;
	AVAssetWriter* video_writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
	if(video_writer == nil)
		throw Indigo::Exception("Failed to create video writer: " + errorDesc(err));
	
	NSDictionary* video_settings = [NSDictionary dictionaryWithObjectsAndKeys:
									AVVideoCodecTypeH264, AVVideoCodecKey,
									[NSNumber numberWithInt:vid_params.width], AVVideoWidthKey,
									[NSNumber numberWithInt:vid_params.height], AVVideoHeightKey,
									nil];
	
	AVAssetWriterInput* writer_input = [[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
																		   outputSettings:video_settings] retain];
	if(writer_input == nil)
		throw Indigo::Exception("Failed to create asset writer input.");
	
	AVAssetWriterInputPixelBufferAdaptor* adaptor = [[AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:writer_input sourcePixelBufferAttributes:nil] retain];
	
	if(adaptor == nil)
		throw Indigo::Exception("Failed to create input pixel buffer adaptor.");
	
	m_video_writer = video_writer;
	m_writer_input = writer_input;
	m_adaptor = adaptor;
	
	[video_writer addInput:writer_input];
	[video_writer startWriting];
	[video_writer startSessionAtSourceTime:kCMTimeZero];
	
	[url release];
}


AVFVideoWriter::~AVFVideoWriter()
{
	AVAssetWriterInputPixelBufferAdaptor* adaptor = (AVAssetWriterInputPixelBufferAdaptor*)m_adaptor;
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	
	[adaptor release]; // We manually retained in the constructor.
	[writer_input release]; // We manually retained in the constructor.
	[video_writer release];
}


static void releaseCallback(void* release_ref_con, const void* base_address)
{
	free((void*)base_address);
}


void AVFVideoWriter::writeFrame(const uint8* source_data, size_t source_row_stride_B)
{
	AVAssetWriterInputPixelBufferAdaptor* adaptor = (AVAssetWriterInputPixelBufferAdaptor*)m_adaptor;
	
	const double frame_time = 1.0 / vid_params.fps;
	CMTime cur_time = CMTimeMake(frame_index * frame_time * 1000, 1000);
	
	CVPixelBufferRef pixel_buffer = nil;
	CVPixelBufferCreateWithBytes(nil, // Default allocator
								 vid_params.width, vid_params.height, kCVPixelFormatType_24RGB, (void*)source_data, source_row_stride_B, releaseCallback, nil, nil, &pixel_buffer);
	
	if(pixel_buffer)
	{
		[adaptor appendPixelBuffer:pixel_buffer withPresentationTime:cur_time];
	}
	else
	{
		
		throw Indigo::Exception("Failed to write frame.");
	}
	
	frame_index++;
}


void AVFVideoWriter::finalise()
{
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	
	[writer_input markAsFinished];
	
	[video_writer finishWritingWithCompletionHandler:^{
	}]; // end videoWriter finishWriting Block
	
	// Block until video writing is done.
	while(1)
	{
		if(video_writer.status == AVAssetWriterStatusCompleted)
			break;
		else if(video_writer.status == AVAssetWriterStatusFailed)
			throw Indigo::Exception("Error while finishing writing video: " + errorDesc([video_writer error]));
		
		// else writing still in progress presumably, keep waiting..
		PlatformUtils::Sleep(1);
	}
}


#if 1 // BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../graphics/ImageMap.h"


void AVFVideoWriter::test()
{
	
	try
	{
		{
			VidParams params;
			params.bitrate = 100000000;
			params.fps = 60;
			params.width = 800;
			params.height = 600;
			AVFVideoWriter writer("test.mpg", params);

			Timer timer;
			const int NUM_FRAMES = 40;
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
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS


#endif // OSX
