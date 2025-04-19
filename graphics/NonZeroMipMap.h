/*=====================================================================
NonZeroMipMap.h
---------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "ImageMapUInt1.h"
#include <vector>


/*=====================================================================
NonZeroMipMap
--------------

NOTE: not thoroughly tested.


level
0               1         2


x  x  x  x      y  y      z

x  x  x  x      y  y

x  x  x  x

x  x  x  x


                      /\
                  /       \
               /             \
            /                   \
-----------------------------------------------
|          |           |          |           |
0          1           2          3

=====================================================================*/
class NonZeroMipMap
{
public:
	struct LayerInfo
	{
		uint32 offset;
		uint32 width;
	};

	bool isSectionNonZero(int level, int x, int y) const
	{
		assert(level >= 0 && level < (int)num_levels);

		const uint32 level_offset = layer_info[level].offset;
		const uint32 level_w      = layer_info[level].width;

		// clamp x and y to avoid out-of-bounds reads
		x = myClamp(x, 0, (int)level_w-1);
		y = myClamp(y, 0, (int)level_w-1);

		const uint32 bit_index = x + y*level_w;
		const uint32 uint_index    = bit_index / 32;
		const uint32 in_uint_index = bit_index % 32;

		return ((data[level_offset + uint_index] >> in_uint_index) & 0x1) != 0;
	}


	static inline int roundToInt(float x) { return (int)(x + 0.5f); }

	// Assumes start and end coordinates are at power-of-two pixel coordinates.
	bool isRegionNonZero(float norm_start_x, float norm_start_y, float norm_width, float norm_height) const
	{
		// Work out level
		const int pixel_w = roundToInt(norm_width * image_w); // Get region width in image pixels

		//const int level = myClamp((int)std::round(std::log2(pixel_w)), 0, (int)num_levels);
		const int level = myMin((int)Maths::intLogBase2(myMax((uint32)pixel_w, 1u)), (int)num_levels - 1);

		const int level_0_px = roundToInt(norm_start_x * image_w); // Get x image pixel coordinates of the start of the region
		const int level_0_py = roundToInt(norm_start_y * image_w); // Get y image pixel coordinates of the start of the region

		const int level_px = level_0_px >> level; // Get pixel coordinates at level.  They are smaller by a factor of 2^level
		const int level_py = level_0_py >> level;
		return isSectionNonZero(level, level_px, level_py);
	}


	void build(const ImageMapUInt1& image_map, int channel)
	{
		const size_t W = image_map.getWidth();
		this->image_w = (uint32)W;

		size_t total_num_uint32s = 0;
		size_t layer_W = W;
		int layer = 0;
		while(layer_W >= 1)
		{
			const size_t layer_num_bits = layer_W*layer_W;
			const size_t layer_num_uint32s = Maths::roundedUpDivide<size_t>(layer_num_bits, 32);
			layer_info[layer].offset = (uint32)total_num_uint32s;
			layer_info[layer].width  = (uint32)layer_W;
			total_num_uint32s += layer_num_uint32s;
			layer++;
			layer_W /= 2;
		}

		data.resize(total_num_uint32s);
		const int num_layers = layer;
		this->num_levels = num_layers;

		layer_W = W;
		for(layer = 0; layer < num_layers; ++layer)
		{
			const size_t write_i = (size_t)layer_info[layer].offset;
	
			if(layer == 0)
			{
				assert(write_i == 0);
				// Taking advantage of the fact that both data and image_map.data are vectors of 32-bit uints used in the same way.
				assert(image_map.data.v.size() >= layer_W * layer_W / 32);
				for(size_t i=0; i<layer_W * layer_W / 32; ++i)
					data[i] = image_map.data.v[i];
			}
			else
			{
				// Read from previous level
				const size_t prev_level_offset = layer_info[layer - 1].offset;
				const size_t prev_level_width  = layer_info[layer - 1].width;

				const uint32* const prev_level_data = &data[prev_level_offset];

				for(size_t y=0; y<layer_W; ++y)
				for(size_t x=0; x<layer_W; ++x)
				{
					const size_t prev_bit_index_0_0 = x*2 +     y*2      *prev_level_width;
					const size_t prev_bit_index_1_0 = prev_bit_index_0_0 + 1;                     // x*2 + 1 + y*2      *prev_level_width;
					const size_t prev_bit_index_0_1 = prev_bit_index_0_0 + prev_level_width;      // x*2 +     (y*2 + 1)*prev_level_width;
					const size_t prev_bit_index_1_1 = prev_bit_index_0_0 + 1 + prev_level_width;  // x*2 + 1 + (y*2 + 1)*prev_level_width;

					const uint32 non_zero = 
						((prev_level_data[prev_bit_index_0_0/32] >> (prev_bit_index_0_0%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_1_0/32] >> (prev_bit_index_1_0%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_0_1/32] >> (prev_bit_index_0_1%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_1_1/32] >> (prev_bit_index_1_1%32)) & 0x1);

					if(non_zero != 0)
					{
						const size_t index = x + y*layer_W;
						const size_t uint_index    = index / 32;
						const size_t in_uint_index = index % 32;
						data[write_i + uint_index] |= (1 << in_uint_index);
					}
				}
			}

			layer_W /= 2;
		}
	}


	void build(const ImageMapUInt8& image_map, int channel)
	{
		const size_t W = image_map.getWidth();
		this->image_w = (uint32)W;

		size_t total_num_uint32s = 0;
		size_t layer_W = W;
		int layer = 0;
		while(layer_W >= 1)
		{
			const size_t layer_num_bits = layer_W*layer_W;
			const size_t layer_num_uint32s = Maths::roundedUpDivide<size_t>(layer_num_bits, 32);
			layer_info[layer].offset = (uint32)total_num_uint32s;
			layer_info[layer].width  = (uint32)layer_W;
			total_num_uint32s += layer_num_uint32s;
			layer++;
			layer_W /= 2;
		}

		data.resize(total_num_uint32s);
		const int num_layers = layer;
		this->num_levels = num_layers;

		layer_W = W;
		for(layer = 0; layer < num_layers; ++layer)
		{
			const size_t write_i = (size_t)layer_info[layer].offset;
	
			if(layer == 0)
			{
				assert(write_i == 0);
				for(size_t y=0; y<layer_W; ++y)
				for(size_t x=0; x<layer_W; ++x)
				{
					const size_t bit_index = x + y*layer_W;
					const size_t uint_index    = bit_index / 32;
					const size_t in_uint_index = bit_index % 32;
					if(image_map.getPixel(x, y)[channel] > 0)
						data[uint_index] |= (1 << in_uint_index);
				}
			}
			else
			{
				// Read from previous level
				const size_t prev_level_offset = layer_info[layer - 1].offset;
				const size_t prev_level_width  = layer_info[layer - 1].width;

				const uint32* const prev_level_data = &data[prev_level_offset];

				for(size_t y=0; y<layer_W; ++y)
				for(size_t x=0; x<layer_W; ++x)
				{
					const size_t prev_bit_index_0_0 = x*2 +     y*2      *prev_level_width;
					const size_t prev_bit_index_1_0 = prev_bit_index_0_0 + 1;                     // x*2 + 1 + y*2      *prev_level_width;
					const size_t prev_bit_index_0_1 = prev_bit_index_0_0 + prev_level_width;      // x*2 +     (y*2 + 1)*prev_level_width;
					const size_t prev_bit_index_1_1 = prev_bit_index_0_0 + 1 + prev_level_width;  // x*2 + 1 + (y*2 + 1)*prev_level_width;

					const uint32 non_zero = 
						((prev_level_data[prev_bit_index_0_0/32] >> (prev_bit_index_0_0%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_1_0/32] >> (prev_bit_index_1_0%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_0_1/32] >> (prev_bit_index_0_1%32)) & 0x1) | 
						((prev_level_data[prev_bit_index_1_1/32] >> (prev_bit_index_1_1%32)) & 0x1);

					if(non_zero != 0)
					{
						const size_t index = x + y*layer_W;
						const size_t uint_index    = index / 32;
						const size_t in_uint_index = index % 32;
						data[write_i + uint_index] |= (1 << in_uint_index);
					}
				}
			}

			layer_W /= 2;
		}
	}

	static void test();

private:
	uint32 image_w;
	uint32 num_levels;
	std::vector<uint32> data;
	LayerInfo layer_info[32];
};
