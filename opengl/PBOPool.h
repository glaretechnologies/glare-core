/*=====================================================================
PBOPool.h
---------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Mutex.h>
#include <vector>
class PBO;


/*=====================================================================
PBOPool
-------
For asynchronous texture uploads.

Also tried using a single PBO with a BestFitAllocator, the problem 
is nvidia OpenGL drivers don't seem to signal uploads as completed
until all pending uploads from the buffer have been done.
See 'patches/using single VBO and PBO for async uploads.diff'.
=====================================================================*/
class PBOPool
{
public:
	PBOPool();
	~PBOPool();

	void init();

	// Threadsafe
	Reference<PBO> getMappedAndUnusedVBO(size_t size_B);

	// Threadsafe
	void pboBecameUnused(Reference<PBO> pbo);

private:
	struct PBOInfo
	{
		PBOInfo();
		Reference<PBO> pbo;
		bool used;
	};
	std::vector<PBOInfo> pbo_infos			GUARDED_BY(mutex);
	Mutex mutex;
};
