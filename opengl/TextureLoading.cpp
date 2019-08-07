/*=====================================================================
TextureLoading.cpp
------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "TextureLoading.h"


#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"
#define STB_DXT_STATIC 1
#define STB_DXT_IMPLEMENTATION 1
#include "../libs/stb/stb_dxt.h"
#include <vector>


void TextureLoading::init()
{
	// NOTE: This call has to go in the same translation unit as all stb_compress_dxt_block calls because it's initing static data in a header file.
	stb__InitDXT();
}


// Downsize previous mip level image to current mip level.
// Just uses kinda crappy 2x2 pixel box filter.
Reference<ImageMapUInt8> TextureLoading::downSampleToNextMipMapLevel(const ImageMapUInt8& prev_mip_level_image, size_t level_W, size_t level_H)
{
	Timer timer;
	const size_t old_W = prev_mip_level_image.getWidth();
	const size_t old_H = prev_mip_level_image.getHeight();

	Reference<ImageMapUInt8> new_image = new ImageMapUInt8(level_W, level_H, prev_mip_level_image.getN());

	uint8* const dst_data = new_image->getData();
	const uint8* const src_data = prev_mip_level_image.getData();
	const size_t src_W = prev_mip_level_image.getWidth();
	const size_t N = prev_mip_level_image.getN();

	// Because the new width is max(1, floor(old_width/2)), we have old_width >= new_width*2 in all cases apart from when old_width = 1.
	// If old_width >= new_width*2, then 
	// old_width >= (new_width-1)*2 + 2
	// old_width > (new_width-1)*2 + 1
	// (new_width-1)*2 + 1 < old_width
	// in other words the support pixels of the box filter are guarenteed to be in range (< old_width)
	// Likewise for old height etc..
	assert((old_W == 1) || ((level_W - 1) * 2 + 1 < old_W));
	assert((old_H == 1) || ((level_H - 1) * 2 + 1 < old_H));

	assert(N == 3 || N == 4);
	if(N == 3)
	{
		if(old_W == 1)
		{
			// This is 1xN texture being downsized to 1xfloor(N/2)
			assert(level_W == 1 && old_H > 1);
			assert((level_H - 1) * 2 + 1 < old_H);

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
				dest_pixel[0] = val[0] / 2;
				dest_pixel[1] = val[1] / 2;
				dest_pixel[2] = val[2] / 2;
			}
		}
		else if(old_H == 1)
		{
			// This is Nx1 texture being downsized to floor(N/2)x1
			assert(level_H == 1 && old_W > 1);
			assert((level_W - 1) * 2 + 1 < old_W);

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
				dest_pixel[0] = val[0] / 2;
				dest_pixel[1] = val[1] / 2;
				dest_pixel[2] = val[2] / 2;
			}
		}
		else
		{
			assert(old_W >= 2 && old_H >= 2);
			assert((level_W - 1) * 2 + 1 < old_W);
			assert((level_H - 1) * 2 + 1 < old_H);

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
					dest_pixel[0] = val[0] / 4;
					dest_pixel[1] = val[1] / 4;
					dest_pixel[2] = val[2] / 4;
				}
		}
	}
	else
	{
		if(old_W == 1)
		{
			// This is 1xN texture being downsized to 1xfloor(N/2)
			assert(level_W == 1 && old_H > 1);
			assert((level_H - 1) * 2 + 1 < old_H);

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
				dest_pixel[0] = val[0] / 2;
				dest_pixel[1] = val[1] / 2;
				dest_pixel[2] = val[2] / 2;
				dest_pixel[3] = val[3] / 2;
			}
		}
		else if(old_H == 1)
		{
			// This is Nx1 texture being downsized to floor(N/2)x1
			assert(level_H == 1 && old_W > 1);
			assert((level_W - 1) * 2 + 1 < old_W);

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
				dest_pixel[0] = val[0] / 2;
				dest_pixel[1] = val[1] / 2;
				dest_pixel[2] = val[2] / 2;
				dest_pixel[3] = val[3] / 2;
			}
		}
		else
		{
			assert(old_W >= 2 && old_H >= 2);
			assert((level_W - 1) * 2 + 1 < old_W);
			assert((level_H - 1) * 2 + 1 < old_H);

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
					dest_pixel[0] = val[0] / 4;
					dest_pixel[1] = val[1] / 4;
					dest_pixel[2] = val[2] / 4;
					dest_pixel[3] = val[3] / 4;
				}
		}
	}

	//conPrint("downSampleToNextMipMapLevel took " + timer.elapsedString());
	return new_image;
}


class DXTCompressTask : public Indigo::Task
{
public:
	virtual void run(size_t thread_index)
	{
		const size_t W = imagemap->getWidth();
		const size_t H = imagemap->getHeight();
		const size_t bytes_pp = imagemap->getBytesPerPixel();
		const size_t num_blocks_x = Maths::roundedUpDivide(W, (size_t)4);
		uint8* const compressed_data = compressed;
		if(bytes_pp == 4)
		{
			size_t write_i = num_blocks_x * (begin_y / 4) * 16; // as DXT5 (with alpha) is 16 bytes / block

			// Copy to rgba block
			uint8 rgba_block[4*4*4];

			for(size_t by=begin_y; by<end_y; by += 4) // by = y coordinate of block
				for(size_t bx=0; bx<W; bx += 4)
				{
					int z = 0;
					for(size_t y=by; y<by+4; ++y)
						for(size_t x=bx; x<bx+4; ++x) // For each pixel in block:
						{
							if(x < W && y < H)
							{
								const uint8* const pixel = imagemap->getPixel(x, y);
								rgba_block[z + 0] = pixel[0];
								rgba_block[z + 1] = pixel[1];
								rgba_block[z + 2] = pixel[2];
								rgba_block[z + 3] = pixel[3];
							}
							else
							{
								rgba_block[z + 0] = 0;
								rgba_block[z + 1] = 0;
								rgba_block[z + 2] = 0;
								rgba_block[z + 3] = 0;
							}
							z += 4;
						}

					stb_compress_dxt_block(&(compressed_data[write_i]), rgba_block, /*alpha=*/1, /*mode=*/STB_DXT_HIGHQUAL);
					write_i += 16;
				}
		}
		else if(bytes_pp == 3)
		{
			size_t write_i = num_blocks_x * (begin_y / 4) * 8; // as DXT1 (no alpha) is 8 bytes / block

			// Copy to rgba block
			uint8 rgba_block[4*4*4];

			for(size_t by=begin_y; by<end_y; by += 4) // by = y coordinate of block
				for(size_t bx=0; bx<W; bx += 4)
				{
					int z = 0;
					for(size_t y=by; y<by+4; ++y)
						for(size_t x=bx; x<bx+4; ++x) // For each pixel in block:
						{
							if(x < W && y < H)
							{
								const uint8* const pixel = imagemap->getPixel(x, y);
								rgba_block[z + 0] = pixel[0];
								rgba_block[z + 1] = pixel[1];
								rgba_block[z + 2] = pixel[2];
								rgba_block[z + 3] = 0;
							}
							else
							{
								rgba_block[z + 0] = 0;
								rgba_block[z + 1] = 0;
								rgba_block[z + 2] = 0;
								rgba_block[z + 3] = 0;
							}
							z += 4;
						}

					stb_compress_dxt_block(&(compressed_data[write_i]), rgba_block, /*alpha=*/0, /*mode=*/STB_DXT_HIGHQUAL);
					write_i += 8;
				}
		}
	}
	size_t begin_y, end_y;
	uint8* compressed;
	const ImageMapUInt8* imagemap;
};


Reference<OpenGLTexture> TextureLoading::loadUInt8Map(const ImageMapUInt8* imagemap, const Reference<OpenGLEngine>& opengl_engine,
	OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping)
{
	// conPrint("Creating new OpenGL texture.");
	Reference<OpenGLTexture> opengl_tex = new OpenGLTexture();

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

	// Try and load as a DXT texture compression
	const bool compressed_sRGB_support = opengl_engine->GL_EXT_texture_sRGB_support && opengl_engine->GL_EXT_texture_compression_s3tc_support;
	const size_t W = converted_image->getWidth();
	const size_t H = converted_image->getHeight();
	const size_t bytes_pp = converted_image->getBytesPerPixel();
	if(opengl_engine->settings.compress_textures && compressed_sRGB_support && (bytes_pp == 3 || bytes_pp == 4))
	{
		if(!opengl_engine->task_manager)
			opengl_engine->task_manager = new Indigo::TaskManager();

		std::vector<Reference<Indigo::Task> >& compress_tasks = opengl_engine->compress_tasks;
		compress_tasks.resize(myMax<size_t>(1, opengl_engine->task_manager->getNumThreads()));

		for(size_t z=0; z<compress_tasks.size(); ++z)
			if(compress_tasks[z].isNull())
				compress_tasks[z] = new DXTCompressTask();

		opengl_tex->makeGLTexture(/*format=*/(bytes_pp == 3) ? OpenGLTexture::Format_Compressed_SRGB_Uint8 : OpenGLTexture::Format_Compressed_SRGBA_Uint8);

		Reference<const ImageMapUInt8> prev_mip_level_image = converted_image;

		for(size_t k=0; ; ++k) // For each mipmap level:
		{
			// See https://www.khronos.org/opengl/wiki/Texture#Mipmap_completeness
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

			// conPrint("Building mipmap level " + toString(k) + " data, dims: " + toString(level_W) + " x " + toString(level_H));

			Reference<const ImageMapUInt8> mip_level_image;
			if(k == 0)
				mip_level_image = converted_image;
			else
			{
				// Downsize previous mip level image to current mip level
				mip_level_image = downSampleToNextMipMapLevel(*prev_mip_level_image, level_W, level_H);
			}

			const size_t num_blocks_x = Maths::roundedUpDivide(level_W, (size_t)4);
			const size_t num_blocks_y = Maths::roundedUpDivide(level_H, (size_t)4);
			const size_t num_blocks = num_blocks_x * num_blocks_y;

			js::Vector<uint8, 16> compressed((bytes_pp == 3) ? (num_blocks * 8) : (num_blocks * 16)); // DXT1 is 8 bytes per 4x4 block, DXT5 (with alpha) is 16 bytes per block
			Timer timer;
			if(num_blocks < 1024)
			{
				DXTCompressTask task;
				task.compressed = compressed.data();
				task.imagemap = mip_level_image.ptr();
				task.begin_y = 0;
				task.end_y = level_H;
				task.run(0);
			}
			else
			{
				for(size_t z=0; z<compress_tasks.size(); ++z)
				{
					const size_t y_blocks_per_task = Maths::roundedUpDivide((size_t)num_blocks_y, compress_tasks.size());
					assert(y_blocks_per_task * compress_tasks.size() >= num_blocks_y);

					// Set compress_tasks fields
					assert(dynamic_cast<DXTCompressTask*>(compress_tasks[z].ptr()));
					DXTCompressTask* task = (DXTCompressTask*)compress_tasks[z].ptr();
					task->compressed = compressed.data();
					task->imagemap = mip_level_image.ptr();
					task->begin_y = (size_t)myMin((size_t)level_H, (z       * y_blocks_per_task) * 4);
					task->end_y   = (size_t)myMin((size_t)level_H, ((z + 1) * y_blocks_per_task) * 4);
					assert(task->begin_y >= 0 && task->begin_y <= H && task->end_y >= 0 && task->end_y <= H);
				}
				opengl_engine->task_manager->addTasks(compress_tasks.data(), compress_tasks.size());
				opengl_engine->task_manager->waitForTasksToComplete();
			}

			// conPrint("DXT compression took " + timer.elapsedString());

			opengl_tex->setMipMapLevelData((int)k, level_W, level_H, /*tex data=*/compressed);

			// If we have just finished computing the last mipmap level, we are done.
			if(level_W == 1 && level_H == 1)
				break;

			prev_mip_level_image = mip_level_image;
		}

		opengl_tex->setTexParams(opengl_engine, filtering, wrapping);
	}
	else // Else if not using a compressed texture format:
	{
		OpenGLTexture::Format format;
		if(converted_image->getN() == 3)
			format = OpenGLTexture::Format_SRGB_Uint8;
		else if(converted_image->getN() == 4)
			format = OpenGLTexture::Format_SRGBA_Uint8;
		else
			throw Indigo::Exception("Texture has unhandled number of components: " + toString(converted_image->getN()));

		opengl_tex->load(W, H, ArrayRef<uint8>(converted_image->getData(), converted_image->getDataSize()), opengl_engine,
			format, filtering, wrapping
		);
	}

	return opengl_tex;
}
