/*=====================================================================
OpenCL.h
--------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OPENCL_H_666_
#define __OPENCL_H_666_

#include <string>
#include <vector>
#include "../utils/platform.h"
#include "../indigo/gpuDeviceInfo.h"


#if USE_OPENCL

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <CL/cl.h>
#include <CL/cl_platform.h>
//#include <CL/clext.h>


extern "C"
{

typedef cl_int (CL_API_CALL *clGetPlatformIDs_TYPE) (cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
typedef cl_int (CL_API_CALL *clGetPlatformInfo_TYPE) (cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clGetDeviceIDs_TYPE) (cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
typedef cl_int (CL_API_CALL *clGetDeviceInfo_TYPE) (cl_device_id device, cl_device_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_context (CL_API_CALL *clCreateContextFromType_TYPE) (cl_context_properties *properties, cl_device_type device_type, void (*pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clReleaseContext_TYPE) (cl_context context);
typedef cl_command_queue (CL_API_CALL *clCreateCommandQueue_TYPE) (cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clReleaseCommandQueue_TYPE) (cl_command_queue command_queue);
typedef cl_mem (CL_API_CALL *clCreateBuffer_TYPE) (cl_context context, cl_mem_flags flags, size_t size, void *host_ptr, cl_int *errcode_ret);
typedef cl_mem (CL_API_CALL *clCreateImage2D_TYPE) (cl_context context, cl_mem_flags flags, const cl_image_format *image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void *host_ptr, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clReleaseMemObject_TYPE) (cl_mem memobj);
typedef cl_program (CL_API_CALL *clCreateProgramWithSource_TYPE) (cl_context context, cl_uint count, const char **strings, const size_t *lengths, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clBuildProgram_TYPE) (cl_program program, cl_uint num_devices, const cl_device_id *device_list, const char *options, void (*pfn_notify)(cl_program, void *user_data), void *user_data);
typedef cl_int (CL_API_CALL *clGetProgramBuildInfo_TYPE) (cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_kernel (CL_API_CALL *clCreateKernel_TYPE) (cl_program program, const char *kernel_name, cl_int *errcode_ret);
typedef cl_int (CL_API_CALL *clSetKernelArg_TYPE) (cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void *arg_value);
typedef cl_int (CL_API_CALL *clEnqueueWriteBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb, const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clEnqueueReadBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb, void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clEnqueueNDRangeKernel_TYPE) (cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset, const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (CL_API_CALL *clReleaseKernel_TYPE) (cl_kernel kernel);
typedef cl_int (CL_API_CALL *clReleaseProgram_TYPE) (cl_program program);
typedef cl_int (CL_API_CALL *clGetProgramInfo_TYPE) (cl_program program, cl_program_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);

typedef cl_int (CL_API_CALL *clSetCommandQueueProperty_TYPE) (cl_command_queue command_queue, cl_command_queue_properties properties, cl_bool enable, cl_command_queue_properties *old_properties);
typedef cl_int (CL_API_CALL *clGetEventProfilingInfo_TYPE) (cl_event event, cl_profiling_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (CL_API_CALL *clEnqueueMarker_TYPE) (cl_command_queue command_queue, cl_event *event);
typedef cl_int (CL_API_CALL *clWaitForEvents_TYPE) (cl_uint num_events, const cl_event *event_list);

}

#endif


/*=====================================================================
OpenCL
------

=====================================================================*/
class OpenCL
{
public:
	/*=====================================================================
	OpenCL
	------
	
	=====================================================================*/

	// If desired_device_number < 0 it will automatically choose a device.
	OpenCL(int desired_device_number, bool verbose_init = true);

	~OpenCL();

	int getChosenDeviceNumber();

#if USE_OPENCL
	static const std::string errorString(cl_int result);

	std::vector<gpuDeviceInfo> getDeviceInfo() const;

	cl_program buildProgram(
		const std::vector<std::string>& program_lines,
		cl_device_id device,
		const std::string& compile_options
	);

	void dumpBuildLog(cl_program program);

//private:

	clGetPlatformIDs_TYPE clGetPlatformIDs;
	clGetPlatformInfo_TYPE clGetPlatformInfo;
	clGetDeviceIDs_TYPE clGetDeviceIDs;
	clGetDeviceInfo_TYPE clGetDeviceInfo;
	clCreateContextFromType_TYPE clCreateContextFromType;
	clReleaseContext_TYPE clReleaseContext;
	clCreateCommandQueue_TYPE clCreateCommandQueue;
	clReleaseCommandQueue_TYPE clReleaseCommandQueue;
	clCreateBuffer_TYPE clCreateBuffer;
	clCreateImage2D_TYPE clCreateImage2D;
	clReleaseMemObject_TYPE clReleaseMemObject;
	clCreateProgramWithSource_TYPE clCreateProgramWithSource;
	clBuildProgram_TYPE clBuildProgram;
	clGetProgramBuildInfo_TYPE clGetProgramBuildInfo;
	clCreateKernel_TYPE clCreateKernel;
	clSetKernelArg_TYPE clSetKernelArg;
	clEnqueueWriteBuffer_TYPE clEnqueueWriteBuffer;
	clEnqueueReadBuffer_TYPE clEnqueueReadBuffer;
	clEnqueueNDRangeKernel_TYPE clEnqueueNDRangeKernel;
	clReleaseKernel_TYPE clReleaseKernel;
	clReleaseProgram_TYPE clReleaseProgram;
	clGetProgramInfo_TYPE clGetProgramInfo;

	clSetCommandQueueProperty_TYPE clSetCommandQueueProperty;
	clGetEventProfilingInfo_TYPE clGetEventProfilingInfo;
	clEnqueueMarker_TYPE clEnqueueMarker;
	clWaitForEvents_TYPE clWaitForEvents;

	HMODULE module;
	cl_platform_id platform_to_use;
	cl_device_id device_to_use;
	cl_context context;
	cl_command_queue command_queue;
#endif

	int chosen_device_number;

	std::vector<gpuDeviceInfo> device_info;
};


#endif //__OPENCL_H_666_
