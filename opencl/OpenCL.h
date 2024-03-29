/*=====================================================================
OpenCL.h
--------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "OpenCLProgram.h"
#include "OpenCLContext.h"
#include "OpenCLPlatform.h"
#include "../utils/IncludeWindows.h"
#include "../utils/Platform.h"
#include "../utils/DynamicLib.h"
#include <string>
#include <vector>


#ifdef OSX
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include <CL/cl_ext.h>
#include <CL/cl_gl.h>
#endif

//#include <CL/clext.h>


// If OPENCL_MEM_LOG is defined, all allocations and deallocations are printed to the console
//#define OPENCL_MEM_LOG


extern "C"
{

typedef cl_int (CL_API_CALL *clGetPlatformIDs_TYPE) (cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
typedef cl_int (CL_API_CALL *clGetPlatformInfo_TYPE) (cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clGetDeviceIDs_TYPE) (cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
typedef cl_int (CL_API_CALL *clGetDeviceInfo_TYPE) (cl_device_id device, cl_device_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);

typedef cl_context (CL_API_CALL *clCreateContext_TYPE) (const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, void (CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info, size_t cb, void* user_data), void* user_data, cl_int* errcode_ret);
#if defined(_WIN32)
typedef cl_context (CL_API_CALL *clCreateContextFromType_TYPE) (cl_context_properties *properties, cl_device_type device_type, void (*pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret);
#else
typedef cl_context (CL_API_CALL *clCreateContextFromType_TYPE) (const cl_context_properties *properties, cl_device_type device_type, void (*pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret);
#endif
	
typedef cl_int (CL_API_CALL *clReleaseContext_TYPE) (cl_context context);
typedef cl_command_queue (CL_API_CALL *clCreateCommandQueue_TYPE) (cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clReleaseCommandQueue_TYPE) (cl_command_queue command_queue);
typedef cl_mem (CL_API_CALL *clCreateBuffer_TYPE) (cl_context context, cl_mem_flags flags, size_t size, void *host_ptr, cl_int *errcode_ret);
typedef cl_mem (CL_API_CALL *clCreateImage_TYPE) (cl_context context, cl_mem_flags flags, const cl_image_format *image_format, const cl_image_desc *image_desc, void *host_ptr, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clReleaseMemObject_TYPE) (cl_mem memobj);
typedef cl_int (CL_API_CALL *clRetainEvent_TYPE) (cl_event event);
typedef cl_program (CL_API_CALL *clCreateProgramWithSource_TYPE) (cl_context context, cl_uint count, const char **strings, const size_t *lengths, cl_int *errcode_ret);
typedef cl_program (CL_API_CALL *clCreateProgramWithBinary_TYPE) (cl_context context, cl_uint num_devices, const cl_device_id * device_list, const size_t* lengths, const unsigned char**, cl_int* binary_status, cl_int* errcode_ret);
typedef cl_int (CL_API_CALL *clBuildProgram_TYPE) (cl_program program, cl_uint num_devices, const cl_device_id *device_list, const char *options, void (*pfn_notify)(cl_program, void *user_data), void *user_data);
typedef cl_int (CL_API_CALL *clGetProgramBuildInfo_TYPE) (cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_kernel (CL_API_CALL *clCreateKernel_TYPE) (cl_program program, const char *kernel_name, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clSetKernelArg_TYPE) (cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void *arg_value);
typedef cl_int (CL_API_CALL *clEnqueueWriteBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb, const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clEnqueueReadBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb, void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef void * (CL_API_CALL *clEnqueueMapBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t cb, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clEnqueueUnmapMemObject_TYPE) (cl_command_queue command_queue, cl_mem memobj, void *mapped_ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clEnqueueNDRangeKernel_TYPE) (cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset, const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clReleaseKernel_TYPE) (cl_kernel kernel);
typedef cl_int (CL_API_CALL *clReleaseProgram_TYPE) (cl_program program);
typedef cl_int (CL_API_CALL *clReleaseEvent_TYPE) (cl_event event);
typedef cl_int (CL_API_CALL *clGetProgramInfo_TYPE) (cl_program program, cl_program_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clGetKernelWorkGroupInfo_TYPE) (cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clGetEventProfilingInfo_TYPE) (cl_event event, cl_profiling_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clGetEventInfo_TYPE)  (cl_event event, cl_event_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clEnqueueMarker_TYPE) (cl_command_queue command_queue, cl_event *event);
typedef cl_int (CL_API_CALL *clWaitForEvents_TYPE) (cl_uint num_events, const cl_event *event_list);
typedef cl_int (CL_API_CALL *clGetMemObjectInfo_TYPE) (cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);


typedef cl_int (CL_API_CALL *clFinish_TYPE) (cl_command_queue command_queue);
typedef cl_int (CL_API_CALL *clFlush_TYPE) (cl_command_queue command_queue);

typedef cl_int (CL_API_CALL *clEnqueueFillBuffer_TYPE)(cl_command_queue, cl_mem buffer, const void * pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event * event_wait_list, cl_event * event);

#if OPENCL_OPENGL_INTEROP
typedef void * (CL_API_CALL *clGetExtensionFunctionAddress_TYPE) (const char *funcname);
typedef cl_int (CL_API_CALL *clGetGLContextInfoKHR_TYPE) (cl_context_properties *properties, cl_gl_context_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_mem (CL_API_CALL *clCreateFromGLTexture_TYPE) (cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int *errcode_ret);
typedef cl_mem (CL_API_CALL *clCreateFromGLTexture2D_TYPE) (cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel, cl_GLuint texture, cl_int *errcode_ret);
typedef cl_mem (CL_API_CALL *clCreateFromGLBuffer_TYPE) (cl_context context, cl_mem_flags flags, /*GLuint*/unsigned int bufobj, cl_int * errcode_ret);
typedef cl_int (CL_API_CALL *clEnqueueAcquireGLObjects_TYPE) (cl_command_queue command_queue, cl_uint num_objects, const cl_mem *mem_objects, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clEnqueueReleaseGLObjects_TYPE) (cl_command_queue command_queue, cl_uint num_objects, const cl_mem *mem_objects, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
#endif

}




/*=====================================================================
OpenCL
------

=====================================================================*/
class OpenCL
{
public:
	OpenCL(bool verbose);
	~OpenCL();


	void queryDevices();
	const std::vector<OpenCLDeviceRef>& getOpenCLDevices() const;
	unsigned int getNumPlatforms() const;
	const std::vector<OpenCLPlatformRef>& getPlatforms() const { return platforms; }
	OpenCLPlatformRef getPlatformForPlatformID(cl_platform_id platform_id) const;

	
	static const std::string errorString(cl_int result);

	// Build an OpenCL program from source, for a set of devices on a particular platform.
	OpenCLProgramRef buildProgram(
		const std::string& program_source,
		OpenCLContextRef& opencl_context,
		const std::vector<OpenCLDeviceRef>& devices,
		const std::string& compile_options,
		std::string& build_log_out // Will be set to a non-empty string on build failure.
	);

	const std::string getBuildLog(cl_program program, cl_device_id device);

	void dumpProgramBinaryToDisk(cl_program program);


	clGetPlatformIDs_TYPE clGetPlatformIDs;
	clGetPlatformInfo_TYPE clGetPlatformInfo;
	clGetDeviceIDs_TYPE clGetDeviceIDs;
	clGetDeviceInfo_TYPE clGetDeviceInfo;
	clCreateContext_TYPE clCreateContext;
	clCreateContextFromType_TYPE clCreateContextFromType;
	clReleaseContext_TYPE clReleaseContext;
	clCreateCommandQueue_TYPE clCreateCommandQueue;
	clReleaseCommandQueue_TYPE clReleaseCommandQueue;
	clCreateBuffer_TYPE clCreateBuffer;
	clCreateImage_TYPE clCreateImage;
	clReleaseMemObject_TYPE clReleaseMemObject;
	clRetainEvent_TYPE clRetainEvent;
	clCreateProgramWithSource_TYPE clCreateProgramWithSource;
	clCreateProgramWithBinary_TYPE clCreateProgramWithBinary;
	clBuildProgram_TYPE clBuildProgram;
	clGetProgramBuildInfo_TYPE clGetProgramBuildInfo;
	clCreateKernel_TYPE clCreateKernel;
	clSetKernelArg_TYPE clSetKernelArg;
	clEnqueueWriteBuffer_TYPE clEnqueueWriteBuffer;
	clEnqueueReadBuffer_TYPE clEnqueueReadBuffer;
	clEnqueueMapBuffer_TYPE clEnqueueMapBuffer;
	clEnqueueUnmapMemObject_TYPE clEnqueueUnmapMemObject;
	clEnqueueNDRangeKernel_TYPE clEnqueueNDRangeKernel;
	clReleaseKernel_TYPE clReleaseKernel;
	clReleaseProgram_TYPE clReleaseProgram;
	clReleaseEvent_TYPE clReleaseEvent;
	clGetProgramInfo_TYPE clGetProgramInfo;
	clGetKernelWorkGroupInfo_TYPE clGetKernelWorkGroupInfo;
	clGetEventProfilingInfo_TYPE clGetEventProfilingInfo;
	clGetEventInfo_TYPE clGetEventInfo;
	clWaitForEvents_TYPE clWaitForEvents;
	clGetMemObjectInfo_TYPE clGetMemObjectInfo;
	clFinish_TYPE clFinish;
	clFlush_TYPE clFlush;
	clEnqueueFillBuffer_TYPE clEnqueueFillBuffer; // May be NULL if the OpenCL version is < 1.2.

#if OPENCL_OPENGL_INTEROP
	// Extensions
	clGetExtensionFunctionAddress_TYPE clGetExtensionFunctionAddress;
	clGetGLContextInfoKHR_TYPE clGetGLContextInfoKHR;
	clCreateFromGLTexture_TYPE clCreateFromGLTexture;
	clCreateFromGLTexture2D_TYPE clCreateFromGLTexture2D;
	clCreateFromGLBuffer_TYPE clCreateFromGLBuffer;
	clEnqueueAcquireGLObjects_TYPE clEnqueueAcquireGLObjects;
	clEnqueueReleaseGLObjects_TYPE clEnqueueReleaseGLObjects;

	cl_context_properties current_gl_context;
	cl_context_properties current_DC;
#endif

private:
	void libraryInit();

#if defined(_WIN32) || defined(__linux__)
	glare::DynamicLib opencl_lib;
#endif

	bool initialised;
	bool verbose;

	// Queried platforms and devices
	std::vector<OpenCLPlatformRef> platforms;
	std::vector<OpenCLDeviceRef> devices;
};


// If we failed to create the OpenCL object (for example if no OpenCL driver found)
// returns NULL, and sets an error message that can be retrieved with getGlobalOpenCLLastErrorMsg().
OpenCL* getGlobalOpenCL();
void destroyGlobalOpenCL();
std::string getGlobalOpenCLLastErrorMsg();
