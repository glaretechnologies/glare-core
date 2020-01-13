/*=====================================================================
VideoReader.h
-------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:58:43 +1300
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"


struct FrameInfo
{
	double frame_time;
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

	virtual FrameInfo getAndLockNextFrame(unsigned char*& frame_buffer_out, size_t& stride_B_out) = 0; // frame_buffer_out will be set to NULL if we have reached EOF
	virtual void unlockFrame() = 0;

	virtual void seek(double time) = 0;
};
