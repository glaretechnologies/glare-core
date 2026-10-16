/*=====================================================================
VideoWriter.h
-------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:58:43 +1300
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>


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
VideoWriter
-----------
Encodes RGB frames to a video file.
=====================================================================*/
class VideoWriter : public RefCounted
{
public:
	VideoWriter();
	virtual ~VideoWriter();

	// Creates the writer for the current platform (WMFVideoWriter on Windows, AVFVideoWriter on Mac).  Throws glare::Exception if not supported or on failure.
	static Reference<VideoWriter> create(const std::string& path, const VidParams& vid_params);

	// data is 8-bit RGB, top row first.
	virtual void writeFrame(const uint8* data, size_t source_row_stride_B) = 0;

	virtual void finalise() = 0;
};


typedef Reference<VideoWriter> VideoWriterRef;
