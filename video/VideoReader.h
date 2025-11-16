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
#include "../utils/PoolAllocator.h"
#include "../utils/GlareAllocator.h"
#include "../utils/ThreadSafeQueue.h"
#include "../utils/ArrayRef.h"


class VideoReader;


class SampleInfo : public ThreadSafeRefCounted
{
public:
	SampleInfo() : is_EOS_marker(false), frame_buffer(0), width(0), height(0), stride_B(0), top_down(true), buffer_len_B(0), is_audio(false) {}
	virtual ~SampleInfo() {}

	bool is_EOS_marker;

	double frame_time; // presentation time
	double frame_duration;
	uint8* frame_buffer;
	uint64 width;
	uint64 height;
	uint64 stride_B; // >= 0
	bool top_down;

	bool is_audio;
	// Audio:
	uint32 num_channels;
	uint32 sample_rate_hz;
	uint64 buffer_len_B; // Size of frame_buffer, in bytes.
	uint32 bits_per_sample;

	Reference<glare::PoolAllocator> allocator;
	int allocation_index;
};

typedef Reference<SampleInfo> SampleInfoRef;


// Template specialisation of destroyAndFreeOb for FrameInfo.
template <>
inline void destroyAndFreeOb<SampleInfo>(SampleInfo* ob)
{
	Reference<glare::PoolAllocator> allocator = ob->allocator;

	// Destroy object
	ob->~SampleInfo();

	// Free object mem
	if(allocator.nonNull())
		ob->allocator->free(ob->allocation_index);
	else
		delete ob;
}


//class VideoReaderCallback
//{
//public:
//	// NOTE: These methods may be called from another thread!
//	virtual void frameDecoded(VideoReader* vid_reader, const Reference<SampleInfo>& frameinfo) = 0;
//
//	virtual void endOfStream(VideoReader* /*vid_reader*/) {}
//};


/*=====================================================================
VideoReader
-----------

=====================================================================*/
class VideoReader : public ThreadSafeRefCounted
{
public:
	VideoReader();
	virtual ~VideoReader();

	virtual void startReadingNextSample() = 0;

	virtual Reference<SampleInfo> getAndLockNextSample(bool just_get_vid_sample) = 0;

	virtual void seek(double time) = 0;


	ThreadSafeQueue<Reference<SampleInfo>> frame_queue; // Decoded frames, oldest at the front.
};
