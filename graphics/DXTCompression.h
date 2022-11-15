/*=====================================================================
DXTCompression.h
----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "../utils/Task.h"
#include <vector>
namespace glare { class TaskManager; }


namespace DXTCompression
{
	size_t getCompressedSizeBytes(size_t w, size_t h, size_t bytes_pp);

	struct TempData
	{
		std::vector<Reference<glare::Task> > compress_tasks;
	};

	// Multi-thread if task_manager is non-null
	void compress(glare::TaskManager* task_manager, TempData& temp_data, size_t src_W, size_t src_H, size_t src_bytes_pp, const uint8* src_image_data, 
		uint8* compressed_data_out, size_t compressed_data_out_size);


	void test();
};

