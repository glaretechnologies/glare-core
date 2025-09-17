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


VBOPool::VBOPool()
{
}


VBOPool::~VBOPool()
{
	for(size_t i=0; i<vbo_infos.size(); ++i)
	{
		if(vbo_infos[i].vbo->getMappedPtr())
			vbo_infos[i].vbo->unmap();
	}
}


void VBOPool::init(GLenum buffer_type)
{
	Lock lock(mutex);

	const GLenum usage = GL_STREAM_DRAW;

	const bool create_persistent_buffer = false;

	size_info.push_back(SizeInfo({64 * 1024, vbo_infos.size()}));
	for(int i=0; i<16; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 64 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({512 * 1024, vbo_infos.size()}));
	for(int i=0; i<32; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 512 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({1024 * 1024, vbo_infos.size()}));
	for(int i=0; i<8; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 1024 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({4 * 1024 * 1024, vbo_infos.size()}));
	for(int i=0; i<8; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 4 * 1024 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({8 * 1024 * 1024, vbo_infos.size()}));
	for(int i=0; i<4; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 8 * 1024 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({16 * 1024 * 1024, vbo_infos.size()}));
	for(int i=0; i<1; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 16 * 1024 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_info.push_back(SizeInfo({32 * 1024 * 1024, vbo_infos.size()}));
	for(int i=0; i<1; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 32 * 1024 * 1024, buffer_type, usage, create_persistent_buffer);
		vbo_infos.push_back(info);
	}

	size_t total_size = 0;
	for(size_t i=0; i<vbo_infos.size(); ++i)
	{
		vbo_infos[i].vbo->pool_index = i;

		total_size += vbo_infos[i].vbo->getSize();
	}
	conPrint("Total VBO pool size: " + uInt32ToStringCommaSeparated((uint32)total_size) + " B");
}


VBORef VBOPool::getUnusedVBO(size_t size_B)
{
	Lock lock(mutex);

	// Work out start index
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
