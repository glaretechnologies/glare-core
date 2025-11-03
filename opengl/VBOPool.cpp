/*=====================================================================
VBOPool.cpp
-----------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "VBOPool.h"


#include "VBO.h"
#include "IncludeOpenGL.h"
#include <utils/Lock.h>
#include <utils/StringUtils.h>
#include <utils/ConPrint.h>
#include <utils/RuntimeCheck.h>
#include <limits>


VBOPool::VBOPool(GLenum buffer_type)
{
	const GLenum usage = GL_STREAM_DRAW;

	const bool create_persistently_mapped_buffer = false;

	for(int i=0; i<4; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 64 * 1024, buffer_type, usage, create_persistently_mapped_buffer);
		vbo_infos.push_back(info);
	}
	
	for(int i=0; i<4; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 1024 * 1024, buffer_type, usage, create_persistently_mapped_buffer);
		vbo_infos.push_back(info);
	}

	for(int i=0; i<1; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 32 * 1024 * 1024, buffer_type, usage, create_persistently_mapped_buffer);
		vbo_infos.push_back(info);
	}

	size_t prev_VBO_size = std::numeric_limits<size_t>::max();
	size_t total_size = 0;
	for(size_t i=0; i<vbo_infos.size(); ++i)
	{
		vbo_infos[i].vbo->pool_index = i;

		total_size += vbo_infos[i].vbo->getSize();

		if(vbo_infos[i].vbo->getSize() != prev_VBO_size)
			size_info.push_back(SizeInfo({vbo_infos[i].vbo->getSize(), /*offset=*/i}));
		prev_VBO_size = vbo_infos[i].vbo->getSize();
	}
	conPrint("Total VBO pool size: " + uInt32ToStringCommaSeparated((uint32)total_size) + " B");
}


VBOPool::~VBOPool()
{
	for(size_t i=0; i<vbo_infos.size(); ++i)
	{
		if(vbo_infos[i].vbo->getMappedPtr())
			vbo_infos[i].vbo->unmap();
	}
}


VBORef VBOPool::getUnusedVBO(size_t size_B)
{
	Lock lock(mutex);

	// Work out start index: where we can start searching in vbo_infos for a VBO of sufficient size.
	size_t start_index = vbo_infos.size();
	for(size_t i=0; i<size_info.size(); ++i)
		if(size_info[i].size >= size_B)
		{
			start_index = size_info[i].offset;
			break;
		}

	for(size_t i=start_index; i<vbo_infos.size(); ++i)
	{
		assert(vbo_infos[i].vbo->getSize() >= size_B);
		if(!vbo_infos[i].used)
		{
			vbo_infos[i].used = true;

			return vbo_infos[i].vbo;
		}
	}

	return nullptr;
}


void VBOPool::vboBecameUnused(const Reference<VBO>& vbo)
{
	Lock lock(mutex);

	runtimeCheck(vbo->pool_index < vbo_infos.size());

	vbo_infos[vbo->pool_index].used = false;
}
