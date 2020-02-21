/*=====================================================================
AVFVideoWriter.cpp
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "AVFVideoWriter.h"


#ifdef OSX


#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"

#include <AVFoundation/AVFoundation.h>
#import <Foundation/NSURL.h>


AVFVideoWriter::AVFVideoWriter(const std::string& URL, const VidParams& vid_params_)
:	frame_index(0),
	vid_params(vid_params_)
{
	NSString* s = [NSString stringWithUTF8String:URL.c_str()];
	NSURL* url = [[NSURL alloc] initFileURLWithPath: s];
	
	NSError* the_error = nil;
	AVAssetWriter* video_writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&the_error];
	
	if(video_writer == nil)
		throw Indigo::Exception("Failed to create video writer.");
	
	NSDictionary* video_settings = [NSDictionary dictionaryWithObjectsAndKeys:
									AVVideoCodecTypeH264, AVVideoCodecKey,
									[NSNumber numberWithInt:vid_params.width], AVVideoWidthKey,
									[NSNumber numberWithInt:vid_params.height], AVVideoHeightKey,
									nil];
	
	AVAssetWriterInput* writer_input = [[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
																	  outputSettings:video_settings] retain];
	
	if(writer_input == nil)
		throw Indigo::Exception("Failed to create video writer.");
	
	AVAssetWriterInputPixelBufferAdaptor* adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:writer_input sourcePixelBufferAttributes:nil];
	
	m_video_writer = video_writer;
	m_writer_input = writer_input;
	m_adaptor = adaptor;
	
	[video_writer addInput:writer_input];
	
	[video_writer startWriting];
	[video_writer startSessionAtSourceTime:kCMTimeZero];
}


AVFVideoWriter::~AVFVideoWriter()
{
}

void releaseCallback(void* release_ref_con, const void* base_address)
{
	free((void*)base_address);
}

void AVFVideoWriter::writeFrame(const uint8* source_data, size_t source_row_stride_B)
{
	//AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	//AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	AVAssetWriterInputPixelBufferAdaptor* adaptor = (AVAssetWriterInputPixelBufferAdaptor*)m_adaptor;
	
	double frame_time = 1.0 / vid_params.fps;
	CMTime cur_time = CMTimeMake(frame_index*frame_time*1000, 1000);
	
	CVPixelBufferRef pixel_buffer = nil;
	CVPixelBufferCreateWithBytes(nil, // Default allocator
								 vid_params.width, vid_params.height, kCVPixelFormatType_24RGB, (void*)source_data, source_row_stride_B, releaseCallback, nil, nil, &pixel_buffer);
	
	if(pixel_buffer)
	{
		//[writer_input appendSampleBuffer:pixel_buffer];
		[adaptor appendPixelBuffer:pixel_buffer withPresentationTime:cur_time];
	}
	else
		throw Indigo::Exception("Failed to write frame.");
	
	frame_index++;
}


void AVFVideoWriter::finalise()
{
	AVAssetWriter* video_writer = (AVAssetWriter*)m_video_writer;
	AVAssetWriterInput* writer_input = (AVAssetWriterInput*)m_writer_input;
	
	[writer_input markAsFinished];
	//[video_writer finishWriting];
	
	[video_writer finishWritingWithCompletionHandler:^{
	}]; // end videoWriter finishWriting Block
	
	// Block until video writing is done.
	while(1)
	{
		if(video_writer.status == AVAssetWriterStatusCompleted)
			break;
		else if(video_writer.status == AVAssetWriterStatusFailed)
			throw Indigo::Exception("Error while finishing writing video.");
		
		// else writing still in progress presumably, keep waiting..
		PlatformUtils::Sleep(1);
	}
}


#if 1 // BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"


void AVFVideoWriter::test()
{
	
	try
	{
		{
			//WMFVideoReader reader("D:\\video\\test5.avi");
			VidParams params;
			params.bitrate = 100000000;
			params.fps = 60;
			params.width = 800;
			params.height = 600;
			AVFVideoWriter writer("test.mpg", params);

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


#endif // BUILD_TESTS


#endif // OSX
