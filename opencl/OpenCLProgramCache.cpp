/*=====================================================================
OpenCLProgramCache.cpp
----------------------
Copyright Glare Technologies Limited 2018 -
Generated at 2016-10-14 15:08:16 +0100
=====================================================================*/
#include "OpenCLProgramCache.h"


#include "../utils/StringUtils.h"
#include "../utils/MemMappedFile.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../utils/IncludeXXHash.h"
#include "../utils/Lock.h"
#include "../utils/Mutex.h"


static const bool DO_CACHING = true; // Can disable caching for debugging with this.
static const int CACHE_EPOCH = 4; // This can be incremented to effectively invalidate the cache, since keys will change.
static const bool VERBOSE = false;


// Computes hash over program source and compilation options, which we will use as the cache key.
// Hash over platform version so that a new driver version will effectively invalidate existing cached programs.
static uint64 computeProgramHashCode(const std::string& program_source, const std::string& compile_options, const std::string& platform_version)
{
	XXH64_state_t hash_state;
	XXH64_reset(&hash_state, 1);
	XXH64_update(&hash_state, (void*)program_source.data(), program_source.size());
	XXH64_update(&hash_state, (void*)compile_options.data(), compile_options.size());
	XXH64_update(&hash_state, (void*)platform_version.data(), platform_version.size());
	XXH64_update(&hash_state, (void*)&CACHE_EPOCH, sizeof(CACHE_EPOCH));
	return XXH64_digest(&hash_state);
}


bool OpenCLProgramCache::isProgramInCache(
		const std::string cachedir_path,
		const std::string& program_source,
		const std::vector<OpenCLDeviceRef>& selected_devices_on_plat, // all devices must share the same platform
		const std::string& compile_options
)
{
	if(!DO_CACHING)
		return false;

	const OpenCLPlatform* platform = selected_devices_on_plat[0]->platform;

	const uint64 hashcode = computeProgramHashCode(program_source, compile_options, platform->version);

	{
		Lock lock(mem_cache_mutex);
		if(mem_cache.count(hashcode) != 0)
			return true;
	}

	std::vector<OpenCLDeviceRef> devices; // Devices to build program for.

	// If we're on macOS, we need to build the program for all devices, otherwise clGetProgramInfo will crash.
#ifdef OSX
	devices = platform->platform_devices;
#else
	devices = selected_devices_on_plat;
#endif

	try
	{
		// Load binaries from disk
		for(size_t i=0; i<devices.size(); ++i)
		{
			const OpenCLDevice& device = *devices[i];
			const std::string device_string_id = device.vendor_name + "_" + device.device_name;
			const uint64 device_key = XXH64(device_string_id.data(), device_string_id.size(), 1);
			const uint64 dir_bits = hashcode >> 58; // 6 bits for the dirs => 64 subdirs in program_cache.
			const std::string dir = ::toHexString(dir_bits);
			const std::string cachefile_path = cachedir_path + "/program_cache/" + dir + "/" + toHexString(hashcode) + "_" + toHexString(device_key);

			// If the binary is not present in the cache, don't throw an exception then print a warning message.
			if(!FileUtils::fileExists(cachefile_path))
				return false;

			// try and load it
			MemMappedFile file(cachefile_path);
			if(file.fileSize() == 0)
				throw glare::Exception("cached binary had size 0.");
		}

		return true;
	}
	catch(glare::Exception&)
	{
		return false;
	}
}


OpenCLProgramCache::Results OpenCLProgramCache::getOrBuildProgram(
		const std::string cachedir_path,
		const std::string& program_source,
		OpenCLContextRef& opencl_context,
		const std::vector<OpenCLDeviceRef>& selected_devices_on_plat, // all devices must share the same platform
		const std::string& compile_options,
		std::string& build_log_out
	)
{
	// Compute hash over program source and compilation options, which we will use as the cache key.
	const OpenCLPlatform* platform = selected_devices_on_plat[0]->platform;

	const uint64 hashcode = computeProgramHashCode(program_source, compile_options, platform->version);

	// Check in-memory cache to see we have this program in mem:
	if(DO_CACHING)
	{
		Lock lock(mem_cache_mutex);
		auto it = mem_cache.find(hashcode);
		if(it != mem_cache.end())
		{
			if(VERBOSE) conPrint("Found OpenCL program in mem cache.");
			return OpenCLProgramCache::Results(it->second, /*cache_hit=*/true);
		}
	}

#ifdef OSX // MacOS Sierra is currently crashing on clGetProgramInfo, so don't do on-disk caching for now on Mac.
	// The workaround to build all devices has the drawback of making build times too long in some cases, so don't do that for now either.

	OpenCLProgramRef program = ::getGlobalOpenCL()->buildProgram(
		program_source,
		opencl_context,
		selected_devices_on_plat,
		compile_options,
		build_log_out
	);

	// Add to mem-cache
	Lock lock(mem_cache_mutex);
	mem_cache[hashcode] = program;

	return OpenCLProgramCache::Results(program, /*cache_hit=*/false);

#else // else if not OS X

	std::vector<OpenCLDeviceRef> devices; // Devices to build program for.

	// If we're on macOS, we need to build the program for all devices, otherwise clGetProgramInfo will crash.
#ifdef OSX
	devices = platform->platform_devices;
#else
	devices = selected_devices_on_plat;
#endif

	std::vector<cl_device_id> device_ids(devices.size()); // Get device ids in a packed array
	for(size_t i=0; i<devices.size(); ++i)
		device_ids[i] = devices[i]->opencl_device_id;

	OpenCL* open_cl = ::getGlobalOpenCL();

	if(DO_CACHING)
	{
		try
		{
			std::vector<std::vector<uint8> > binaries;
			binaries.resize(devices.size());

			// Load binaries from disk
			for(size_t i=0; i<devices.size(); ++i)
			{
				const OpenCLDevice& device = *devices[i];
				const std::string device_string_id = device.vendor_name + "_" + device.device_name;
				const uint64 device_key = XXH64(device_string_id.data(), device_string_id.size(), 1);
				const uint64 dir_bits = hashcode >> 58; // 6 bits for the dirs => 64 subdirs in program_cache.
				const std::string dir = ::toHexString(dir_bits);
				const std::string cachefile_path = cachedir_path + "/program_cache/" + dir + "/" + toHexString(hashcode) + "_" + toHexString(device_key);

				// If the binary is not present in the cache, don't throw an exception then print a warning message.
				if(!FileUtils::fileExists(cachefile_path))
				{
					if(VERBOSE) conPrint("Cache miss: " + cachefile_path + " was not in disk cache.");
					goto build_program;
				}

				// try and load it
				MemMappedFile file(cachefile_path);
				if(file.fileSize() == 0)
					throw glare::Exception("cached binary had size 0.");

				if(VERBOSE) conPrint("Cache hit for device: " + cachefile_path + " was in disk cache.");

				binaries[i].resize(file.fileSize());
				std::memcpy(binaries[i].data(), file.fileData(), file.fileSize());
			}

			if(VERBOSE) conPrint("Cache hit for all devices!");

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
				),
				opencl_context
			);
			if(result != CL_SUCCESS)
				throw glare::Exception("clCreateProgramWithSource failed: " + OpenCL::errorString(result));

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
				throw glare::Exception("clBuildProgram failed: " + OpenCL::errorString(result));

			// Add to mem-cache
			Lock lock(mem_cache_mutex);
			mem_cache[hashcode] = program;

			// Program was successfully loaded and built from cache, so return it
			return OpenCLProgramCache::Results(program, /*cache_hit=*/true);
		}
		catch(glare::Exception& e)
		{
			// Cache failed.
			conPrint("Warning: failed building OpenCL program from cache: " + e.what());
		}
	}

build_program:
	// Program binary was not in cache, or building program from binaries failed.  So (re)build program.
	OpenCLProgramRef program = open_cl->buildProgram(
		program_source,
		opencl_context,
		devices,
		compile_options,
		build_log_out
	);


	// Get the program binaries and write them to cache
	if(DO_CACHING)
	{
		try
		{
			// Get binaries for program.  Note that the associated devices can differ in order from the device list we supplied when building the program!
			std::vector<OpenCLProgram::Binary> binaries;
			program->getProgramBinaries(binaries);

			// Save binaries to disk
			for(size_t i=0; i<binaries.size(); ++i)
			{
				if(binaries[i].data.size() > 0) // If a binary is available for the device:
				{
					// Find the device in our device list that this corresponds to
					size_t device_index = std::numeric_limits<size_t>::max();
					for(size_t z=0; z<devices.size(); ++z)
						if(devices[z]->opencl_device_id == binaries[i].device_id)
							device_index = z;

					if(device_index == std::numeric_limits<size_t>::max())
						throw glare::Exception("Failed to find device.");

					const OpenCLDevice& device = *devices[device_index];
					const std::string device_string_id = device.vendor_name + "_" + device.device_name;
					const uint64 device_key = XXH64(device_string_id.data(), device_string_id.size(), 1);
					const uint64 dir_bits = hashcode >> 58; // 6 bits for the dirs => 64 subdirs in program_cache.
					const std::string dir = ::toHexString(dir_bits);
					const std::string cachefile_path = cachedir_path + "/program_cache/" + dir + "/" + toHexString(hashcode) + "_" + toHexString(device_key);

					FileUtils::createDirIfDoesNotExist(cachedir_path + "/program_cache");
					FileUtils::createDirIfDoesNotExist(cachedir_path + "/program_cache/" + dir);
					FileUtils::writeEntireFileAtomically(cachefile_path, (const char*)binaries[i].data.data(), binaries[i].data.size());
				}
			}
		}
		catch(glare::Exception& e)
		{
			conPrint("Warning: failed saving OpenCL binaries to cache: " + e.what());
		}

		// Add to mem-cache
		Lock lock(mem_cache_mutex);
		mem_cache[hashcode] = program;
	}

	return OpenCLProgramCache::Results(program, /*cache_hit=*/false);

#endif // End else if not OS X
}
