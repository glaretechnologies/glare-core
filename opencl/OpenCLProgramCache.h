/*=====================================================================
OpenCLProgramCache.h
--------------------
Copyright Glare Technologies Limited 2018 -
Generated at 2016-10-14 15:08:16 +0100
=====================================================================*/
#pragma once


#include "OpenCLProgram.h"
#include "OpenCLContext.h"
#include "OpenCL.h"
#include "../utils/Mutex.h"
#include "../utils/RefCounted.h"
#include <vector>
#include <map>


/*=====================================================================
OpenCLProgramCache
------------------
Caches OpenCL program binaries on disk, and in memory.
Especially useful for AMD drivers which don't seem to do caching themselves.
=====================================================================*/
class OpenCLProgramCache : public RefCounted
{
public:
	// Caches based on a key made from program_source and compile_options.
	struct Results
	{
		Results(const OpenCLProgramRef& program_, bool cache_hit_) : program(program_), cache_hit(cache_hit_) {}
		OpenCLProgramRef program;
		bool cache_hit;
	};

	bool isProgramInCache(
		const std::string cachedir_path,
		const std::string& program_source,
		const std::vector<OpenCLDeviceRef>& devices, // all devices must share the same platform
		const std::string& compile_options
	);

	Results getOrBuildProgram(
		const std::string cachedir_path,
		const std::string& program_source,
		OpenCLContextRef opencl_context,
		const std::vector<OpenCLDeviceRef>& devices, // all devices must share the same platform
		const std::string& compile_options,
		std::string& build_log_out
	);

	Mutex mem_cache_mutex;
	std::map<uint64, OpenCLProgramRef> mem_cache;
};
