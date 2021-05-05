/*=====================================================================
VideoReader.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Platform.h"
#include "../utils/GlareAllocator.h"


class VideoReader;


class FrameInfo : public ThreadSafeRefCounted
{
public:
	FrameInfo() : frame_buffer(0), width(0), height(0), stride_B(0), top_down(true) {}
	virtual ~FrameInfo() {}

	double frame_time;
	uint8* frame_buffer;
	uint64 width;
	uint64 height;
	uint64 stride_B; // >= 0
	bool top_down;

	Reference<glare::Allocator> allocator;
};

typedef Reference<FrameInfo> FrameInfoRef;


// Template specialisation of destroyAndFreeOb for FrameInfo.
template <>
inline void destroyAndFreeOb<FrameInfo>(FrameInfo* ob)
{
	Reference<glare::Allocator> allocator = ob->allocator;

	// Destroy object
	ob->~FrameInfo();

	// Free object mem
	if(allocator.nonNull())
		ob->allocator->free(ob);
	else
		delete ob;
}


class VideoReaderCallback
{
public:
	// NOTE: These methods may be called from another thread!
	virtual void frameDecoded(VideoReader* vid_reader, const Reference<FrameInfo>& frameinfo) = 0;

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

	virtual Reference<FrameInfo> getAndLockNextFrame() = 0; // FrameInfo.frame_buffer will be set to NULL if we have reached EOF.

	virtual void seek(double time) = 0;
};
