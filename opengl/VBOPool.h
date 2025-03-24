/*=====================================================================
VBOPool.h
---------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Mutex.h>
#include <vector>
class VBO;


/*=====================================================================
VBOPool
-------

=====================================================================*/
class VBOPool
{
public:
	VBOPool();
	~VBOPool();

	void init();

	// Threadsafe
	Reference<VBO> getMappedVBO(size_t size_B);

	// Threadsafe
	void vboBecameUnused(const Reference<VBO>& vbo);

	struct VBOInfo
	{
		VBOInfo() : used(false) {}
		Reference<VBO> vbo;
		bool used;
	};
	std::vector<VBOInfo> vbo_infos			GUARDED_BY(mutex);
	Mutex mutex;
};
