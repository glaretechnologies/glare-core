/*=====================================================================
PBOPool.cpp
-----------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "PBOPool.h"


#include "PBO.h"
#include <utils/Lock.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#include <utils/RuntimeCheck.h>
#include <tracy/Tracy.hpp>


PBOPool::PBOInfo::PBOInfo() : used(false) {}


PBOPool::PBOPool()
{
	const bool create_persistent_buffer = false;

	// total size: 1 MB
	size_info.push_back(SizeInfo({64 * 1024, pbo_infos.size()}));
	for(int i=0; i<16; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(64 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	// total size: 16 MB
	size_info.push_back(SizeInfo({512 * 1024, pbo_infos.size()}));
	for(int i=0; i<32; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(512 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	// total size: 8 MB
	size_info.push_back(SizeInfo({1024 * 1024, pbo_infos.size()}));
	for(int i=0; i<8; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(1024 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	size_info.push_back(SizeInfo({4 * 1024 * 1024, pbo_infos.size()}));
	for(int i=0; i<8; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(4 * 1024 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}
	
	// total size: 32 MB
	size_info.push_back(SizeInfo({8 * 1024 * 1024, pbo_infos.size()}));
	for(int i=0; i<4; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(8 * 1024 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	size_info.push_back(SizeInfo({16 * 1024 * 1024, pbo_infos.size()}));
	for(int i=0; i<2; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(16 * 1024 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	size_info.push_back(SizeInfo({32 * 1024 * 1024, pbo_infos.size()}));
	for(int i=0; i<1; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(32 * 1024 * 1024, /*for upload=*/true, create_persistent_buffer);
		pbo_infos.push_back(info);
	}

	size_t total_size = 0;
	for(size_t i=0; i<pbo_infos.size(); ++i)
	{
		pbo_infos[i].pbo->pool_index = i;

		total_size += pbo_infos[i].pbo->getSize();
	}
	conPrint("Total PBO pool size: " + uInt32ToStringCommaSeparated((uint32)total_size) + " B");
}


PBOPool::~PBOPool()
{
	for(size_t i=0; i<pbo_infos.size(); ++i)
	{
		if(pbo_infos[i].pbo->getMappedPtr())
			pbo_infos[i].pbo->unmap();
	}
}


PBORef PBOPool::getUnusedVBO(size_t size_B)
{
	Lock lock(mutex);

	// Work out start index
	size_t start_index = pbo_infos.size();
	for(size_t i=0; i<size_info.size(); ++i)
		if(size_info[i].size >= size_B)
		{
			start_index = size_info[i].offset;
			break;
		}

	for(size_t i=start_index; i<pbo_infos.size(); ++i)
	{
		assert(pbo_infos[i].pbo->getSize() >= size_B);
		if(!pbo_infos[i].used)
		{
			pbo_infos[i].used = true;

			return pbo_infos[i].pbo;
		}
	}

	return nullptr;
}


void PBOPool::pboBecameUnused(Reference<PBO> pbo)
{
	ZoneScoped; // Tracy profiler

	Lock lock(mutex);

	runtimeCheck(pbo->pool_index < pbo_infos.size());

	pbo_infos[pbo->pool_index].used = false;
}
