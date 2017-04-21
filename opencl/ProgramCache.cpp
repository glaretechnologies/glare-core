/*=====================================================================
ProgramCache.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-10-14 15:08:16 +0100
=====================================================================*/
#include "ProgramCache.h"


#include "../utils/StringUtils.h"
#include "../utils/MemMappedFile.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../utils/IncludeXXHash.h"


OpenCLProgramRef ProgramCache::getOrBuildProgram(
		const std::string appdata_path,
		const std::string& program_source,
		OpenCLContextRef opencl_context,
		const std::vector<OpenCLDevice>& devices, // all devices must share the same platform
		const std::string& compile_options,
		std::string& build_log_out
	)
{
#ifdef OSX // MacOS Sierra is currently crashing on clGetProgramInfo, so don't do caching for now on Mac.

	return ::getGlobalOpenCL()->buildProgram(
		program_source,
		opencl_context->getContext(),
		devices,
		compile_options,
		build_log_out
	);

#else // else if not OS X

	const bool VERBOSE = false;

	// Compute hash over program source and compilation options, which we will use as the cache key.
	XXH64_state_t hash_state;
	XXH64_reset(&hash_state, 1);
	XXH64_update(&hash_state, (void*)program_source.data(), program_source.size());
	XXH64_update(&hash_state, (void*)compile_options.data(), compile_options.size());
	const uint64 hashcode = XXH64_digest(&hash_state);


	std::vector<cl_device_id> device_ids(devices.size()); // Get device ids in a packed array
	for(size_t i=0; i<devices.size(); ++i)
		device_ids[i] = devices[i].opencl_device_id;

	std::vector<std::vector<uint8> > binaries;
	binaries.resize(devices.size());

	OpenCL* open_cl = ::getGlobalOpenCL();

	try
	{
		// Load binaries from disk
		for(size_t i=0; i<devices.size(); ++i)
		{
			const OpenCLDevice& device = devices[i];
			const std::string device_string_id = device.vendor_name + "_" + device.device_name;
			const uint64 device_key = XXH64(device_string_id.data(), device_string_id.size(), 1);
			const uint64 dir_bits = hashcode >> 58; // 6 bits for the dirs => 64 subdirs in program_cache.
			const std::string dir = ::toHexString(dir_bits);
			const std::string cachefile_path = appdata_path + "/cache/program_cache/" + dir + "/" + toHexString(hashcode) + "_" + toHexString(device_key);

			// If the binary is not present in the cache, don't throw an exception then print a warning message.
			if(!FileUtils::fileExists(cachefile_path))
			{
				if(VERBOSE) conPrint("Cache miss: " + cachefile_path + " was not in cache.");
				goto build_program;
			}

			// try and load it
			MemMappedFile file(cachefile_path);
			if(file.fileSize() == 0)
				throw Indigo::Exception("cached binary had size 0.");

			binaries[i].resize(file.fileSize());
			std::memcpy(binaries[i].data(), file.fileData(), file.fileSize());
		}

		if(VERBOSE) conPrint("Cache hit!");

		std::vector<size_t> binary_lengths(binaries.size());
		std::vector<const unsigned char*> binary_pointers(binaries.size());
		for(size_t i=0; i<binary_pointers.size(); ++i)
		{
			binary_lengths[i] = binaries[i].size();
			binary_pointers[i] = (const unsigned char*)binaries[i].data();
		}

		cl_int result = CL_SUCCESS;
		OpenCLProgramRef program = new OpenCLProgram(open_cl->clCreateProgramWithBinary(
			opencl_context->getContext(),
			(cl_uint)device_ids.size(), // num devices
			device_ids.data(), // device list
			binary_lengths.data(), // lengths
			binary_pointers.data(), // binaries
			NULL, // binary status - Individual statuses for each binary.  We won't use this.
			&result
		));
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clCreateProgramWithSource failed: " + OpenCL::errorString(result));

		Timer timer;
		result = open_cl->clBuildProgram(
			program->getProgram(),
			(cl_uint)device_ids.size(), // num devices
			device_ids.data(), // device ids
			compile_options.c_str(), // options
			NULL, // pfn_notify
			NULL // user data
		);
		if(VERBOSE) conPrint("clBuildProgram took " + timer.elapsedStringNSigFigs(4));

		if(result != CL_SUCCESS)
			throw Indigo::Exception("clBuildProgram failed: " + OpenCL::errorString(result));

		// Program was successfully loaded and built from cache, so return it
		return program;
	}
	catch(Indigo::Exception& e)
	{
		// Cache failed.
		if(VERBOSE) conPrint("Warning: failed building OpenCL program from cache: " + e.what());
	}


build_program:
	// Program binary was not in cache, or building program from binaries failed.  So (re)build program.
	OpenCLProgramRef program = open_cl->buildProgram(
		program_source,
		opencl_context->getContext(),
		devices,
		compile_options,
		build_log_out
	);


	// Get the program binaries and write to cache
	try
	{
		// Get binary sizes
		std::vector<size_t> binary_sizes(devices.size());
		cl_int result = open_cl->clGetProgramInfo(
			program->getProgram(),
			CL_PROGRAM_BINARY_SIZES,
			binary_sizes.size() * sizeof(size_t), // param value size
			binary_sizes.data(), // param value
			NULL // param_value_size_ret
		);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clGetProgramInfo failed: " + OpenCL::errorString(result));

		// Allocate space for binaries
		for(size_t i=0; i<binaries.size(); ++i)
			binaries[i].resize(binary_sizes[i]);

		std::vector<unsigned char*> binary_pointers(binaries.size());
		for(size_t i=0; i<binary_pointers.size(); ++i)
			binary_pointers[i] = (unsigned char*)binaries[i].data();

		// Get actual binaries
		result = open_cl->clGetProgramInfo(
			program->getProgram(),
			CL_PROGRAM_BINARIES,
			binary_pointers.size() * sizeof(unsigned char*), // param value size
			binary_pointers.data(), // param value
			NULL // param_value_size_ret
		);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clGetProgramInfo 2 failed: " + OpenCL::errorString(result));

		// Save binaries to disk
		for(size_t i=0; i<devices.size(); ++i)
		{
			const OpenCLDevice& device = devices[i];
			const std::string device_string_id = device.vendor_name + "_" + device.device_name;
			const uint64 device_key = XXH64(device_string_id.data(), device_string_id.size(), 1);
			const uint64 dir_bits = hashcode >> 58; // 6 bits for the dirs => 64 subdirs in program_cache.
			const std::string dir = ::toHexString(dir_bits);
			const std::string cachefile_path = appdata_path + "/cache/program_cache/" + dir + "/" + toHexString(hashcode) + "_" + toHexString(device_key);

			FileUtils::createDirIfDoesNotExist(appdata_path + "/cache");
			FileUtils::createDirIfDoesNotExist(appdata_path + "/cache/program_cache");
			FileUtils::createDirIfDoesNotExist(appdata_path + "/cache/program_cache/" + dir);
			FileUtils::writeEntireFileAtomically(cachefile_path, (const char*)binaries[i].data(), binaries[i].size());
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		conPrint("Warning: failed saving OpenCL binaries to cache: " + e.what());
	}
	catch(Indigo::Exception& e)
	{
		conPrint("Warning: failed saving OpenCL binaries to cache: " + e.what());
	}

	return program;

#endif // End else if not OS X
}
