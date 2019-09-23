/*=====================================================================
DXTCompression.h
----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/TaskManager.h"
#include <vector>


namespace DXTCompression
{
	void init();

	size_t getCompressedSizeBytes(const ImageMapUInt8* src_imagemap);


	struct TempData
	{
		std::vector<Reference<Indigo::Task> > compress_tasks;
	};

	// Multi-thread if task_manager is non-null
	void compress(Indigo::TaskManager* task_manager, TempData& temp_data, const ImageMapUInt8* src_imagemap, uint8* compressed_data_out, size_t compressed_data_out_size);


	void test();
};

