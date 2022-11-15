/*=====================================================================
DXTCompression.cpp
------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "DXTCompression.h"


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
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4244) // Disable warning C4244: onversion from 'int' to 'unsigned char', possible loss of data
#endif
#include "../libs/stb/stb_dxt.h"
#ifdef _WIN32
#pragma warning(pop)
#endif
#include <vector>


namespace DXTCompression
{


class DXTCompressTask : public glare::Task
{
public:
	virtual void run(size_t /*thread_index*/)
	{
		const size_t W = src_W;
		const size_t H = src_H;
		const size_t bytes_pp = src_bytes_pp;
		const size_t num_blocks_x = Maths::roundedUpDivide(W, (size_t)4);
		const uint8* const src_data = src_image_data;
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
							const size_t use_x = myMin(x, W-1); // Clamp to image width/height in order to pad edges with edge pixel data.
							const size_t use_y = myMin(y, H-1);
							assert(use_x < W && use_y < H);
							const uint8* const pixel = src_data + (use_x + W * use_y) * 4;

							// Use 32 bit reads/writes here.
							assert((uint64)pixel % 4 == 0); // Check 4-byte aligned
							assert((uint64)rgba_block % 4 == 0); // Check 4-byte aligned
							*((uint32*)(&rgba_block[z])) = *((uint32*)pixel);
							//rgba_block[z + 0] = pixel[0];
							//rgba_block[z + 1] = pixel[1];
							//rgba_block[z + 2] = pixel[2];
							//rgba_block[z + 3] = pixel[3];

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
							const size_t use_x = myMin(x, W-1); // Clamp to image width/height in order to pad edges with edge pixel data.
							const size_t use_y = myMin(y, H-1);
							assert(use_x < W && use_y < H);
							const uint8* const pixel = src_data + (use_x + W * use_y) * 3;
							rgba_block[z + 0] = pixel[0];
							rgba_block[z + 1] = pixel[1];
							rgba_block[z + 2] = pixel[2];
							rgba_block[z + 3] = 0;
							
							z += 4;
						}

					// Write out block
					/*conPrint("block:");
					for(size_t y=0; y<4; ++y)
					{
						for(size_t x=0; x<4; ++x)
						{
							conPrintStr("(" + toString(rgba_block[y*16 + x*4 + 0]) + "," + toString(rgba_block[y*16 + x*4 + 1]) + "," + toString(rgba_block[y*16 + x*4 + 2]) + ") ");
						}
						conPrint("");
					}*/
					
					stb_compress_dxt_block(&(compressed_data[write_i]), rgba_block, /*alpha=*/0, /*mode=*/STB_DXT_HIGHQUAL);
					write_i += 8;
				}
		}
	}
	size_t begin_y, end_y;
	uint8* compressed;
	size_t src_W;
	size_t src_H;
	size_t src_bytes_pp;
	const uint8* src_image_data;
};


size_t getCompressedSizeBytes(size_t W, size_t H, size_t bytes_pp)
{
	assert(bytes_pp == 3 || bytes_pp == 4);

	const size_t num_blocks_x = Maths::roundedUpDivide(W, (size_t)4);
	const size_t num_blocks_y = Maths::roundedUpDivide(H, (size_t)4);
	const size_t num_blocks = num_blocks_x * num_blocks_y;

	return (bytes_pp == 3) ? (num_blocks * 8) : (num_blocks * 16);
}


// Multi-thread if task_manager is non-null
void compress(glare::TaskManager* task_manager, TempData& temp_data, size_t src_W, size_t src_H, size_t src_bytes_pp, const uint8* src_image_data, uint8* compressed_data_out, size_t compressed_data_out_size)
{
	// Try and load as a DXT texture compression
	const size_t W = src_W;
	const size_t H = src_H;
	assert(src_bytes_pp == 3 || src_bytes_pp == 4);

	const size_t num_blocks_x = Maths::roundedUpDivide(W, (size_t)4);
	const size_t num_blocks_y = Maths::roundedUpDivide(H, (size_t)4);
	const size_t num_blocks = num_blocks_x * num_blocks_y;

	assert(compressed_data_out_size >= /*compressed_data_size=*/((src_bytes_pp == 3) ? (num_blocks * 8) : (num_blocks * 16)));

	// Timer timer;
	if(!task_manager || (num_blocks < 1024))
	{
		DXTCompressTask task;
		task.compressed = compressed_data_out;
		task.src_W = src_W;
		task.src_H = src_H;
		task.src_bytes_pp = src_bytes_pp;
		task.src_image_data = src_image_data;
		task.begin_y = 0;
		task.end_y = H;
		task.run(0);
	}
	else
	{
		std::vector<Reference<glare::Task> >& compress_tasks = temp_data.compress_tasks;
		compress_tasks.resize(myMax<size_t>(1, task_manager->getNumThreads()));

		for(size_t z=0; z<compress_tasks.size(); ++z)
			if(compress_tasks[z].isNull())
				compress_tasks[z] = new DXTCompressTask();

		for(size_t z=0; z<compress_tasks.size(); ++z)
		{
			const size_t y_blocks_per_task = Maths::roundedUpDivide((size_t)num_blocks_y, compress_tasks.size());
			assert(y_blocks_per_task * compress_tasks.size() >= num_blocks_y);

			// Set compress_tasks fields
			assert(dynamic_cast<DXTCompressTask*>(compress_tasks[z].ptr()));
			DXTCompressTask* task = (DXTCompressTask*)compress_tasks[z].ptr();
			task->compressed = compressed_data_out;
			task->src_W = src_W;
			task->src_H = src_H;
			task->src_bytes_pp = src_bytes_pp;
			task->src_image_data = src_image_data;
			task->begin_y = (size_t)myMin((size_t)H, (z       * y_blocks_per_task) * 4);
			task->end_y   = (size_t)myMin((size_t)H, ((z + 1) * y_blocks_per_task) * 4);
			assert(task->begin_y >= 0 && task->begin_y <= H && task->end_y >= 0 && task->end_y <= H);
		}
		task_manager->runTasks(compress_tasks);
	}
	// conPrint("DXT compression took " + timer.elapsedString());
}


} // end namespace DXTCompression


#if BUILD_TESTS


#include "../utils/TestUtils.h"


namespace DXTCompression
{


void test()
{
	{
		const uint8 rgba_block[64] ={ 8u, 4u, 1u, 0u, 8u, 4u, 1u, 0u, 9u, 4u, 1u, 0u, 9u, 4u, 1u, 0u, 8u, 4u, 1u, 0u, 8u, 4u, 1u, 0u, 9u, 4u, 1u, 0u, 9u, 4u, 1u, 0u, 8u, 4u, 1u, 0u, 8u, 4u, 1u, 0u, 9u, 4u, 1u, 0u, 10u, 5u, 2u, 0u, 7u, 3u, 0u, 0u, 7u, 3u, 0u, 0u, 8u, 3u, 0u, 0u, 8u, 3u, 0u, 0u };
		uint8 compressed_data[8];
		stb_compress_dxt_block(compressed_data, rgba_block, /*alpha=*/0, /*mode=*/STB_DXT_HIGHQUAL);
		for(int i=0; i<4; ++i)
			testAssert(compressed_data[4 + i] == 0); // Check mask is zero.
	}

	{
		uint8 rgba_block[64];
		for(int i=0; i<64; ++i)
			rgba_block[i] = 0;

		uint8 compressed_data[8];
		stb_compress_dxt_block(compressed_data, rgba_block, /*alpha=*/0, /*mode=*/STB_DXT_HIGHQUAL);
	}
}


} // end namespace DXTCompression


#endif // BUILD_TESTS
