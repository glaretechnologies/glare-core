/*=====================================================================
TextureProcessing.cpp
---------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "TextureProcessing.h"


#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
#include "../graphics/DXTCompression.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/IncludeHalf.h"
#include <vector>
#include <graphics/CompressedImage.h>


// Downsize previous mip level image to current mip level.
// Just uses kinda crappy 2x2 pixel box filter.
// alpha_coverage_out: frac of pixels with alpha >= 0.5, set if N == 4.
// N = num components per pixel.
void TextureProcessing::downSampleToNextMipMapLevel(size_t prev_W, size_t prev_H, size_t N, const uint8* prev_level_image_data, float alpha_scale, size_t level_W, size_t level_H,
	uint8* data_out, float& alpha_coverage_out)
{
	Timer timer;
	uint8* const dst_data = data_out;
	const uint8* const src_data = prev_level_image_data;
	const size_t src_W = prev_W;
	const size_t src_H = prev_H;

	// Because the new width is max(1, floor(old_width/2)), we have old_width >= new_width*2 in all cases apart from when old_width = 1.
	// If old_width >= new_width*2, then 
	// old_width >= (new_width-1)*2 + 2
	// old_width > (new_width-1)*2 + 1
	// (new_width-1)*2 + 1 < old_width
	// in other words the support pixels of the box filter are guaranteed to be in range (< old_width)
	// Likewise for old height etc..
	assert((src_W == 1) || ((level_W - 1) * 2 + 1 < src_W));
	assert((src_H == 1) || ((level_H - 1) * 2 + 1 < src_H));

	assert(N == 3 || N == 4);
	if(N == 3)
	{
		if(src_W == 1)
		{
			// This is 1xN texture being downsized to 1xfloor(N/2)
			assert(level_W == 1 && src_H > 1);
			assert((level_H - 1) * 2 + 1 < src_H);

			for(int y=0; y<(int)level_H; ++y)
			{
				int val[3] = { 0, 0, 0 };
				size_t sx = 0;
				size_t sy = y*2;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
				}
				sy = y*2 + 1;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
				}
				uint8* const dest_pixel = dst_data + (0 + level_W * y) * N;
				dest_pixel[0] = (uint8)(val[0] / 2);
				dest_pixel[1] = (uint8)(val[1] / 2);
				dest_pixel[2] = (uint8)(val[2] / 2);
			}
		}
		else if(src_H == 1)
		{
			// This is Nx1 texture being downsized to floor(N/2)x1
			assert(level_H == 1 && src_W > 1);
			assert((level_W - 1) * 2 + 1 < src_W);

			for(int x=0; x<(int)level_W; ++x)
			{
				int val[3] = { 0, 0, 0 };
				int sx = x*2;
				int sy = 0;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
				}
				sx = x*2 + 1;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
				}
				uint8* const dest_pixel = dst_data + (x + level_W * 0) * N;
				dest_pixel[0] = (uint8)(val[0] / 2);
				dest_pixel[1] = (uint8)(val[1] / 2);
				dest_pixel[2] = (uint8)(val[2] / 2);
			}
		}
		else
		{
			assert(src_W >= 2 && src_H >= 2);
			assert((level_W - 1) * 2 + 1 < src_W);
			assert((level_H - 1) * 2 + 1 < src_H);

			// In this case all reads should be in-bounds
			for(int y=0; y<(int)level_H; ++y)
				for(int x=0; x<(int)level_W; ++x)
				{
					int val[3] = { 0, 0, 0 };
					int sx = x*2;
					int sy = y*2;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
					}
					sx = x*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
					}
					sx = x*2;
					sy = y*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
					}
					sx = x*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
					}

					uint8* const dest_pixel = dst_data + (x + level_W * y) * N;
					dest_pixel[0] = (uint8)(val[0] / 4);
					dest_pixel[1] = (uint8)(val[1] / 4);
					dest_pixel[2] = (uint8)(val[2] / 4);
				}
		}
	}
	else // else if(N == 4):
	{
		int num_opaque_px = 0;
		if(src_W == 1)
		{
			// This is 1xN texture being downsized to 1xfloor(N/2)
			assert(level_W == 1 && src_H > 1);
			assert((level_H - 1) * 2 + 1 < src_H);

			for(int y=0; y<(int)level_H; ++y)
			{
				int val[4] = { 0, 0, 0, 0 };
				int sx = 0;
				int sy = y*2;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
					val[3] += src[3];
				}
				sy = y*2 + 1;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
					val[3] += src[3];
				}
				uint8* const dest_pixel = dst_data + (0 + level_W * y) * N;
				dest_pixel[0] = (uint8)(val[0] / 2);
				dest_pixel[1] = (uint8)(val[1] / 2);
				dest_pixel[2] = (uint8)(val[2] / 2);
				dest_pixel[3] = (uint8)(myMin(255.f, alpha_scale * (val[3] / 2)));

				if(dest_pixel[3] >= 186)
					num_opaque_px++;
			}
		}
		else if(src_H == 1)
		{
			// This is Nx1 texture being downsized to floor(N/2)x1
			assert(level_H == 1 && src_W > 1);
			assert((level_W - 1) * 2 + 1 < src_W);

			for(int x=0; x<(int)level_W; ++x)
			{
				int val[4] = { 0, 0, 0, 0 };
				int sx = x*2;
				int sy = 0;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
					val[3] += src[3];
				}
				sx = x*2 + 1;
				{
					const uint8* src = src_data + (sx + src_W * sy) * N;
					val[0] += src[0];
					val[1] += src[1];
					val[2] += src[2];
					val[3] += src[3];
				}
				uint8* const dest_pixel = dst_data + (x + level_W * 0) * N;
				dest_pixel[0] = (uint8)(val[0] / 2);
				dest_pixel[1] = (uint8)(val[1] / 2);
				dest_pixel[2] = (uint8)(val[2] / 2);
				dest_pixel[3] = (uint8)(myMin(255.f, alpha_scale * (val[3] / 2)));

				if(dest_pixel[3] >= 186)
					num_opaque_px++;
			}
		}
		else
		{
			assert(src_W >= 2 && src_H >= 2);
			assert((level_W - 1) * 2 + 1 < src_W);
			assert((level_H - 1) * 2 + 1 < src_H);

			// In this case all reads should be in-bounds
			for(int y=0; y<(int)level_H; ++y)
				for(int x=0; x<(int)level_W; ++x)
				{
					int val[4] = { 0, 0, 0, 0 };
					int sx = x*2;
					int sy = y*2;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
						val[3] += src[3];
					}
					sx = x*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
						val[3] += src[3];
					}
					sx = x*2;
					sy = y*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
						val[3] += src[3];
					}
					sx = x*2 + 1;
					{
						const uint8* src = src_data + (sx + src_W * sy) * N;
						val[0] += src[0];
						val[1] += src[1];
						val[2] += src[2];
						val[3] += src[3];
					}

					uint8* const dest_pixel = dst_data + (x + level_W * y) * N;
					dest_pixel[0] = (uint8)(val[0] / 4);
					dest_pixel[1] = (uint8)(val[1] / 4);
					dest_pixel[2] = (uint8)(val[2] / 4);
					dest_pixel[3] = (uint8)(myMin(255.f, alpha_scale * (val[3] / 4)));

					if(dest_pixel[3] >= 186) // 186 = floor(256 * (0.5 ^ (1/2.2))), e.g. the value that when divided by 256 and then raised to the power of 2.2 (~ sRGB gamma), is 0.5.
						num_opaque_px++;
				}
		}

		alpha_coverage_out = num_opaque_px / (float)(level_W * level_H);
	}

	//conPrint("downSampleToNextMipMapLevel took " + timer.elapsedString());
}


int TextureProcessing::computeNumMIPLevels(size_t W, size_t H)
{
	int num_levels = 0;
	for(size_t k=0; ; ++k)
	{
		num_levels++;

		const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
		const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

		if(level_W == 1 && level_H == 1)
			break;
	}
	return num_levels;
}


// Work out number of MIP levels we need, also work out byte offsets for the compressed data for each level, as we will store the compressed data for all MIP levels in one buffer.
// Store the offsets in texture_data->level_offsets.
// Also work out the sizes needed for the two temporary buffers which we will use for ping-ponging downsized data:
//
// level 0: use source iamge            (e.g. 512 x 512)
// level 1: use temp_tex_buf_a          (e.g. 256 x 256)
// level 2: use temp_tex_buf_b          (e.g. 128 x 128)
// level 2: use temp_tex_buf_a          (e.g. 64 x 64)
// level 3: use temp_tex_buf_b          (e.g. 32 x 32)
//
// So temp_tex_buf_a is used for odd levels, with max size used for level 1, and temp_tex_buf_b is used for even levels >= 2, with max size used for level 2.
//
// Only defined for 8-bit image data
static void computeMipLevelOffsets(TextureData* texture_data, bool do_compression, size_t& total_compressed_size_out, size_t& temp_tex_buf_a_size_out, size_t& temp_tex_buf_b_size_out)
{
	temp_tex_buf_a_size_out = 0;
	temp_tex_buf_b_size_out = 0;

	const size_t W			= texture_data->W;
	const size_t H			= texture_data->H;
	const size_t bytes_pp	= texture_data->numChannels();
	
	texture_data->level_offsets.reserve(16); // byte offset for each mipmap level
	size_t cur_offset = 0;
	for(size_t k=0; ; ++k)
	{
		const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
		const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

		const size_t level_uncompressed_tex_size = level_W * level_H * bytes_pp;

		if(k == 1)
			temp_tex_buf_a_size_out = level_uncompressed_tex_size;
		else if(k == 2)
			temp_tex_buf_b_size_out = level_uncompressed_tex_size;

		const size_t result_level_compressed_size = do_compression ? DXTCompression::getCompressedSizeBytes(level_W, level_H, bytes_pp) : level_uncompressed_tex_size;

		texture_data->level_offsets.push_back(TextureData::LevelOffsetData(cur_offset, result_level_compressed_size));

		cur_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(cur_offset + result_level_compressed_size, 8);
		if(level_W == 1 && level_H == 1)
			break;
	}

	total_compressed_size_out = cur_offset;
}

/*
See http://the-witness.net/news/2010/09/computing-alpha-mipmaps/ for the alpha coverage technique.
Basically we want to scale up the alpha values for mip levels such that the overall fraction of pixels with alpha > 0.5 is the same as in the base mip level.
*/
static float computeAlphaCoverage(const uint8* level_image_data, size_t level_W, size_t level_H)
{
	const int N = 4;
	int num_opaque_px = 0;
	for(int y=0; y<(int)level_H; ++y)
	for(int x=0; x<(int)level_W; ++x)
	{
		const uint8 alpha = level_image_data[(x + level_W * y) * N + 3];
		if(alpha >= 186) // 186 = floor(256 * (0.5 ^ (1/2.2))), e.g. the value that when divided by 256 and then raised to the power of 2.2 (~ sRGB gamma), is 0.5.
			num_opaque_px++;
	}
	return num_opaque_px / (float)(level_W * level_H);
}


// Compute possibly-DXT compressed image data for all MIP levels.  Does downsizing to produce the successive MIP levels.
// Stores the possibly-DXT compressed image data in texture_data->frames[cur_frame_i].compressed_data.
// Uses task_manager for multi-threading if non-null.
// Called by buildUInt8MapTextureData() and buildUInt8MapSequenceTextureData() to do the actual downsizing and compression work.
void TextureProcessing::buildMipMapDataForImageFrame(size_t total_compressed_size, bool do_compression, js::Vector<uint8, 16>& temp_tex_buf_a, js::Vector<uint8, 16>& temp_tex_buf_b, 
	DXTCompression::TempData& compress_temp_data, TextureData* texture_data, size_t cur_frame_i, const ImageMapUInt8* source_image, glare::TaskManager* task_manager)
{
	const size_t W			= texture_data->W;
	const size_t H			= texture_data->H;
	const size_t bytes_pp	= texture_data->numChannels();

	TextureFrameData& frame_data = texture_data->frames[cur_frame_i];

	frame_data.mipmap_data.resize(total_compressed_size);

	float level_0_alpha_coverage = 0;
	for(size_t k=0; ; ++k) // For each mipmap level:
	{
		// See https://www.khronos.org/opengl/wiki/Texture#Mipmap_completeness
		const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
		const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

		// conPrint("Building mipmap level " + toString(k) + " data, dims: " + toString(level_W) + " x " + toString(level_H));

		uint8* level_uncompressed_data;
		size_t level_uncompressed_data_size;
		if(k == 0)
		{
			level_uncompressed_data = (uint8*)source_image->getData(); // Read directly from source_image.
			level_uncompressed_data_size = source_image->getDataSize();

			if(bytes_pp == 4)
				level_0_alpha_coverage = computeAlphaCoverage(level_uncompressed_data, level_W, level_H);
		}
		else
		{
			// Downsize previous mip level image to current mip level
			const size_t prev_level_W = myMax((size_t)1, W / ((size_t)1 << (k-1)));
			const size_t prev_level_H = myMax((size_t)1, H / ((size_t)1 << (k-1)));
			const uint8* prev_level_uncompressed_data;
			if(k == 1)
				prev_level_uncompressed_data = source_image->getData(); // Special case for prev k == 0, read from source image
			else
			{
				if((k % 2) == 0) // Read from buffer a if k-1 is odd, so if k is even.
				{
					prev_level_uncompressed_data = temp_tex_buf_a.data();
					runtimeCheck(prev_level_W * prev_level_H * bytes_pp <= temp_tex_buf_a.size());
				}
				else
				{
					prev_level_uncompressed_data = temp_tex_buf_b.data();
					runtimeCheck(prev_level_W * prev_level_H * bytes_pp <= temp_tex_buf_b.size());
				}
			}
			if((k % 2) == 1) // Write to buffer a if k is odd, buffer b if even.
			{
				level_uncompressed_data = temp_tex_buf_a.data();
				level_uncompressed_data_size = temp_tex_buf_a.size();
				runtimeCheck(level_W * level_H * bytes_pp <= temp_tex_buf_a.size());
			}
			else
			{
				level_uncompressed_data = temp_tex_buf_b.data();
				level_uncompressed_data_size = temp_tex_buf_b.size();
				runtimeCheck(level_W * level_H * bytes_pp <= temp_tex_buf_b.size());
			}


			// Increase alpha scale until we get the same alpha coverage as the base LOD level. (See comment above)
			float alpha_scale = 1.f;
			if(bytes_pp == 4)
			{
				for(int i=0; i<8; ++i) // Bound max number of iters
				{
					float alpha_coverage;
					downSampleToNextMipMapLevel(prev_level_W, prev_level_H, bytes_pp, prev_level_uncompressed_data, alpha_scale, level_W, level_H, level_uncompressed_data, alpha_coverage);

					// conPrint("Mipmap level " + toString(k) + ": Using alpha scale of " + doubleToStringNSigFigs(alpha_scale, 4) + " resulted in a alpha_coverage of " + doubleToStringNSigFigs(alpha_coverage, 4) + 
					//	" (level_0_alpha_coverage: " + doubleToStringNSigFigs(level_0_alpha_coverage, 4) + ")");

					if(alpha_coverage >= 0.9f * level_0_alpha_coverage)
						break;

					alpha_scale *= 1.1f;
				}
			}
			else
			{
				float alpha_coverage;
				downSampleToNextMipMapLevel(prev_level_W, prev_level_H, bytes_pp, prev_level_uncompressed_data, alpha_scale, level_W, level_H, level_uncompressed_data, alpha_coverage);
			}
		}

		
		const size_t level_offset = texture_data->level_offsets[k].offset;
		const size_t level_size   = texture_data->level_offsets[k].level_size;
		if(do_compression)
		{
			runtimeCheck((level_size == DXTCompression::getCompressedSizeBytes(level_W, level_H, bytes_pp)) && 
				(level_offset + level_size <= frame_data.mipmap_data.size()));

			DXTCompression::compress(task_manager, compress_temp_data, level_W, level_H, bytes_pp, /*src data=*/level_uncompressed_data,
				/*dst data=*/&frame_data.mipmap_data[level_offset], /*dst size=*/level_size);
		}
		else
		{
			runtimeCheck((level_size == level_W * level_H * bytes_pp) && (level_offset + level_size <= frame_data.mipmap_data.size()) && (level_size <= level_uncompressed_data_size));
			std::memcpy(&frame_data.mipmap_data[level_offset], level_uncompressed_data, level_size); // TODO: could avoid copy
		}

		// If we have just finished computing the last mipmap level, we are done.
		if(level_W == 1 && level_H == 1)
			break;
	}
}


static Reference<ImageMapUInt8> convertUInt16ToUInt8ImageMap(const ImageMap<uint16, UInt16ComponentValueTraits>& map)
{
	Reference<ImageMapUInt8> new_map = new ImageMapUInt8(map.getWidth(), map.getHeight(), map.getN());
	for(size_t i=0; i<map.getDataSize(); ++i)
		new_map->getData()[i] = (uint8)(map.getData()[i] / 256);
	return new_map;
}


Reference<TextureData> TextureProcessing::buildTextureData(const Map2D* map, glare::Allocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps)
{
	if(dynamic_cast<const ImageMapUInt8*>(map))
	{
		const ImageMapUInt8* imagemap = static_cast<const ImageMapUInt8*>(map);

		return buildUInt8MapTextureData(imagemap, general_mem_allocator, task_manager, allow_compression, build_mipmaps);
	}
	else if(dynamic_cast<const ImageMapSequenceUInt8*>(map))
	{
		const ImageMapSequenceUInt8* imagemapseq = static_cast<const ImageMapSequenceUInt8*>(map);

		return buildUInt8MapSequenceTextureData(imagemapseq, general_mem_allocator, task_manager, allow_compression, build_mipmaps);
	}
	else if(dynamic_cast<const ImageMap<uint16, UInt16ComponentValueTraits>*>(map))
	{
		// Convert to 8-bit
		Reference<ImageMapUInt8> im_map_uint8 = convertUInt16ToUInt8ImageMap(static_cast<const ImageMap<uint16, UInt16ComponentValueTraits>&>(*map));

		return buildUInt8MapTextureData(im_map_uint8.ptr(), general_mem_allocator, task_manager, allow_compression, build_mipmaps);
	}
	else if(dynamic_cast<const CompressedImage*>(map))
	{
		const CompressedImage* compressed_image = static_cast<const CompressedImage*>(map);

		if(!allow_compression)
			throw glare::Exception("Can't handle compressed image, allow_compression is false.");

		return compressed_image->texture_data;
	}
	else
	{
		//is_8_bit = false;

		Reference<const Map2D> final_map = map;

		// Convert 32-bit floating point images to half-precision floating point (16-bit) images.
		if(dynamic_cast<const ImageMapFloat*>(map))
		{
			const ImageMapFloat* const image_map_float = static_cast<const ImageMapFloat*>(map);
			Reference<ImageMap<half, HalfComponentValueTraits> > half_image = new ImageMap<half, HalfComponentValueTraits>(map->getMapWidth(), map->getMapHeight(), map->numChannels());

			const float* const src = image_map_float->getData();
			half*  const dst = half_image->getData();
			const size_t data_size = image_map_float->numPixels() * map->numChannels();
			for(size_t i=0; i<data_size; ++i)
				dst[i] = half(src[i]);

			final_map = half_image;
		}

		Reference<TextureData> texture_data = new TextureData();
		texture_data->frames.resize(1);
		texture_data->frames[0].converted_image = final_map;
		texture_data->W = final_map->getMapWidth();
		texture_data->H = final_map->getMapHeight();
		texture_data->format = (final_map->numChannels() == 3) ? OpenGLTextureFormat::Format_RGB_Linear_Half : OpenGLTextureFormat::Format_RGBA_Linear_Half;
		return texture_data;
	}
}


Reference<TextureData> TextureProcessing::buildUInt8MapTextureData(const ImageMapUInt8* imagemap, glare::Allocator* general_mem_allocator, 
	glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps)
{
	if(imagemap->getWidth() == 0 || imagemap->getHeight() == 0 || imagemap->getN() == 0)
		throw glare::Exception("zero sized image not allowed.");

	// conPrint("Creating new OpenGL texture.");
	Reference<TextureData> texture_data = new TextureData();
	texture_data->frames.resize(1);
	if(general_mem_allocator) 
		texture_data->frames[0].mipmap_data.setAllocator(general_mem_allocator);

	// If we have a 1 or 2 bytes per pixel texture, convert to 3 or 4.
	// Handling such textures without converting them here would have to be done in the shaders, which we don't do currently.
	Reference<const ImageMapUInt8> converted_image;
	if(imagemap->getN() == 1)
	{
		// Convert to RGB:
		ImageMapUInt8Ref new_image = new ImageMapUInt8(imagemap->getWidth(), imagemap->getHeight(), 3);
		const uint8* const src  = imagemap->getData();
		uint8* const dest = new_image->getData();
		const size_t num_pixels = imagemap->numPixels();
		for(size_t i=0; i<num_pixels; ++i)
		{
			const uint8 r = src[i];
			dest[i*3 + 0] = r;
			dest[i*3 + 1] = r;
			dest[i*3 + 2] = r;
		}
		converted_image = new_image;
	}
	else if(imagemap->getN() == 2)
	{
		// Convert to RGBA:
		ImageMapUInt8Ref new_image = new ImageMapUInt8(imagemap->getWidth(), imagemap->getHeight(), 4);
		const uint8* const src  = imagemap->getData();
		uint8* const dest = new_image->getData();
		const size_t num_pixels = imagemap->numPixels();
		for(size_t i=0; i<num_pixels; ++i)
		{
			const uint8 r = src[i*2 + 0];
			const uint8 a = src[i*2 + 1];
			dest[i*4 + 0] = r;
			dest[i*4 + 1] = r;
			dest[i*4 + 2] = r;
			dest[i*4 + 3] = a;
		}
		converted_image = new_image;
	}
	else
		converted_image = imagemap;

	assert(converted_image->getN() >= 3);
	if(converted_image->getN() != 3 && converted_image->getN() != 4)
		throw glare::Exception("Texture has unhandled number of components: " + toString(converted_image->getN()));

	// Try and load as a DXT texture compression
	const size_t W = converted_image->getWidth();
	const size_t H = converted_image->getHeight();
	const bool is_one_dim_col_lookup_tex = (W == 1) || (H == 1); // Special case for palette textures: don't compress, since we can lose colours that way.
	texture_data->W = W;
	texture_data->H = H;
	if(build_mipmaps)
	{
		const bool do_compression = allow_compression && !is_one_dim_col_lookup_tex;
		if(do_compression)
			texture_data->format = (converted_image->getN() == 3) ? OpenGLTextureFormat::Format_Compressed_SRGB_Uint8 : OpenGLTextureFormat::Format_Compressed_SRGBA_Uint8;
		else
			texture_data->format = (converted_image->getN() == 3) ? OpenGLTextureFormat::Format_SRGB_Uint8 : OpenGLTextureFormat::Format_SRGBA_Uint8;

		size_t total_compressed_size, temp_tex_buf_a_size, temp_tex_buf_b_size;
		computeMipLevelOffsets(texture_data.ptr(), do_compression, total_compressed_size, temp_tex_buf_a_size, temp_tex_buf_b_size);

		js::Vector<uint8, 16> temp_tex_buf_a(temp_tex_buf_a_size);
		js::Vector<uint8, 16> temp_tex_buf_b(temp_tex_buf_b_size);
		DXTCompression::TempData compress_temp_data;

		// Stores possibly-DXT compressed image data in texture_data->frames[cur_frame_i].mipmap_data.
		buildMipMapDataForImageFrame(total_compressed_size, do_compression, temp_tex_buf_a, temp_tex_buf_b, compress_temp_data, texture_data.ptr(), /*cur frame i=*/0, /*source image=*/converted_image.ptr(), task_manager);
	}
	else
	{
		texture_data->frames[0].converted_image = converted_image;

		texture_data->format = (converted_image->getN() == 3) ? OpenGLTextureFormat::Format_SRGB_Uint8 : OpenGLTextureFormat::Format_SRGBA_Uint8;
	}

	return texture_data;
}


Reference<TextureData> TextureProcessing::buildUInt8MapSequenceTextureData(const ImageMapSequenceUInt8* seq, 
	glare::Allocator* general_mem_allocator, glare::TaskManager* task_manager, bool allow_compression, bool build_mipmaps)
{
	if(seq->images.empty())
		throw glare::Exception("empty image sequence");

	if(seq->images[0]->getWidth() == 0 || seq->images[0]->getHeight() == 0 || seq->images[0]->getN() == 0)
		throw glare::Exception("zero sized image not allowed.");

	Reference<TextureData> texture_data = new TextureData();
	texture_data->frames.resize(seq->images.size());
	if(general_mem_allocator)
		for(size_t i=0; i<texture_data->frames.size(); ++i)
			texture_data->frames[i].mipmap_data.setAllocator(general_mem_allocator);

	const ImageMapUInt8* imagemap_0 = seq->images[0].ptr();

	// Try and load as a DXT texture compression
	const size_t W = imagemap_0->getWidth();
	const size_t H = imagemap_0->getHeight();
	texture_data->W = W;
	texture_data->H = H;
	const bool do_compression = allow_compression;

	OpenGLTextureFormat format;
	if(do_compression)
		format = (imagemap_0->getN() == 3) ? OpenGLTextureFormat::Format_Compressed_SRGB_Uint8 : OpenGLTextureFormat::Format_Compressed_SRGBA_Uint8;
	else
		format = (imagemap_0->getN() == 3) ? OpenGLTextureFormat::Format_SRGB_Uint8 : OpenGLTextureFormat::Format_SRGBA_Uint8;
	texture_data->format = format;

	size_t total_compressed_size, temp_tex_buf_a_size, temp_tex_buf_b_size;
	computeMipLevelOffsets(texture_data.ptr(), do_compression, total_compressed_size, temp_tex_buf_a_size, temp_tex_buf_b_size);

	js::Vector<uint8, 16> temp_tex_buf_a(temp_tex_buf_a_size);
	js::Vector<uint8, 16> temp_tex_buf_b(temp_tex_buf_b_size);
	DXTCompression::TempData compress_temp_data;

	for(size_t frame_i = 0; frame_i != texture_data->frames.size(); ++frame_i)
	{
		const ImageMapUInt8* imagemap = seq->images[frame_i].ptr();

		if(imagemap->getN() != 3 && imagemap->getN() != 4)
			throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

		if(build_mipmaps)
		{
			buildMipMapDataForImageFrame(total_compressed_size, do_compression, temp_tex_buf_a, temp_tex_buf_b, compress_temp_data, texture_data.ptr(), /*cur frame i=*/frame_i, /*source image=*/imagemap, task_manager);
		}
		else
		{
			if(imagemap->getN() != 3 && imagemap->getN() != 4)
				throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));

			texture_data->frames[frame_i].converted_image = seq->images[frame_i];
		}
	}

	const std::vector<double>& frame_durations = seq->frame_durations;

	// Build frame_end_times
	texture_data->frame_end_times.resize(frame_durations.size());
	for(size_t i=0; i<frame_durations.size(); ++i)
		texture_data->frame_end_times[i] = seq->frame_start_times[i] + frame_durations[i];

	texture_data->last_frame_end_time = texture_data->frame_end_times.back();
	texture_data->num_frames = texture_data->frame_end_times.size();
	runtimeCheck(texture_data->num_frames == texture_data->frames.size());

	// See if the frame durations are equal
	texture_data->frame_durations_equal = false;

	if(frame_durations.size() >= 1)
	{
		bool is_equally_spaced = true;
		const double duration_0 = frame_durations[0];
		for(size_t i=1; i<frame_durations.size(); ++i)
		{
			if(frame_durations[i] != duration_0)
			{
				//conPrint("============frame duration not equal: duration: " + toString(frame_durations[i]) + ", duration_0: " + toString(duration_0));
				is_equally_spaced = false;
				break;
			}
		}

		if(is_equally_spaced)
		{
			//conPrint("==============Image frames durations are equal!");
			texture_data->frame_durations_equal = true;
			texture_data->recip_frame_duration = 1.0 / duration_0;
		}
	}

	return texture_data;
}
