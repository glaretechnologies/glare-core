/*=====================================================================
VBOPool.h
---------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include <utils/Reference.h>
#include <utils/Mutex.h>
#include <vector>
class VBO;


/*=====================================================================
VBOPool
-------
For asynchronous mesh geometry uploads.
=====================================================================*/
class VBOPool
{
public:
	VBOPool();
	~VBOPool();

	void init(GLenum buffer_type);

	// Threadsafe
	Reference<VBO> getUnusedVBO(size_t size_B);

	// Threadsafe
	void vboBecameUnused(const Reference<VBO>& vbo);

	size_t getLargestVBOSize() const { return size_info.empty() ? 0 : size_info.back().size; }

	
	struct VBOInfo
	{
		VBOInfo() : used(false) {}
		Reference<VBO> vbo;
		bool used;
	};
	std::vector<VBOInfo> vbo_infos			GUARDED_BY(mutex);
	Mutex mutex;

	// For fast lookups of PBOs of a particular size.
	struct SizeInfo
	{
		size_t size;
		size_t offset;
	};
	std::vector<SizeInfo> size_info;
};
