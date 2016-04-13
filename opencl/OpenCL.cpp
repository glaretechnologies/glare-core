/*=====================================================================
OpenCL.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCL.h"


#include "../indigo/gpuDeviceInfo.h"
#include "../maths/mathstypes.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/Mutex.h"
#include "../utils/Lock.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>
#if defined(__linux__)
#include <dlfcn.h>
#endif


OpenCL::OpenCL(bool verbose_)
:	initialised(false),
	verbose(verbose_)
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
}


void OpenCL::libraryInit()
{
#if USE_OPENCL

	std::vector<std::string> opencl_paths;

#if defined(_WIN32)
	opencl_paths.push_back("OpenCL.dll");
/*
	try // ATI/AMD OpenCL
	{
		std::string ati_sdk_root = PlatformUtils::getEnvironmentVariable("ATISTREAMSDKROOT");
	#if defined(_WIN64)
		if(verbose) conPrint("Detected ATI 64 bit OpenCL SDK at " + ati_sdk_root);
		opencl_paths.push_back(ati_sdk_root + "bin\\x86_64\\atiocl64.dll");
	#else
		if(verbose) conPrint("Detected ATI 32 bit OpenCL SDK at " + ati_sdk_root);
		opencl_paths.push_back(ati_sdk_root + "bin\\x86\\atiocl.dll");
	#endif
	}
	catch(PlatformUtils::PlatformUtilsExcep&) { } // No ATI/AMD OpenCL found

	try // Intel OpenCL
	{
		std::string intel_sdk_root = PlatformUtils::getEnvironmentVariable("INTELOCLSDKROOT");

	#if defined(_WIN64)
		if(verbose) conPrint("Detected Intel 64 bit OpenCL SDK at " + intel_sdk_root);
		opencl_paths.push_back(intel_sdk_root + "bin\\x64\\intelocl.dll");
	#else
		if(verbose) conPrint("Detected Intel 64 bit OpenCL SDK at " + intel_sdk_root);
		opencl_paths.push_back(intel_sdk_root + "bin\\x86\\intelocl.dll");
	#endif
	}
	catch(PlatformUtils::PlatformUtilsExcep&) { } // No Intel OpenCL found
*/
#else
	opencl_paths.push_back("libOpenCL.so");
#endif


#if defined(_WIN32) || defined(__linux__)
	size_t searched_paths = 0;
	for( ; searched_paths < opencl_paths.size(); ++searched_paths)
	{
		try
		{
			opencl_lib.open(opencl_paths[searched_paths]);
		}
		catch(Indigo::Exception&)
		{
			continue;
		}

		// Successfully loaded library, try to get required function pointers
		try
		{
			clGetPlatformIDs = opencl_lib.getFuncPointer<clGetPlatformIDs_TYPE>("clGetPlatformIDs");
			clGetPlatformInfo = opencl_lib.getFuncPointer<clGetPlatformInfo_TYPE>("clGetPlatformInfo");
			clGetDeviceIDs = opencl_lib.getFuncPointer<clGetDeviceIDs_TYPE>("clGetDeviceIDs");
			clGetDeviceInfo = opencl_lib.getFuncPointer<clGetDeviceInfo_TYPE>("clGetDeviceInfo");
			clCreateContextFromType = opencl_lib.getFuncPointer<clCreateContextFromType_TYPE>("clCreateContextFromType");
			clReleaseContext = opencl_lib.getFuncPointer<clReleaseContext_TYPE>("clReleaseContext");
			clCreateCommandQueue = opencl_lib.getFuncPointer<clCreateCommandQueue_TYPE>("clCreateCommandQueue");
			clReleaseCommandQueue = opencl_lib.getFuncPointer<clReleaseCommandQueue_TYPE>("clReleaseCommandQueue");
			clCreateBuffer = opencl_lib.getFuncPointer<clCreateBuffer_TYPE>("clCreateBuffer");
			clCreateImage2D = opencl_lib.getFuncPointer<clCreateImage2D_TYPE>("clCreateImage2D");
			clReleaseMemObject = opencl_lib.getFuncPointer<clReleaseMemObject_TYPE>("clReleaseMemObject");
			clRetainEvent = opencl_lib.getFuncPointer<clRetainEvent_TYPE>("clRetainEvent");
			clCreateProgramWithSource = opencl_lib.getFuncPointer<clCreateProgramWithSource_TYPE>("clCreateProgramWithSource");
			clBuildProgram = opencl_lib.getFuncPointer<clBuildProgram_TYPE>("clBuildProgram");
			clGetProgramBuildInfo = opencl_lib.getFuncPointer<clGetProgramBuildInfo_TYPE>("clGetProgramBuildInfo");
			clCreateKernel = opencl_lib.getFuncPointer<clCreateKernel_TYPE>("clCreateKernel");
			clSetKernelArg = opencl_lib.getFuncPointer<clSetKernelArg_TYPE>("clSetKernelArg");
			clEnqueueWriteBuffer = opencl_lib.getFuncPointer<clEnqueueWriteBuffer_TYPE>("clEnqueueWriteBuffer");
			clEnqueueReadBuffer = opencl_lib.getFuncPointer<clEnqueueReadBuffer_TYPE>("clEnqueueReadBuffer");
			clEnqueueMapBuffer = opencl_lib.getFuncPointer<clEnqueueMapBuffer_TYPE>("clEnqueueMapBuffer");
			clEnqueueUnmapMemObject = opencl_lib.getFuncPointer<clEnqueueUnmapMemObject_TYPE>("clEnqueueUnmapMemObject");
			clEnqueueNDRangeKernel = opencl_lib.getFuncPointer<clEnqueueNDRangeKernel_TYPE>("clEnqueueNDRangeKernel");
			clReleaseKernel = opencl_lib.getFuncPointer<clReleaseKernel_TYPE>("clReleaseKernel");
			clReleaseProgram = opencl_lib.getFuncPointer<clReleaseProgram_TYPE>("clReleaseProgram");
			clReleaseEvent = opencl_lib.getFuncPointer<clReleaseEvent_TYPE>("clReleaseEvent");
			clGetProgramInfo = opencl_lib.getFuncPointer<clGetProgramInfo_TYPE>("clGetProgramInfo");
			clGetKernelWorkGroupInfo = opencl_lib.getFuncPointer<clGetKernelWorkGroupInfo_TYPE>("clGetKernelWorkGroupInfo");

			// OpenCL 1.0 function deprecated in 1.1.
			//clSetCommandQueueProperty = opencl_lib.getFuncPointer<clSetCommandQueueProperty_TYPE>("clSetCommandQueueProperty");
			clGetEventProfilingInfo = opencl_lib.getFuncPointer<clGetEventProfilingInfo_TYPE>("clGetEventProfilingInfo");
			clGetEventInfo = opencl_lib.getFuncPointer<clGetEventInfo_TYPE>("clGetEventInfo");
			clWaitForEvents = opencl_lib.getFuncPointer<clWaitForEvents_TYPE>("clWaitForEvents");

			clFinish = opencl_lib.getFuncPointer<clFinish_TYPE>("clFinish");
			clFlush = opencl_lib.getFuncPointer<clFlush_TYPE>("clFlush");

#if OPENCL_OPENGL_INTEROP
			clGetExtensionFunctionAddress = opencl_lib.getFuncPointer<clGetExtensionFunctionAddress_TYPE>("clGetExtensionFunctionAddress");
			clGetGLContextInfoKHR = (clGetGLContextInfoKHR_TYPE)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
			clCreateFromGLTexture = (clCreateFromGLTexture_TYPE)clGetExtensionFunctionAddress("clCreateFromGLTexture");
			clCreateFromGLTexture2D = (clCreateFromGLTexture2D_TYPE)clGetExtensionFunctionAddress("clCreateFromGLTexture2D");
			clEnqueueAcquireGLObjects = (clEnqueueAcquireGLObjects_TYPE)clGetExtensionFunctionAddress("clEnqueueAcquireGLObjects");
			clEnqueueReleaseGLObjects = (clEnqueueReleaseGLObjects_TYPE)clGetExtensionFunctionAddress("clEnqueueReleaseGLObjects");
#endif
		}
		catch(Indigo::Exception& e)
		{
			if(verbose) conPrint("Error loading OpenCL library from " + opencl_paths[searched_paths] + ": " + e.what());
			continue; // try the next library
		}

		if(verbose) conPrint("Successfully loaded OpenCL functions from " + opencl_paths[searched_paths]);
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
	this->clRetainEvent = ::clRetainEvent;
	this->clCreateProgramWithSource = ::clCreateProgramWithSource;
	this->clBuildProgram = ::clBuildProgram;
	this->clGetProgramBuildInfo = ::clGetProgramBuildInfo;
	this->clCreateKernel = ::clCreateKernel;
	this->clSetKernelArg = ::clSetKernelArg;
	this->clEnqueueWriteBuffer = ::clEnqueueWriteBuffer;
	this->clEnqueueReadBuffer = ::clEnqueueReadBuffer;
	this->clEnqueueMapBuffer = ::clEnqueueMapBuffer;
	this->clEnqueueUnmapMemObject = ::clEnqueueUnmapMemObject;
	this->clEnqueueNDRangeKernel = ::clEnqueueNDRangeKernel;
	this->clReleaseKernel = ::clReleaseKernel;
	this->clReleaseProgram = ::clReleaseProgram;
	this->clReleaseEvent = ::clReleaseEvent;
	this->clGetProgramInfo = ::clGetProgramInfo;
	this->clGetKernelWorkGroupInfo = ::clGetKernelWorkGroupInfo;

	// OpenCL 1.0 function deprecated in 1.1.
	//this->clSetCommandQueueProperty = ::clSetCommandQueueProperty;
	this->clGetEventProfilingInfo = ::clGetEventProfilingInfo;
	this->clGetEventInfo = ::clGetEventInfo;
	this->clWaitForEvents = ::clWaitForEvents;

	this->clFinish = ::clFinish;
	this->clFlush = ::clFlush;
	
	if(this->clGetPlatformIDs == NULL)
		throw Indigo::Exception("OpenCL is not available in this version of OSX.");
#endif

	initialised = true;

#else
	throw Indigo::Exception("OpenCL disabled.");
#endif
}


#if USE_OPENCL
// Comparison operator for sorting by platform vendor name, so that device listing is stable even if OpenCL reports platforms in a different order.
bool devicePlatformSort(const gpuDeviceInfo& lhs, const gpuDeviceInfo& rhs)
{
	//return lhs.opencl_platform_vendor < rhs.opencl_platform_vendor;
	return lhs.opencl_platform >= rhs.opencl_platform;
}


void OpenCL::queryDevices()
{
	if(!initialised)
		throw Indigo::Exception("OpenCL library not initialised");

	// Temporary storage for the strings OpenCL returns
	std::vector<char> char_buff(128 * 1024);

	int current_device_number = 0;

	std::vector<cl_platform_id> platform_ids(128);
	cl_uint num_platforms = 0;
	cl_int result = this->clGetPlatformIDs(128, &platform_ids[0], &num_platforms);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clGetPlatformIDs failed: " + errorString(result));


#if OPENCL_OPENGL_INTEROP
#if defined(_WIN32)
	const cl_context_properties current_context = (cl_context_properties)wglGetCurrentContext();
	const cl_context_properties current_DC = (cl_context_properties)wglGetCurrentDC();
#else
	// TODO mac and linux
	const cl_context_properties current_context = 0;
	const cl_context_properties current_DC = 0;
#endif
#endif

	if(verbose) conPrint("Num platforms: " + toString(num_platforms));

	device_info.clear();
	info.platforms.clear();

	for(cl_uint i = 0; i < num_platforms; ++i)
	{
		std::string platform_vendor;
		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VENDOR, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		platform_vendor = std::string(&char_buff[0]);

		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_EXTENSIONS, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		const std::string platform_extensions_string(&char_buff[0]);
		const std::vector<std::string> platform_extensions = split(platform_extensions_string, ' ');

#if OPENCL_OPENGL_INTEROP
		// Check extensions for OpenGL interop
		bool platform_OpenGL_interop = false;
		for(size_t j = 0; j < platform_extensions.size(); ++j)
		{
			if(platform_extensions[j] == "cl_khr_gl_sharing" || platform_extensions[j] == "cl_APPLE_gl_sharing")
			{
				platform_OpenGL_interop = true;
				break;
			}
		}

		cl_device_id platform_GL_devices[32];
		int platform_num_GL_devices = 0;
		if(platform_OpenGL_interop)
		{
			// Create CL context properties, add WGL context & handle to DC
			cl_context_properties properties[] =
			{
				CL_GL_CONTEXT_KHR, current_context, // WGL Context
				CL_WGL_HDC_KHR, current_DC, // WGL HDC
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform_ids[i], // OpenCL platform
				0
			};

			// Find CL capable devices in the current GL context
			size_t devices_size;
			if(clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 32 * sizeof(cl_device_id), platform_GL_devices, &devices_size) == CL_SUCCESS)
			platform_num_GL_devices = (int)(devices_size / sizeof(cl_device_id));
		}
#endif

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

			conPrint("");
			conPrint("platform_id: " + toString((uint64)platform_ids[i]));
			conPrint("platform_profile: " + platform_profile);
			conPrint("platform_version: " + platform_version);
			conPrint("platform_name: " + platform_name);
			conPrint("platform_vendor: " + platform_vendor);
			conPrint("platform_extensions: " + platform_extensions_string);
#if OPENCL_OPENGL_INTEROP
			conPrint("platform_num_GL_devices: " + platform_num_GL_devices);
#endif
		}

		OpenCLPlatform opencl_platform;
		opencl_platform.platform_id = platform_ids[i];
		opencl_platform.vendor_name = platform_vendor;

		std::vector<cl_device_id> device_ids(16);
		cl_uint num_devices = 0;
		if(clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, (cl_uint)device_ids.size(), &device_ids[0], &num_devices) != CL_SUCCESS)
			throw Indigo::Exception("clGetDeviceIDs failed");

		if(verbose) conPrint(toString(num_devices) + " device(s) found.");

		for(cl_uint d = 0; d < num_devices; ++d)
		{
			if(verbose) conPrint("----------- Device " + toString(current_device_number) + " -----------");

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

			cl_ulong device_max_mem_alloc_size = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(device_max_mem_alloc_size), &device_max_mem_alloc_size, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			cl_uint device_max_compute_units = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(device_max_compute_units), &device_max_compute_units, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			cl_uint device_max_clock_frequency = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(device_max_clock_frequency), &device_max_clock_frequency, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");
			const std::string device_version(&char_buff[0]);

			if(clGetDeviceInfo(device_ids[d], CL_DRIVER_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");
			const std::string driver_version(&char_buff[0]);
			
			cl_ulong device_max_constant_buffer_size = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(device_max_constant_buffer_size), &device_max_constant_buffer_size, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

#if OPENCL_OPENGL_INTEROP
			bool device_OpenGL_interop = false;
			for(int j = 0; j < platform_num_GL_devices; ++j)
				if(platform_GL_devices[j] == device_ids[d]) { device_OpenGL_interop = true; break; }
#endif

			if(verbose)
			{
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_PROFILE, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
				const std::string device_profile(&char_buff[0]);

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
				std::cout << "device_max_constant_buffer_size: " << device_max_constant_buffer_size << std::endl;

				for(size_t z = 0; z < device_max_num_work_items.size(); ++z)
					std::cout << "Dim " << z << " device_max_num_work_items: " << device_max_num_work_items[z] << std::endl;

				std::cout << "device_max_clock_frequency: " << device_max_clock_frequency << " MHz" << std::endl;
				std::cout << "device_global_mem_size: " << device_global_mem_size << " B" << std::endl;
			}

			// Add device info structure to the vector.
			{
				gpuDeviceInfo di;
				di.device_number = current_device_number;
				di.device_name = device_name_;
				di.total_memory_size = (size_t)device_global_mem_size;
				di.core_count = device_max_compute_units;
				di.core_clock = device_max_clock_frequency; // in MHz
				di.CPU = (device_type & CL_DEVICE_TYPE_CPU) != 0;
				di.max_constant_buffer_size = device_max_constant_buffer_size;

				di.opencl_platform = platform_ids[i];
				di.opencl_device = device_ids[d];
				di.opencl_platform_vendor = platform_vendor;
				di.OpenCL_version = device_version;
				di.OpenCL_driver_info = driver_version;

#if OPENCL_OPENGL_INTEROP
				di.supports_OpenGL_interop = device_OpenGL_interop;
#else
				di.supports_OpenGL_interop = false;
#endif
				device_info.push_back(di);
			}

			// NEW
			{
				OpenCLDevice opencl_device;
				opencl_device.device_id = device_ids[d];
				opencl_device.device_type = device_type;

				opencl_device.name = device_name_;

				opencl_device.global_mem_size = device_global_mem_size;
				opencl_device.max_mem_alloc_size = device_max_mem_alloc_size;
				opencl_device.compute_units = device_max_compute_units;
				opencl_device.clock_speed = device_max_clock_frequency;

#if OPENCL_OPENGL_INTEROP
				opencl_device.supports_GL_interop = device_OpenGL_interop;
#else
				opencl_device.supports_GL_interop = false;
#endif

				opencl_platform.devices.push_back(opencl_device);
			}

			++current_device_number;
		}

		// Add this platform and all its devices to the platforms list
		info.platforms.push_back(opencl_platform);
	}

	// Sort by platform vendor name, so that device listing is stable even if OpenCL reports platforms in a different order.
	std::stable_sort(device_info.begin(), device_info.end());
	std::stable_sort(info.platforms.begin(), info.platforms.end());
}


void OpenCL::deviceInit(const gpuDeviceInfo& chosen_device, bool enable_profiling, cl_context& context_out, cl_command_queue& command_queue_out)
{
	if(!initialised)
		throw Indigo::Exception("OpenCL library not initialised");

	cl_context_properties cps[3] =
    {
        CL_CONTEXT_PLATFORM,
		(cl_context_properties)chosen_device.opencl_platform,
        0
    };

	cl_int error_code;

	cl_context new_context = clCreateContextFromType(
		cps, // properties
		CL_DEVICE_TYPE_ALL, // CL_DEVICE_TYPE_CPU, // TEMP CL_DEVICE_TYPE_GPU, // device type
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);
	if(new_context == 0)
		throw Indigo::Exception("clCreateContextFromType failed: " + errorString(error_code));

	// Create command queue
	cl_command_queue new_command_queue = this->clCreateCommandQueue(
		new_context,
		chosen_device.opencl_device,
		enable_profiling ? CL_QUEUE_PROFILING_ENABLE : 0, // queue properties
		&error_code);
	if(new_command_queue == 0)
		throw Indigo::Exception("clCreateCommandQueue failed"); // XXX BUG If this exception is thrown, context gets leaked. Need to use wrapper classes.

	context_out = new_context;
	command_queue_out = new_command_queue;
}


void OpenCL::deviceFree(cl_context& context, cl_command_queue& command_queue)
{
	// Free command queue
	if(command_queue)
	{
		if(clReleaseCommandQueue(command_queue) != CL_SUCCESS)
		{
			assert(0);
		}
	}

	// Cleanup
	if(context)
	{
		if(clReleaseContext(context) != CL_SUCCESS)
		{
			assert(0);
		}
	}
}


#endif


#if USE_OPENCL


// From http://stackoverflow.com/a/24336429
const std::string OpenCL::errorString(cl_int result)
{
	switch(result)
	{
		// run-time and JIT compiler errors
		case 0: return "CL_SUCCESS";
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
		case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
		case -15: return "CL_COMPILE_PROGRAM_FAILURE";
		case -16: return "CL_LINKER_NOT_AVAILABLE";
		case -17: return "CL_LINK_PROGRAM_FAILURE";
		case -18: return "CL_DEVICE_PARTITION_FAILED";
		case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

		// compile-time errors
		case -30: return "CL_INVALID_VALUE";
		case -31: return "CL_INVALID_DEVICE_TYPE";
		case -32: return "CL_INVALID_PLATFORM";
		case -33: return "CL_INVALID_DEVICE";
		case -34: return "CL_INVALID_CONTEXT";
		case -35: return "CL_INVALID_QUEUE_PROPERTIES";
		case -36: return "CL_INVALID_COMMAND_QUEUE";
		case -37: return "CL_INVALID_HOST_PTR";
		case -38: return "CL_INVALID_MEM_OBJECT";
		case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case -40: return "CL_INVALID_IMAGE_SIZE";
		case -41: return "CL_INVALID_SAMPLER";
		case -42: return "CL_INVALID_BINARY";
		case -43: return "CL_INVALID_BUILD_OPTIONS";
		case -44: return "CL_INVALID_PROGRAM";
		case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
		case -46: return "CL_INVALID_KERNEL_NAME";
		case -47: return "CL_INVALID_KERNEL_DEFINITION";
		case -48: return "CL_INVALID_KERNEL";
		case -49: return "CL_INVALID_ARG_INDEX";
		case -50: return "CL_INVALID_ARG_VALUE";
		case -51: return "CL_INVALID_ARG_SIZE";
		case -52: return "CL_INVALID_KERNEL_ARGS";
		case -53: return "CL_INVALID_WORK_DIMENSION";
		case -54: return "CL_INVALID_WORK_GROUP_SIZE";
		case -55: return "CL_INVALID_WORK_ITEM_SIZE";
		case -56: return "CL_INVALID_GLOBAL_OFFSET";
		case -57: return "CL_INVALID_EVENT_WAIT_LIST";
		case -58: return "CL_INVALID_EVENT";
		case -59: return "CL_INVALID_OPERATION";
		case -60: return "CL_INVALID_GL_OBJECT";
		case -61: return "CL_INVALID_BUFFER_SIZE";
		case -62: return "CL_INVALID_MIP_LEVEL";
		case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
		case -64: return "CL_INVALID_PROPERTY";
		case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
		case -66: return "CL_INVALID_COMPILER_OPTIONS";
		case -67: return "CL_INVALID_LINKER_OPTIONS";
		case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

		// extension errors
		case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
		case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
		case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
		case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
		case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
		case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
		default: return "Unknown OpenCL error";
    }
}


// Accessor method for device data queried in the constructor.
const std::vector<gpuDeviceInfo>& OpenCL::getDeviceInfo() const { return device_info; }


cl_program OpenCL::buildProgram(
	const std::string& program_source,
	cl_context opencl_context,
	cl_device_id opencl_device,
	const std::string& compile_options,
	std::string& build_log_out // Will be set to a non-empty string on build failure.
)
{
	// Build string pointer and length arrays
	std::vector<const char*> strings;
	std::vector<size_t> lengths; // Length of each line, including possible \n at end of line.
	size_t last_line_start_i = 0;
	for(size_t i=0; i<program_source.size(); ++i)
	{
		if(program_source[i] == '\n')
		{
			strings.push_back(program_source.data() + last_line_start_i);
			const size_t line_len = i - last_line_start_i + 1; // Plus one to include this newline character.
			assert(line_len >= 1);
			lengths.push_back(line_len);
			// const std::string line(program_source.data() + last_line_start_i, line_len);
			last_line_start_i = i + 1; // Next line starts after this newline char.
		}
	}

	// Add last line (that may not be terminated by a newline character) if there is one.
	assert(last_line_start_i <= program_source.size());
	if(program_source.size() - last_line_start_i >= 1)
	{
		const size_t line_len = program_source.size() - last_line_start_i;
		assert(line_len >= 1);
		strings.push_back(program_source.data() + last_line_start_i);
		lengths.push_back(line_len);
		// const std::string line(program_source.data() + last_line_start_i, line_len);
	}
	assert(strings.size() == lengths.size());

	cl_int result;
	cl_program program = this->clCreateProgramWithSource(
		opencl_context,
		(cl_uint)strings.size(), // count
		strings.data(),
		lengths.data(),
		&result
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateProgramWithSource failed: " + errorString(result));

	// Build program
	//Timer timer;
	result = this->clBuildProgram(
		program,
		1, // num devices
		&opencl_device, // device ids
		compile_options.c_str(), // options
		NULL, // pfn_notify
		NULL // user data
	);
	//std::cout << "clBuildProgram took " + timer.elapsedStringNSigFigs(4) << std::endl; 

	if(result != CL_SUCCESS)
	{
		try
		{
			build_log_out = getBuildLog(program, opencl_device);
		}
		catch(Indigo::Exception& e)
		{
			build_log_out = "[Failed to get build log: " + e.what() + "]";
		}

		throw Indigo::Exception("clBuildProgram failed: " + errorString(result));
	}


	// NOTE: not sure why we were calling this..
	// Get build status.  
	//cl_build_status build_status;
	//this->clGetProgramBuildInfo(
	//	program,
	//	opencl_device,
	//	CL_PROGRAM_BUILD_STATUS,
	//	sizeof(build_status), // param value size
	//	&build_status, // param value
	//	NULL
	//	);
	//if(build_status != CL_BUILD_SUCCESS) // This will happen on compilation error.
	//	throw Indigo::Exception("Kernel build failed.");

	return program;
}


void OpenCL::dumpProgramBinaryToDisk(cl_program program)
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


const std::string OpenCL::getBuildLog(cl_program program, cl_device_id device)
{
	cl_int result;

	// Call once to get the size of the buffer
	size_t param_value_size_ret;
	result = this->clGetProgramBuildInfo(
		program,
		device,
		CL_PROGRAM_BUILD_LOG, // param_name
		0, // param value size.  Ignored as param value is NULL.
		NULL, // param value
		&param_value_size_ret);
	
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clGetProgramBuildInfo failed: " + errorString(result));

	if(param_value_size_ret == 0)
		return std::string();

	std::vector<char> buf(param_value_size_ret);

	result = this->clGetProgramBuildInfo(
		program,
		device,
		CL_PROGRAM_BUILD_LOG,
		buf.size(),
		&buf[0],
		NULL);

	if(result == CL_SUCCESS)
		return std::string(&buf[0], param_value_size_ret);
	else
		throw Indigo::Exception("clGetProgramBuildInfo failed: " + errorString(result));
}


#endif // USE_OPENCL


#if USE_OPENCL


static Mutex global_opencl_mutex;
static OpenCL* global_opencl = NULL;
static bool open_cl_load_failed = false;


OpenCL* getGlobalOpenCL()
{
	Lock lock(global_opencl_mutex);
	
	if(global_opencl == NULL)
	{
		// If OpenCL creation failed last time, don't try again.
		if(open_cl_load_failed)
			return NULL;
		
		try
		{
			const bool verbose = false;
			global_opencl = new OpenCL(verbose);
			global_opencl->queryDevices();
		}
		catch(Indigo::Exception&)
		{
			assert(global_opencl == NULL);
			open_cl_load_failed = true;
		}
	}
		
	return global_opencl;
}


void destroyGlobalOpenCL()
{
	Lock lock(global_opencl_mutex);
	delete global_opencl;
	global_opencl = NULL;
	open_cl_load_failed = false;
}


#else // USE_OPENCL


OpenCL* getGlobalOpenCL() { return NULL; }
void destroyGlobalOpenCL() {}


#endif // USE_OPENCL
