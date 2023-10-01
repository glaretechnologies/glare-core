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
	glare::AllocatorVector<uint8, 16> mipmap_data; // Data for all mip-map levels, possibly compressed.
	Reference<const Map2D> converted_image;
};


class TextureData : public ThreadSafeRefCounted
{
public:
	TextureData() : frame_durations_equal(false), num_mip_levels(1), data_is_compressed(false) {}

	inline size_t compressedSizeBytes() const;

	size_t W, H, bytes_pp;

	size_t num_mip_levels; // Generally equal to level_offsets.size(), apart from when converted_image points to a compressed image, 
	// in which case is equal to converted_image->mipmap_level_data.size().

	bool data_is_compressed;

	struct LevelOffsetData
	{
		LevelOffsetData() {}
		LevelOffsetData(size_t offset_, size_t level_size_) : offset(offset_), level_size(level_size_) {}
		size_t offset; // Offset in frames[i].mipmap_data.  Same for all frames.
		size_t level_size; // Size (in bytes) of level data in frames[i].mipmap_data.  Same for all frames.
	};

	std::vector<LevelOffsetData> level_offsets;

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
		sum += frames[i].mipmap_data.dataSizeBytes();
	return sum;
}
