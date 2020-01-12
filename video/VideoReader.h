/*=====================================================================
VideoReader.h
-------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:58:43 +1300
=====================================================================*/
#pragma once



/*=====================================================================
VideoReader
-----------

=====================================================================*/
class VideoReader
{
public:
	VideoReader();
	~VideoReader();

	virtual void getAndLockNextFrame(unsigned char*& frame_buffer_out) = 0; // frame_buffer_out will be set to NULL if we have reached EOF
	virtual void unlockFrame() = 0;
};
