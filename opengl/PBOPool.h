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


#if EMSCRIPTEN
// See https://registry.khronos.org/webgl/specs/latest/2.0/#5.14 "The MapBufferRange, FlushMappedBufferRange, and UnmapBuffer entry points are removed from the WebGL 2.0 AP"
const bool USE_MEM_MAPPING_FOR_TEXTURE_UPLOAD = false;
#else
const bool USE_MEM_MAPPING_FOR_TEXTURE_UPLOAD = true;
#endif


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
	Reference<PBO> getUnusedVBO(size_t size_B);

	// Threadsafe
	void pboBecameUnused(Reference<PBO> pbo);

	size_t getLargestPBOSize() const { return size_info.empty() ? 0 : size_info.back().size; }

private:
	struct PBOInfo
	{
		PBOInfo();
		Reference<PBO> pbo;
		bool used;
	};
	std::vector<PBOInfo> pbo_infos			GUARDED_BY(mutex);
	Mutex mutex;

	// For fast lookups of PBOs of a particular size.
	struct SizeInfo
	{
		size_t size;
		size_t offset;
	};
	std::vector<SizeInfo> size_info;
};
