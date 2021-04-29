/*=====================================================================
VideoReader.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include "../utils/Platform.h"


class VideoReader;


struct FrameInfo
{
	FrameInfo() : frame_buffer(0), width(0), height(0), stride_B(0), top_down(true), media_buffer(0), buffer2d(0) {}
	
	double frame_time;
	uint8* frame_buffer;
	size_t width;
	size_t height;
	size_t stride_B; // >= 0
	bool top_down;

	void* media_buffer;
	void* buffer2d;
};


class VideoReaderCallback
{
public:
	// NOTE: These methods may be called from another thread!
	virtual void frameDecoded(VideoReader* vid_reader, const FrameInfo& frameinfo) = 0;

	virtual void endOfStream(VideoReader* vid_reader) {}
};


/*=====================================================================
VideoReader
-----------

=====================================================================*/
class VideoReader : public RefCounted
{
public:
	VideoReader();
	virtual ~VideoReader();

	virtual void startReadingNextFrame() = 0;

	virtual FrameInfo getAndLockNextFrame() = 0; // FrameInfo.frame_buffer will be set to NULL if we have reached EOF.

	virtual void unlockAndReleaseFrame(const FrameInfo& frameinfo) = 0;

	virtual void seek(double time) = 0;
};
