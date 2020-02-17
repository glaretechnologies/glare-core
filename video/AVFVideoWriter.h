/*=====================================================================
AVFVideoWriter.h
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "VideoWriter.h"
#include "ComObHandle.h"
#include "IncludeWindows.h"
#include <string>


#ifdef OSX


struct VidParams
{
	enum CompressionStandard
	{
		CompressionStandard_H264,
		CompressionStandard_HEVC // a.k.a. H.265
	};

	VidParams() : standard(CompressionStandard_H264) {}

	uint32 bitrate; // Approximate data rate of the video stream, in bits per second.
	uint32 width;
	uint32 height;
	double fps;

	CompressionStandard standard;
};


/*=====================================================================
AVFVideoWriter
--------------
Apple AVFoundation video writer.
=====================================================================*/
class AVFVideoWriter : public VideoWriter
{
public:
	AVFVideoWriter(const std::string& URL, const VidParams& vid_params); // Throws Indigo::Exception
	~AVFVideoWriter();

	static void test();

	// RGB data
	virtual void writeFrame(const uint8* data, size_t source_row_stride_B);

	virtual void finalise();

	const VidParams& getVidParams() const { return vid_params; }
private:
	uint64 frame_index;
	VidParams vid_params;
};


#endif // OSX
