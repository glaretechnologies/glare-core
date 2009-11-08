/*=====================================================================
OpenCL.h
--------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OPENCL_H_666_
#define __OPENCL_H_666_


#if USE_OPENCL


#include <CL/cl.h>
#include <CL/clext.h>


typedef cl_int (*clGetPlatformIDs_TYPE) (cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
typedef cl_int (*clGetPlatformInfo_TYPE) (cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_int (*clGetDeviceIDs_TYPE) (cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
typedef cl_int (*clGetDeviceInfo_TYPE) (cl_device_id device, cl_device_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_context (*clCreateContextFromType_TYPE) (cl_context_properties *properties, cl_device_type device_type, void (*pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret);
typedef cl_int (*clReleaseContext_TYPE) (cl_context context);
typedef cl_command_queue (*clCreateCommandQueue_TYPE) (cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int *errcode_ret);
typedef cl_int (*clReleaseCommandQueue_TYPE) (cl_command_queue command_queue);
typedef cl_mem (*clCreateBuffer_TYPE) (cl_context context, cl_mem_flags flags, size_t size, void *host_ptr, cl_int *errcode_ret);
typedef cl_int (*clReleaseMemObject_TYPE) (cl_mem memobj);
typedef cl_program (*clCreateProgramWithSource_TYPE) (cl_context context, cl_uint count, const char **strings, const size_t *lengths, cl_int *errcode_ret);
typedef cl_int (*clBuildProgram_TYPE) (cl_program program, cl_uint num_devices, const cl_device_id *device_list, const char *options, void (*pfn_notify)(cl_program, void *user_data), void *user_data);
typedef cl_int (*clGetProgramBuildInfo_TYPE) (cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret);
typedef cl_kernel (*clCreateKernel_TYPE) (cl_program program, const char *kernel_name, cl_int *errcode_ret);
typedef cl_int (*clSetKernelArg_TYPE) (cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void *arg_value);
typedef cl_int (*clEnqueueWriteBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t cb, const void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (*clEnqueueReadBuffer_TYPE) (cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t cb, void *ptr, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);
typedef cl_int (*clEnqueueNDRangeKernel_TYPE) (cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t *global_work_offset, const size_t *global_work_size, const size_t *local_work_size, cl_uint num_events_in_wait_list, const cl_event *event_wait_list, cl_event *event);


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
	OpenCL();

	~OpenCL();

//private:
#if USE_OPENCL
	clGetPlatformIDs_TYPE clGetPlatformIDs;
	clGetPlatformInfo_TYPE clGetPlatformInfo;
	clGetDeviceIDs_TYPE clGetDeviceIDs;
	clGetDeviceInfo_TYPE clGetDeviceInfo;
	clCreateContextFromType_TYPE clCreateContextFromType;
	clReleaseContext_TYPE clReleaseContext;
	clCreateCommandQueue_TYPE clCreateCommandQueue;
	clReleaseCommandQueue_TYPE clReleaseCommandQueue;
	clCreateBuffer_TYPE clCreateBuffer;
	clReleaseMemObject_TYPE clReleaseMemObject;
	clCreateProgramWithSource_TYPE clCreateProgramWithSource;
	clBuildProgram_TYPE clBuildProgram;
	clGetProgramBuildInfo_TYPE clGetProgramBuildInfo;
	clCreateKernel_TYPE clCreateKernel;
	clSetKernelArg_TYPE clSetKernelArg;
	clEnqueueWriteBuffer_TYPE clEnqueueWriteBuffer;
	clEnqueueReadBuffer_TYPE clEnqueueReadBuffer;
	clEnqueueNDRangeKernel_TYPE clEnqueueNDRangeKernel;

	cl_device_id device_to_use_id;
	cl_context context;
	cl_command_queue command_queue;
#endif
};


#endif //__OPENCL_H_666_
