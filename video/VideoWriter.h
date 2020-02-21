/*=====================================================================
VideoWriter.h
-------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:58:43 +1300
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"


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

=====================================================================*/
class VideoWriter : public RefCounted
{
public:
	VideoWriter();
	virtual ~VideoWriter();
};
