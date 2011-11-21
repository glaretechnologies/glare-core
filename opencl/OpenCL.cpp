/*=====================================================================
OpenCL.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCL.h"


#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__linux__)
#include <dlfcn.h>
#endif

#include <cmath>
#include <fstream>
#include <iostream>

#include "../maths/mathstypes.h"
#include "../utils/platformutils.h"
#include "../utils/stringutils.h"
#include "../utils/Exception.h"
#include "../utils/timer.h"
#include "../indigo/gpuDeviceInfo.h"


#if defined(_WIN32)
template <class FuncPointerType>
static FuncPointerType getFuncPointer(HMODULE module, const std::string& name)
{
	FuncPointerType f = (FuncPointerType)::GetProcAddress(module, name.c_str());
	if(!f)
		throw Indigo::Exception("Failed to get pointer to function '" + name + "'");
	return f;
}
#elif defined(__linux__)
template <class FuncPointerType>
static FuncPointerType getFuncPointer(void *handle, const std::string& name)
{
	FuncPointerType f = (FuncPointerType)dlsym(handle, name.c_str());
	if(!f)
		throw Indigo::Exception("Failed to get pointer to function '" + name + "'");
	return f;
}
#endif


OpenCL::OpenCL(bool verbose_)
:	initialised(false),
	verbose(verbose_),
	allow_CPU_devices(false)
{
#if USE_OPENCL

	// Initialise the OpenCL library, importing the function pointers etc.
	libraryInit();

#else
	throw Indigo::Exception("OpenCL disabled.");
#endif
}


OpenCL::~OpenCL()
{
#if USE_OPENCL
	// Free command queue
	if(this->command_queue)
	{
		if(clReleaseCommandQueue(this->command_queue) != CL_SUCCESS)
			throw Indigo::Exception("clReleaseCommandQueue failed");
	}

	// Cleanup
	if(this->context)
	{
		if(clReleaseContext(this->context) != CL_SUCCESS)
			throw Indigo::Exception("clReleaseContext failed");
	}

#if defined(_WIN32)
	if(!::FreeLibrary(opencl_handle))
		throw Indigo::Exception("FreeLibrary failed");
#endif

	//std::cout << "Shut down OpenCL." << std::endl;
#endif
}


void OpenCL::libraryInit()
{
#if USE_OPENCL
	context = 0;
	command_queue = 0;

	std::vector<std::string> opencl_paths;

#if defined(_WIN32)
	opencl_paths.push_back("OpenCL.dll");

	try // ATI/AMD OpenCL
	{
		std::string ati_sdk_root = PlatformUtils::getEnvironmentVariable("ATISTREAMSDKROOT");
	#if defined(_WIN64)
		if(verbose) std::cout << "Detected ATI 64 bit OpenCL SDK at " << ati_sdk_root << std::endl;
		opencl_paths.push_back(ati_sdk_root + "bin\\x86_64\\atiocl64.dll");
	#else
		if(verbose) std::cout << "Detected ATI 32 bit OpenCL SDK at " << ati_sdk_root << std::endl;
		opencl_paths.push_back(ati_sdk_root + "bin\\x86\\atiocl.dll");
	#endif
	}
	catch(PlatformUtils::PlatformUtilsExcep&) { } // No ATI/AMD OpenCL found

	try // Intel OpenCL
	{
		std::string intel_sdk_root = PlatformUtils::getEnvironmentVariable("INTELOCLSDKROOT");

	#if defined(_WIN64)
		if(verbose) std::cout << "Detected Intel 64 bit OpenCL SDK at " << intel_sdk_root << std::endl;
		opencl_paths.push_back(intel_sdk_root + "bin\\x64\\intelocl.dll");
	#else
		if(verbose) std::cout << "Detected Intel 64 bit OpenCL SDK at " << intel_sdk_root << std::endl;
		opencl_paths.push_back(intel_sdk_root + "bin\\x86\\intelocl.dll");
	#endif
	}
	catch(PlatformUtils::PlatformUtilsExcep&) { } // No Intel OpenCL found
#else
	opencl_paths.push_back("libOpenCL.so");
#endif


#if defined(_WIN32) || defined(__linux__)
	size_t searched_paths = 0;
	for( ; searched_paths < opencl_paths.size(); ++searched_paths)
	{
#if defined(_WIN32)
		const std::wstring path = StringUtils::UTF8ToPlatformUnicodeEncoding(opencl_paths[searched_paths]);
		opencl_handle = ::LoadLibrary(path.c_str());
		if(!opencl_handle)
		{
			const DWORD error_code = GetLastError();
			//std::cout << "Failed to load OpenCL library from '" << StringUtils::PlatformToUTF8UnicodeEncoding(path) << "', error_code: " << ::toString((uint32)error_code) << std::endl;
			continue;
		}
#elif defined(__linux__)
		opencl_handle = dlopen(opencl_paths[searched_paths].c_str(), RTLD_LAZY);
		if(!opencl_handle)
		{
			std::cout << "Failed to load OpenCL library" << std::endl;
			continue;
		}
#endif

		// Successfully loaded library, try to get required function pointers
		try
		{
			clGetPlatformIDs = getFuncPointer<clGetPlatformIDs_TYPE>(opencl_handle, "clGetPlatformIDs");
			clGetPlatformInfo = getFuncPointer<clGetPlatformInfo_TYPE>(opencl_handle, "clGetPlatformInfo");
			clGetDeviceIDs = getFuncPointer<clGetDeviceIDs_TYPE>(opencl_handle, "clGetDeviceIDs");
			clGetDeviceInfo = getFuncPointer<clGetDeviceInfo_TYPE>(opencl_handle, "clGetDeviceInfo");
			clCreateContextFromType = getFuncPointer<clCreateContextFromType_TYPE>(opencl_handle, "clCreateContextFromType");
			clReleaseContext = getFuncPointer<clReleaseContext_TYPE>(opencl_handle, "clReleaseContext");
			clCreateCommandQueue = getFuncPointer<clCreateCommandQueue_TYPE>(opencl_handle, "clCreateCommandQueue");
			clReleaseCommandQueue = getFuncPointer<clReleaseCommandQueue_TYPE>(opencl_handle, "clReleaseCommandQueue");
			clCreateBuffer = getFuncPointer<clCreateBuffer_TYPE>(opencl_handle, "clCreateBuffer");
			clCreateImage2D = getFuncPointer<clCreateImage2D_TYPE>(opencl_handle, "clCreateImage2D");
			clReleaseMemObject = getFuncPointer<clReleaseMemObject_TYPE>(opencl_handle, "clReleaseMemObject");
			clCreateProgramWithSource = getFuncPointer<clCreateProgramWithSource_TYPE>(opencl_handle, "clCreateProgramWithSource");
			clBuildProgram = getFuncPointer<clBuildProgram_TYPE>(opencl_handle, "clBuildProgram");
			clGetProgramBuildInfo = getFuncPointer<clGetProgramBuildInfo_TYPE>(opencl_handle, "clGetProgramBuildInfo");
			clCreateKernel = getFuncPointer<clCreateKernel_TYPE>(opencl_handle, "clCreateKernel");
			clSetKernelArg = getFuncPointer<clSetKernelArg_TYPE>(opencl_handle, "clSetKernelArg");
			clEnqueueWriteBuffer = getFuncPointer<clEnqueueWriteBuffer_TYPE>(opencl_handle, "clEnqueueWriteBuffer");
			clEnqueueReadBuffer = getFuncPointer<clEnqueueReadBuffer_TYPE>(opencl_handle, "clEnqueueReadBuffer");
			clEnqueueNDRangeKernel = getFuncPointer<clEnqueueNDRangeKernel_TYPE>(opencl_handle, "clEnqueueNDRangeKernel");
			clReleaseKernel = getFuncPointer<clReleaseKernel_TYPE>(opencl_handle, "clReleaseKernel");
			clReleaseProgram = getFuncPointer<clReleaseProgram_TYPE>(opencl_handle, "clReleaseProgram");
			clGetProgramInfo = getFuncPointer<clGetProgramInfo_TYPE>(opencl_handle, "clGetProgramInfo");
			clGetKernelWorkGroupInfo = getFuncPointer<clGetKernelWorkGroupInfo_TYPE>(opencl_handle, "clGetKernelWorkGroupInfo");

			clSetCommandQueueProperty = getFuncPointer<clSetCommandQueueProperty_TYPE>(opencl_handle, "clSetCommandQueueProperty");
			clGetEventProfilingInfo = getFuncPointer<clGetEventProfilingInfo_TYPE>(opencl_handle, "clGetEventProfilingInfo");
			clEnqueueMarker = getFuncPointer<clEnqueueMarker_TYPE>(opencl_handle, "clEnqueueMarker");
			clWaitForEvents = getFuncPointer<clWaitForEvents_TYPE>(opencl_handle, "clWaitForEvents");
		}
		catch(Indigo::Exception& e)
		{
			if(verbose) std::cout << "Error loading OpenCL library from " << opencl_paths[searched_paths] << ": " << e.what() << std::endl;
			continue; // try the next library
		}

		if(verbose) std::cout << "Successfully loaded OpenCL functions from " << opencl_paths[searched_paths] << std::endl;
		break; // found all req functions, break out of search loop
	}
	if(searched_paths == opencl_paths.size())
	{
		throw Indigo::Exception("Failed to find OpenCL library.");
	}
#else
	// Just get the function pointers directly for Mac OS X
	this->clGetPlatformIDs = ::clGetPlatformIDs;
	this->clGetPlatformInfo = ::clGetPlatformInfo;
	this->clGetDeviceIDs = ::clGetDeviceIDs;
	this->clGetDeviceInfo = ::clGetDeviceInfo;
	this->clCreateContextFromType = ::clCreateContextFromType;
	this->clReleaseContext = ::clReleaseContext;
	this->clCreateCommandQueue = ::clCreateCommandQueue;
	this->clReleaseCommandQueue = ::clReleaseCommandQueue;
	this->clCreateBuffer = ::clCreateBuffer;
	this->clCreateImage2D = ::clCreateImage2D;
	this->clReleaseMemObject = ::clReleaseMemObject;
	this->clCreateProgramWithSource = ::clCreateProgramWithSource;
	this->clBuildProgram = ::clBuildProgram;
	this->clGetProgramBuildInfo = ::clGetProgramBuildInfo;
	this->clCreateKernel = ::clCreateKernel;
	this->clSetKernelArg = ::clSetKernelArg;
	this->clEnqueueWriteBuffer = ::clEnqueueWriteBuffer;
	this->clEnqueueReadBuffer = ::clEnqueueReadBuffer;
	this->clEnqueueNDRangeKernel = ::clEnqueueNDRangeKernel;
	this->clReleaseKernel = ::clReleaseKernel;
	this->clReleaseProgram = ::clReleaseProgram;
	this->clGetProgramInfo = ::clGetProgramInfo;
	this->clGetKernelWorkGroupInfo = ::clGetKernelWorkGroupInfo;

	this->clSetCommandQueueProperty = ::clSetCommandQueueProperty;
	this->clGetEventProfilingInfo = ::clGetEventProfilingInfo;
	this->clEnqueueMarker = ::clEnqueueMarker;
	this->clWaitForEvents = ::clWaitForEvents;
	
	if(this->clGetPlatformIDs == NULL)
		throw Indigo::Exception("OpenCL is not available in this version of OSX.");
#endif

	initialised = true;

#else
	throw Indigo::Exception("OpenCL disabled.");
#endif
}


#if USE_OPENCL
void OpenCL::queryDevices()
{
	if(!initialised)
		throw Indigo::Exception("OpenCL library not initialised");

	// Temporary storage for the strings OpenCL returns
	std::vector<char> char_buff(128 * 1024);

	int current_device_number = 0;

	std::vector<cl_platform_id> platform_ids(128);
	cl_uint num_platforms = 0;
	if(this->clGetPlatformIDs(128, &platform_ids[0], &num_platforms) != CL_SUCCESS)
		throw Indigo::Exception("clGetPlatformIDs failed");


	if(verbose) std::cout << "Num platforms: " << num_platforms << std::endl;

	for(cl_uint i = 0; i < num_platforms; ++i)
	{
		if(verbose)
		{
			if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_PROFILE, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetPlatformInfo failed");
			const std::string platform_profile(&char_buff[0]);
			if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetPlatformInfo failed");
			const std::string platform_version(&char_buff[0]);
			if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_NAME, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetPlatformInfo failed");
			const std::string platform_name(&char_buff[0]);
			if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VENDOR, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetPlatformInfo failed");
			const std::string platform_vendor(&char_buff[0]);
			if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_EXTENSIONS, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetPlatformInfo failed");
			const std::string platform_extensions(&char_buff[0]);

			std::cout << std::endl;
			std::cout << "platform_id: " << platform_ids[i] << std::endl;
			std::cout << "platform_profile: " << platform_profile << std::endl;
			std::cout << "platform_version: " << platform_version << std::endl;
			std::cout << "platform_name: " << platform_name << std::endl;
			std::cout << "platform_vendor: " << platform_vendor << std::endl;
			std::cout << "platform_extensions: " << platform_extensions << std::endl;
		}

		std::vector<cl_device_id> device_ids(16);
		cl_uint num_devices = 0;
		if(clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, (cl_uint)device_ids.size(), &device_ids[0], &num_devices) != CL_SUCCESS)
			throw Indigo::Exception("clGetDeviceIDs failed");

		if(verbose) std::cout << num_devices << " device(s) found." << std::endl;

		for(cl_uint d = 0; d < num_devices; ++d)
		{
			if(verbose) std::cout << "----------- Device " << current_device_number << " -----------" << std::endl;

			cl_device_type device_type;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_TYPE, sizeof(device_type), &device_type, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			if(verbose)
			{
				if(device_type & CL_DEVICE_TYPE_CPU)
					std::cout << "device_type: CL_DEVICE_TYPE_CPU" << std::endl;
				if(device_type & CL_DEVICE_TYPE_GPU)
					std::cout << "device_type: CL_DEVICE_TYPE_GPU" << std::endl;
				if(device_type & CL_DEVICE_TYPE_ACCELERATOR)
					std::cout << "device_type: CL_DEVICE_TYPE_ACCELERATOR" << std::endl;
				if(device_type & CL_DEVICE_TYPE_DEFAULT)
					std::cout << "device_type: CL_DEVICE_TYPE_DEFAULT" << std::endl;
			}

			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_NAME, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");
			const std::string device_name_(&char_buff[0]);

			cl_ulong device_global_mem_size = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(device_global_mem_size), &device_global_mem_size, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			cl_uint device_max_compute_units = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(device_max_compute_units), &device_max_compute_units, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			cl_uint device_max_clock_frequency = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(device_max_clock_frequency), &device_max_clock_frequency, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			if(verbose)
			{
				if(clGetDeviceInfo(device_ids[d], CL_DRIVER_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
				const std::string driver_version(&char_buff[0]);
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_PROFILE, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
				const std::string device_profile(&char_buff[0]);
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
				const std::string device_version(&char_buff[0]);

				size_t device_max_work_group_size = 0;
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(device_max_work_group_size), &device_max_work_group_size, NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");

				cl_uint device_max_work_item_dimensions = 0;
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(device_max_work_item_dimensions), &device_max_work_item_dimensions, NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");

				std::vector<size_t> device_max_num_work_items(device_max_work_item_dimensions);
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t) * device_max_num_work_items.size(), &device_max_num_work_items[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");

				size_t device_image2d_max_width = 0;
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(device_image2d_max_width), &device_image2d_max_width, NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");

				std::cout << "device_name: " << device_name_ << std::endl;
				std::cout << "driver_version: " << driver_version << std::endl;
				std::cout << "device_profile: " << device_profile << std::endl;
				std::cout << "device_version: " << device_version << std::endl;
				std::cout << "device_max_compute_units: " << device_max_compute_units << std::endl;
				std::cout << "device_max_work_group_size: " << device_max_work_group_size << std::endl;
				std::cout << "device_max_work_item_dimensions: " << device_max_work_item_dimensions << std::endl;
				std::cout << "device_image2d_max_width: " << device_image2d_max_width << std::endl;

				for(size_t z = 0; z < device_max_num_work_items.size(); ++z)
					std::cout << "Dim " << z << " device_max_num_work_items: " << device_max_num_work_items[z] << std::endl;

				std::cout << "device_max_clock_frequency: " << device_max_clock_frequency << " MHz" << std::endl;
				std::cout << "device_global_mem_size: " << device_global_mem_size << " B" << std::endl;
			}

			// Add device info structure to the vector.
			gpuDeviceInfo di;
			di.device_number = current_device_number;
			di.device_name = device_name_;
			di.memory_size = (size_t)device_global_mem_size;
			di.core_count = device_max_compute_units;
			di.core_clock = device_max_clock_frequency; // in MHz
			di.subsystem = Indigo::GPUDeviceSettings::SUBSYSTEM_OPENCL;
			di.CPU = (device_type & CL_DEVICE_TYPE_CPU) != 0;

			di.opencl_platform = platform_ids[i];
			di.opencl_device = device_ids[d];

			device_info.push_back(di);

			++current_device_number;
		}
	}
}


void OpenCL::deviceInit(int device_number)
{
	if(!initialised)
		throw Indigo::Exception("OpenCL library not initialised");

	if(device_number < 0 || device_number >= (int)device_info.size())
		throw Indigo::Exception("Invalid OpenCL device initialisation requested.");

	chosen_device_number = device_number;

	platform_to_use = device_info[chosen_device_number].opencl_platform;
	device_to_use = device_info[chosen_device_number].opencl_device;

	if(chosen_device_number == -1)
		throw Indigo::Exception("Could not find appropriate OpenCL device.");

	cl_context_properties cps[3] =
    {
        CL_CONTEXT_PLATFORM,
		(cl_context_properties)platform_to_use,
        0
    };

	cl_int error_code;
	this->context = clCreateContextFromType(
		cps, // properties
		CL_DEVICE_TYPE_ALL, // CL_DEVICE_TYPE_CPU, // TEMP CL_DEVICE_TYPE_GPU, // device type
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);

	if(this->context == 0)
		throw Indigo::Exception("clCreateContextFromType failed: " + errorString(error_code));

	// Create command queue
	this->command_queue = this->clCreateCommandQueue(
		context,
		device_to_use,
		0, // CL_QUEUE_PROFILING_ENABLE, // queue properties
		&error_code);

	if(command_queue == 0)
		throw Indigo::Exception("clCreateCommandQueue failed");
}
#endif


int OpenCL::getSuggestedDeviceNumber(const std::string& preferred_dev_name) const
{
	int64 chosen_device_perf = -1;
	int chosen_device = -1;

	for(size_t i = 0; i < device_info.size(); ++i)
	{
		const gpuDeviceInfo& di = device_info[i];

		// If we've asked for a particular device and the name matches exactly, return its device index
		if(!preferred_dev_name.empty() && di.device_name == preferred_dev_name)
			return (int)i;

		bool device_ok = di.CPU ? allow_CPU_devices : true;

		int64 device_perf = (int64)di.core_count * (int64)di.core_clock;
		if(device_ok && device_perf > chosen_device_perf)
		{
			chosen_device_perf = device_perf;
			chosen_device = (int)i;
		}
	}

	return chosen_device;
}


int OpenCL::getChosenDeviceNumber() const
{
	return chosen_device_number;
}


#if USE_OPENCL
const std::string OpenCL::errorString(cl_int result)
{
	switch(result)
	{
		case  0: return "CL_SUCCESS";
		case -1: return "CL_DEVICE_NOT_FOUND";
		case -2: return "CL_DEVICE_NOT_AVAILABLE";
		case -3: return "CL_COMPILER_NOT_AVAILABLE";
		case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case -5: return "CL_OUT_OF_RESOURCES";
		case -6: return "CL_OUT_OF_HOST_MEMORY";
		case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
		case -8: return "CL_MEM_COPY_OVERLAP";
		case -9: return "CL_IMAGE_FORMAT_MISMATCH";
		case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case -11: return "CL_BUILD_PROGRAM_FAILURE";
		case -12: return "CL_MAP_FAILURE";
		default: return "[Unknown: " + ::toString(result) + "]";
	};
}


// Accessor method for device data queried in the constructor.
std::vector<gpuDeviceInfo> OpenCL::getDeviceInfo() const { return device_info; }


cl_program OpenCL::buildProgram(
							   const std::vector<std::string>& program_lines,
							   cl_device_id device,
							   const std::string& compile_options,
							   PrintOutput& print_output
							   )
{
	std::vector<const char*> strings(program_lines.size());
	for(size_t i=0; i<program_lines.size(); ++i)
		strings[i] = program_lines[i].c_str();

	cl_int result;
	cl_program program = this->clCreateProgramWithSource(
		this->context,
		(cl_uint)program_lines.size(),
		&strings[0],
		NULL, // lengths, can be NULL because all strings are null-terminated.
		&result
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateProgramWithSource failed: " + errorString(result));

	// Build program
	result = this->clBuildProgram(
		program,
		1, // num devices
		&device, // device ids
		compile_options.c_str(), // options
		NULL, // pfn_notify
		NULL // user data
	);

	bool build_success = (result == CL_SUCCESS);
	if(!build_success)
	{
		if(result == CL_BUILD_PROGRAM_FAILURE) // If a compile error, don't throw exception yet, print out build log first.
			dumpBuildLog(program, print_output);
		else
			throw Indigo::Exception("clBuildProgram failed: " + errorString(result));
	}


	// Get build status
	cl_build_status build_status;
	this->clGetProgramBuildInfo(
		program,
		this->device_to_use,
		CL_PROGRAM_BUILD_STATUS,
		sizeof(build_status), // param value size
		&build_status, // param value
		NULL
		);
	if(build_status != CL_BUILD_SUCCESS) // This will happen on compilation error.
		throw Indigo::Exception("Kernel build failed.");

	//================= TEMP: get program 'binary' ====================

	if(false)
	{
		// Get number of devices program has been built for
		cl_uint program_num_devices = 0;
		size_t param_value_size_ret = 0;
		this->clGetProgramInfo(
			program,
			CL_PROGRAM_NUM_DEVICES,
			sizeof(program_num_devices),
			&program_num_devices,
			&param_value_size_ret
			);

		std::cout << "program_num_devices: " << program_num_devices << std::endl;

		// Get sizes of the binaries on the devices
		std::vector<size_t> program_binary_sizes(program_num_devices);
		this->clGetProgramInfo(
			program,
			CL_PROGRAM_BINARY_SIZES,
			sizeof(size_t) * program_binary_sizes.size(), // size
			&program_binary_sizes[0],
			&param_value_size_ret
			);

		for(size_t i=0; i<program_binary_sizes.size(); ++i)
		{
			std::cout << "\tprogram " << i << " binary size: " << program_binary_sizes[i] << " B" << std::endl;
		}

		std::vector<std::string> program_binaries(program_num_devices);
		for(size_t i=0; i<program_binary_sizes.size(); ++i)
		{
			program_binaries[i].resize(program_binary_sizes[i]);
		}

		std::vector<unsigned char*> program_binaries_ptrs(program_num_devices);
		for(size_t i=0; i<program_binary_sizes.size(); ++i)
		{
			program_binaries_ptrs[i] = (unsigned char*)&(*program_binaries[i].begin());
		}

		this->clGetProgramInfo(
			program,
			CL_PROGRAM_BINARIES,
			sizeof(unsigned char*) * program_binaries_ptrs.size(), // size
			&program_binaries_ptrs[0],
			&param_value_size_ret
			);

		for(size_t i=0; i<program_binary_sizes.size(); ++i)
		{
			std::cout << "Writing Program " << i << " binary to disk." << std::endl;

			std::ofstream binfile("binary.txt");
			binfile.write(program_binaries[i].c_str(), program_binaries[i].size());

			std::cout << "\tDone." << std::endl;

			//std::cout << program_binaries[i] << std::endl;
		}
	}

	return program;
}


void OpenCL::dumpBuildLog(cl_program program, PrintOutput& print_output)
{
	cl_int result;

	// Call once to get the size of the buffer
	size_t param_value_size_ret;
	result = this->clGetProgramBuildInfo(
		program,
		this->device_to_use,
		CL_PROGRAM_BUILD_LOG,
		0, // param value size
		NULL, // param value
		&param_value_size_ret);

	std::vector<char> buf(param_value_size_ret);

	result = this->clGetProgramBuildInfo(
		program,
		this->device_to_use,
		CL_PROGRAM_BUILD_LOG,
		buf.size(),
		&buf[0],
		NULL);
	if(result == CL_SUCCESS)
	{
		const std::string log(&buf[0], param_value_size_ret);
		print_output.print("OpenCL build log: " + log);


		{
			std::ofstream build_log("build_log.txt");
			build_log << log;
		}
	}
	else
		throw Indigo::Exception("clGetProgramBuildInfo failed: " + errorString(result));
}


#endif
