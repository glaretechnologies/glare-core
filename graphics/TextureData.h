/*=====================================================================
TextureData.h
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../graphics/Map2D.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/AllocatorVector.h"
#include <vector>


/*=====================================================================
TextureData
-----------
Processed texture data, produced by TextureProcessing.
Contains Mipmap data, that is possibly compressed.
=====================================================================*/


class TextureFrameData
{
public:
	glare::AllocatorVector<uint8, 16> compressed_data; // Compressed data for all mip-map levels.
	Reference<const Map2D> converted_image;
};


class TextureData : public ThreadSafeRefCounted
{
public:
	TextureData() : frame_durations_equal(false), num_mip_levels(1) {}

	inline size_t compressedSizeBytes() const;

	size_t W, H, bytes_pp;

	size_t num_mip_levels;

	std::vector<size_t> level_offsets;

	std::vector<TextureFrameData> frames; // will have 1 element for non-animated images, more than 1 for animated gifs etc..

	std::vector<double> frame_end_times;

	bool frame_durations_equal;
	double recip_frame_duration; // Set if frame_durations_equal is true.
	double last_frame_end_time;
	size_t num_frames; // == frames.size() == frame_end_times.size()
};


size_t TextureData::compressedSizeBytes() const
{
	size_t sum = 0;
	for(size_t i=0; i<frames.size(); ++i)
		sum += frames[i].compressed_data.dataSizeBytes();
	return sum;
}
