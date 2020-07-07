/*=====================================================================
OpenCL.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCL.h"


#include "../dll/IndigoStringUtils.h"
#include "../maths/mathstypes.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/Mutex.h"
#include "../utils/Lock.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/FilePrintOutput.h"
#include "../utils/ProfilerStore.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#if defined(__linux__)
#include <dlfcn.h>
#endif


// for cl_amd_device_attribute_query extension. 
#ifndef CL_DEVICE_BOARD_NAME_AMD
#define CL_DEVICE_BOARD_NAME_AMD 0x4038
#endif


OpenCL::OpenCL(bool verbose_)
:	initialised(false),
	verbose(verbose_)
{
	// Initialise the OpenCL library, importing the function pointers etc.
	libraryInit();
}


OpenCL::~OpenCL()
{
}


void OpenCL::libraryInit()
{
#if defined(_WIN32) || defined(__linux__)

	std::vector<std::string> opencl_paths;

#if defined(_WIN32)
	opencl_paths.push_back("OpenCL.dll");
#elif defined(__linux__)
	opencl_paths.push_back("libOpenCL.so");
	// The following names are the names the nvidia provides libOpenCL as on Ubuntu Linux
	opencl_paths.push_back("libOpenCL.so.1");
	opencl_paths.push_back("libOpenCL.so.1.0");
	opencl_paths.push_back("libOpenCL.so.1.0.0");
#endif

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
			clCreateContext = opencl_lib.getFuncPointer<clCreateContext_TYPE>("clCreateContext");
			clCreateContextFromType = opencl_lib.getFuncPointer<clCreateContextFromType_TYPE>("clCreateContextFromType");
			clReleaseContext = opencl_lib.getFuncPointer<clReleaseContext_TYPE>("clReleaseContext");
			clCreateCommandQueue = opencl_lib.getFuncPointer<clCreateCommandQueue_TYPE>("clCreateCommandQueue");
			clReleaseCommandQueue = opencl_lib.getFuncPointer<clReleaseCommandQueue_TYPE>("clReleaseCommandQueue");
			clCreateBuffer = opencl_lib.getFuncPointer<clCreateBuffer_TYPE>("clCreateBuffer");
			clCreateImage = opencl_lib.getFuncPointer<clCreateImage_TYPE>("clCreateImage");
			clReleaseMemObject = opencl_lib.getFuncPointer<clReleaseMemObject_TYPE>("clReleaseMemObject");
			clRetainEvent = opencl_lib.getFuncPointer<clRetainEvent_TYPE>("clRetainEvent");
			clCreateProgramWithSource = opencl_lib.getFuncPointer<clCreateProgramWithSource_TYPE>("clCreateProgramWithSource");
			clCreateProgramWithBinary = opencl_lib.getFuncPointer<clCreateProgramWithBinary_TYPE>("clCreateProgramWithBinary");
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
			clGetEventProfilingInfo = opencl_lib.getFuncPointer<clGetEventProfilingInfo_TYPE>("clGetEventProfilingInfo");
			clGetEventInfo = opencl_lib.getFuncPointer<clGetEventInfo_TYPE>("clGetEventInfo");
			clWaitForEvents = opencl_lib.getFuncPointer<clWaitForEvents_TYPE>("clWaitForEvents");
			clFinish = opencl_lib.getFuncPointer<clFinish_TYPE>("clFinish");
			clFlush = opencl_lib.getFuncPointer<clFlush_TYPE>("clFlush");
			clGetMemObjectInfo = opencl_lib.getFuncPointer<clGetMemObjectInfo_TYPE>("clGetMemObjectInfo");

			// OpenCL 1.2 function:
			clEnqueueFillBuffer = opencl_lib.tryGetFuncPointer<clEnqueueFillBuffer_TYPE>("clEnqueueFillBuffer");

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
	this->clCreateImage = ::clCreateImage;
	this->clReleaseMemObject = ::clReleaseMemObject;
	this->clRetainEvent = ::clRetainEvent;
	this->clCreateProgramWithSource = ::clCreateProgramWithSource;
	this->clCreateProgramWithBinary = ::clCreateProgramWithBinary;
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
	this->clGetEventProfilingInfo = ::clGetEventProfilingInfo;
	this->clGetEventInfo = ::clGetEventInfo;
	this->clWaitForEvents = ::clWaitForEvents;
	this->clFinish = ::clFinish;
	this->clFlush = ::clFlush;
	this->clGetMemObjectInfo = ::clGetMemObjectInfo;

	// OpenCL 1.2 function:
	this->clEnqueueFillBuffer = ::clEnqueueFillBuffer;
	
	if(this->clGetPlatformIDs == NULL)
		throw Indigo::Exception("OpenCL is not available in this version of OSX.");
#endif

	initialised = true;
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
	{
		ScopeProfiler _scope("clGetPlatformIDs", 1);
		cl_int result = this->clGetPlatformIDs(128, &platform_ids[0], &num_platforms);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformIDs failed: " + errorString(result));
	}

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

	StandardPrintOutput print_output;
	//FilePrintOutput print_output("d:/opencl_query_log.txt");
#if defined(_WIN32) // getFullPathToLib() is only implemented for Windows currently.
	if(verbose) print_output.print("OpenCL dynamic lib path: " + opencl_lib.getFullPathToLib());
#endif
	if(verbose) print_output.print("Num platforms: " + toString(num_platforms));

	devices.clear();

	for(cl_uint i = 0; i < num_platforms; ++i)
	{
		platforms.push_back(new OpenCLPlatform(platform_ids[i]));

		std::string platform_vendor;
		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VENDOR, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		platform_vendor = std::string(&char_buff[0]);

		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_EXTENSIONS, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		const std::string platform_extensions_string(&char_buff[0]);
		const std::vector<std::string> platform_extensions = split(platform_extensions_string, ' ');

		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_NAME, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		const std::string platform_name(&char_buff[0]);
		platforms.back()->name = platform_name;

		if(clGetPlatformInfo(platform_ids[i], CL_PLATFORM_VERSION, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
			throw Indigo::Exception("clGetPlatformInfo failed");
		const std::string platform_version(&char_buff[0]);
		platforms.back()->version = platform_version;

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
				

			print_output.print("\n============================Platform ============================");
			print_output.print("platform_id: " + toString((uint64)platform_ids[i]));
			print_output.print("platform_profile: " + platform_profile);
			print_output.print("platform_version: " + platform_version);
			print_output.print("platform_name: " + platform_name);
			print_output.print("platform_vendor: " + platform_vendor);
			print_output.print("platform_extensions: " + platform_extensions_string);
#if OPENCL_OPENGL_INTEROP
			print_output.print("platform_num_GL_devices: " + toString(platform_num_GL_devices));
#endif
		}

		std::vector<cl_device_id> device_ids(64);
		cl_uint num_devices = 0;
		if(clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, (cl_uint)device_ids.size(), &device_ids[0], &num_devices) != CL_SUCCESS)
			throw Indigo::Exception("clGetDeviceIDs failed");

		if(verbose) print_output.print(toString(num_devices) + " device(s) found.");

		for(cl_uint d = 0; d < num_devices; ++d)
		{
			if(verbose) print_output.print("----------- Device " + toString(current_device_number) + " -----------");

			if(verbose) print_output.print("Device id: " + toString((uint64)device_ids[d]));

			cl_device_type device_type;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_TYPE, sizeof(device_type), &device_type, NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");

			if(verbose)
			{
				if(device_type & CL_DEVICE_TYPE_CPU)
					print_output.print("device_type: CL_DEVICE_TYPE_CPU");
				if(device_type & CL_DEVICE_TYPE_GPU)
					print_output.print("device_type: CL_DEVICE_TYPE_GPU");
				if(device_type & CL_DEVICE_TYPE_ACCELERATOR)
					print_output.print("device_type: CL_DEVICE_TYPE_ACCELERATOR");
				if(device_type & CL_DEVICE_TYPE_DEFAULT)
					print_output.print("device_type: CL_DEVICE_TYPE_DEFAULT");
			}

			// Get device extensions.
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_EXTENSIONS, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
				throw Indigo::Exception("clGetDeviceInfo failed");
			const std::string device_extensions(&char_buff[0]);

			// If cl_amd_device_attribute_query extension is supported by the device, use that extension to get the device name.
			// But don't do for CPU devices, as the query just returns an empty string in that case.
			if(StringUtils::containsString(device_extensions, "cl_amd_device_attribute_query") && !(device_type & CL_DEVICE_TYPE_CPU))
			{
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_BOARD_NAME_AMD, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
			}
			else
			{
				if(clGetDeviceInfo(device_ids[d], CL_DEVICE_NAME, char_buff.size(), &char_buff[0], NULL) != CL_SUCCESS)
					throw Indigo::Exception("clGetDeviceInfo failed");
			}
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

			cl_device_fp_config double_fp_capabilities = 0;
			if(clGetDeviceInfo(device_ids[d], CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(double_fp_capabilities), &double_fp_capabilities, NULL) != CL_SUCCESS)
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
				 

				print_output.print("device_name: " + device_name_);
				print_output.print("driver_version: " + driver_version);
				print_output.print("device_profile: " + device_profile);
				print_output.print("device_version: " + device_version);
				print_output.print("device_max_compute_units: " + toString(device_max_compute_units));
				print_output.print("device_max_work_group_size: " + toString(device_max_work_group_size));
				print_output.print("device_max_work_item_dimensions: " + toString(device_max_work_item_dimensions));
				print_output.print("device_image2d_max_width: " + toString(device_image2d_max_width));
				print_output.print("device_max_constant_buffer_size: " + toString(device_max_constant_buffer_size));

				for(size_t z = 0; z < device_max_num_work_items.size(); ++z)
					print_output.print("Dim " + toString(z) + " device_max_num_work_items: " + toString(device_max_num_work_items[z]));

				print_output.print("device_max_clock_frequency: " + toString(device_max_clock_frequency) + " MHz");
				print_output.print("device_global_mem_size: " + toString(device_global_mem_size) + " B");
			}

			// Add device info to devices vector.
			{
				OpenCLDeviceRef opencl_device = new OpenCLDevice();
				opencl_device->opencl_device_id = device_ids[d];
				opencl_device->opencl_platform_id = platform_ids[i];
				opencl_device->opencl_device_type = device_type;

				opencl_device->device_name = device_name_;
				opencl_device->vendor_name = platform_vendor;

				opencl_device->global_mem_size = device_global_mem_size;
				opencl_device->max_mem_alloc_size = device_max_mem_alloc_size;
				opencl_device->compute_units = device_max_compute_units;
				opencl_device->clock_speed = device_max_clock_frequency;

#if OPENCL_OPENGL_INTEROP
				opencl_device->supports_GL_interop = device_OpenGL_interop;
#else
				opencl_device->supports_GL_interop = false;
#endif
				opencl_device->supports_doubles = double_fp_capabilities != 0;

				opencl_device->platform = platforms.back().getPointer();

				platforms.back()->platform_devices.push_back(opencl_device);
				devices.push_back(opencl_device);
			}

			++current_device_number;
		}
	}

	// Sort by device name, then platform vendor name, so that device listing is stable even if OpenCL reports platforms in a different order.
	std::stable_sort(devices.begin(), devices.end(), OpenCLDeviceLessThanName());
	std::stable_sort(devices.begin(), devices.end(), OpenCLDeviceLessThanVendor());

	// After sorting, assign unique id to identical devices.
	std::string prev_dev;
	std::string prev_vendor;
	int64 prev_id = 0;
	for(size_t i = 0; i < devices.size(); ++i)
	{
		// Assign unique id for devices of same vendor and name.
		if(devices[i]->device_name == prev_dev && devices[i]->vendor_name == prev_vendor)
			devices[i]->id = prev_id + 1;
		else
			devices[i]->id = 0;

		prev_dev = devices[i]->device_name;
		prev_vendor = devices[i]->vendor_name;
		prev_id = devices[i]->id;
	}
}


std::vector< ::OpenCLDeviceRef> OpenCL::getSelectedDevices(const std::vector<Indigo::OpenCLDevice>& selected_devices)
{
	std::vector< ::OpenCLDeviceRef> core_selected_devices;

	// For every selected device i.
	for(size_t i = 0; i < selected_devices.size(); ++i)
	{
		// Find the matching device j and add its index to the list of indices.
		for(size_t j = 0; j < devices.size(); ++j)
		{
			// Is it the device we are looking for? I.e. name, vendor and id match.
			if(toStdString(selected_devices[i].device_name) == devices[j]->device_name
				&& toStdString(selected_devices[i].vendor_name) == devices[j]->vendor_name
				&& selected_devices[i].id == devices[j]->id)
			{
				core_selected_devices.push_back(devices[j]);
				break;
			}
		}
	}

	return core_selected_devices;
}


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
const std::vector<OpenCLDeviceRef>& OpenCL::getOpenCLDevices() const
{ 
	return devices;
};


// Only returns correct number of platforms if queryDevices() has been called.
unsigned int OpenCL::getNumPlatforms() const
{
	return (unsigned int)platforms.size();
}


OpenCLPlatformRef OpenCL::getPlatformForPlatformID(cl_platform_id platform_id) const
{
	for(size_t i=0; i<platforms.size(); ++i)
		if(platforms[i]->getPlatformID() == platform_id)
			return platforms[i];
	return NULL;
}


OpenCLProgramRef OpenCL::buildProgram(
	const std::string& program_source,
	OpenCLContextRef& opencl_context,
	const std::vector<OpenCLDeviceRef>& use_devices,
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

	cl_int result = CL_SUCCESS;
	OpenCLProgramRef program = new OpenCLProgram(this->clCreateProgramWithSource(
			opencl_context->getContext(),
			(cl_uint)strings.size(), // count
			strings.data(),
			lengths.data(),
			&result
		),
		opencl_context
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateProgramWithSource failed: " + errorString(result));

	// Build program
	//Timer timer;
	std::vector<cl_device_id> device_ids(use_devices.size());
	for(size_t i=0; i<use_devices.size(); ++i)
		device_ids[i] = use_devices[i]->opencl_device_id;

	result = this->clBuildProgram(
		program->getProgram(),
		(cl_uint)device_ids.size(), // num devices
		device_ids.data(), // device ids
		compile_options.c_str(), // options
		NULL, // pfn_notify
		NULL // user data
	);
	// conPrint("clBuildProgram took " + timer.elapsedStringNSigFigs(4));

	if(result != CL_SUCCESS)
	{
		try
		{
			build_log_out = "";
			for(size_t i=0; i<device_ids.size(); ++i)
				build_log_out += getBuildLog(program->getProgram(), device_ids[i]);
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

	conPrint("program_num_devices: " + toString(program_num_devices));

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
		conPrint("\tprogram " + toString(i) + " binary size: " + toString(program_binary_sizes[i]) + " B");
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
		conPrint("Writing Program " + toString(i) + " binary to disk.");

		std::ofstream binfile("binary.txt");
		binfile.write(program_binaries[i].c_str(), program_binaries[i].size());

		conPrint("\tDone.");
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


static Mutex global_opencl_mutex;
static OpenCL* global_opencl = NULL;
static bool open_cl_load_failed = false;
static std::string last_error_msg;


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
			last_error_msg = "";
		}
		catch(Indigo::Exception& e)
		{
			conPrint("Error initialising OpenCL: " + e.what());
			last_error_msg = e.what();
			delete global_opencl; // global_opencl may be non-null if it failed in queryDevices().
			global_opencl = NULL;
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


std::string getGlobalOpenCLLastErrorMsg()
{
	Lock lock(global_opencl_mutex);
	return last_error_msg;
}
