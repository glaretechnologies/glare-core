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


void VBOPool::init()
{
	const GLenum usage = GL_STREAM_DRAW;

	for(int i=0; i<16; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 64 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<32; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 512 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<8; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 1024 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<8; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 4 * 1024 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<4; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 8 * 1024 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<1; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 16 * 1024 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	for(int i=0; i<1; ++i)
	{
		VBOInfo info;
		info.vbo = new VBO(nullptr, 32 * 1024 * 1024, GL_ARRAY_BUFFER, usage);
		info.vbo->map();
		vbo_infos.push_back(info);
	}

	size_t total_size = 0;
	for(size_t i=0; i<vbo_infos.size(); ++i)
		total_size += vbo_infos[i].vbo->getSize();
	conPrint("Total VBO pool size: " + uInt32ToStringCommaSeparated((uint32)total_size) + " B");
}


VBORef VBOPool::getMappedVBO(size_t size_B)
{
	Lock lock(mutex);

	for(size_t i=0; i<vbo_infos.size(); ++i)
	{
		if(!vbo_infos[i].used && vbo_infos[i].vbo->getSize() >= size_B)
		{
			assert(vbo_infos[i].vbo->getMappedPtr()); // Should be mapped if it's not used.

			vbo_infos[i].used = true;

			return vbo_infos[i].vbo;
		}
	}

	return nullptr;
}


void VBOPool::vboBecameUnused(const Reference<VBO>& vbo)
{
	Lock lock(mutex);

	for(size_t i=0; i<vbo_infos.size(); ++i)
		if(vbo_infos[i].vbo == vbo)
		{
			vbo_infos[i].used = false;

			vbo_infos[i].vbo->map();
			vbo_infos[i].vbo->unbind();

			return;
		}

	assert(false);
}
