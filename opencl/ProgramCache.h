/*=====================================================================
ProgramCache.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-10-14 15:08:16 +0100
=====================================================================*/
#pragma once


#include "OpenCLProgram.h"
#include "OpenCLContext.h"
#include "OpenCL.h"
#include <vector>


/*=====================================================================
ProgramCache
-------------------
Caches OpenCL program binaries on disk.
Especially useful for AMD drivers which don't seem to do caching themselves.
=====================================================================*/
class ProgramCache
{
public:
	// Caches based on a key made from program_source and compile_options.
	static OpenCLProgramRef getOrBuildProgram(
		const std::string cachedir_path,
		const std::string& program_source,
		OpenCLContextRef opencl_context,
		const std::vector<OpenCLDevice>& devices, // all devices must share the same platform
		const std::string& compile_options,
		std::string& build_log_out
	);
};
