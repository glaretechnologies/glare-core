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

=====================================================================*/
class PBOPool
{
public:
	PBOPool();
	~PBOPool();

	void init();

	// Threadsafe
	Reference<PBO> getMappedPBO(size_t size_B);

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
