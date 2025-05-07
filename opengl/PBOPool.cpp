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


PBOPool::PBOInfo::PBOInfo() : used(false) {}


PBOPool::PBOPool()
{
}


PBOPool::~PBOPool()
{
	for(size_t i=0; i<pbo_infos.size(); ++i)
	{
		if(pbo_infos[i].pbo->getMappedPtr())
			pbo_infos[i].pbo->unmap();
	}
}


void PBOPool::init()
{
	// total size: 1 MB
	for(int i=0; i<16; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(64 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	// total size: 16 MB
	for(int i=0; i<32; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(512 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	// total size: 8 MB
	for(int i=0; i<8; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(1024 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	for(int i=0; i<8; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(4 * 1024 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}
	
	// total size: 32 MB
	for(int i=0; i<4; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(8 * 1024 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	for(int i=0; i<2; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(16 * 1024 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	// total size: 32 MB
	for(int i=0; i<1; ++i)
	{
		PBOInfo info;
		info.pbo = new PBO(32 * 1024 * 1024);
		info.pbo->map();
		pbo_infos.push_back(info);
	}

	size_t total_size = 0;
	for(size_t i=0; i<pbo_infos.size(); ++i)
		total_size += pbo_infos[i].pbo->getSize();
	conPrint("Total PBO pool size: " + uInt32ToStringCommaSeparated((uint32)total_size) + " B");
}


PBORef PBOPool::getMappedPBO(size_t size_B)
{
	Lock lock(mutex);

	for(size_t i=0; i<pbo_infos.size(); ++i)
	{
		if(!pbo_infos[i].used && (pbo_infos[i].pbo->getSize() >= size_B))
		{
			assert(pbo_infos[i].pbo->getMappedPtr()); // Should be mapped if it's not used.

			pbo_infos[i].used = true;

			return pbo_infos[i].pbo;
		}
	}

	return nullptr;
}


void PBOPool::pboBecameUnused(Reference<PBO> pbo)
{
	Lock lock(mutex);

	for(size_t i=0; i<pbo_infos.size(); ++i)
		if(pbo_infos[i].pbo == pbo)
		{
			pbo_infos[i].used = false;

			pbo_infos[i].pbo->map();

			return;
		}

	assert(false);
}
