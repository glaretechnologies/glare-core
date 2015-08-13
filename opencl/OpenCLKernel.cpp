/*=====================================================================
OpenCLKernel.cpp
----------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "OpenCLKernel.h"


#if USE_OPENCL


#include "OpenCL.h"
#include "../indigo/globals.h"
#include "../utils/Platform.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


OpenCLKernel::OpenCLKernel(cl_program program, const std::string& kernel_name, cl_device_id opencl_device_id, bool profile_)
{
	kernel = 0;
	kernel_arg_index = 0;
	work_group_size = 1;
	work_group_size_multiple = 1;
	total_exec_time_s = 0;
	profile = profile_;

	createKernel(program, kernel_name, opencl_device_id);
}


OpenCLKernel::~OpenCLKernel()
{
	cl_int result = getGlobalOpenCL()->clReleaseKernel(kernel);
	if(result != CL_SUCCESS)
		conPrint("Warning: clReleaseKernel failed: " + OpenCL::errorString(result));
}


void OpenCLKernel::createKernel(cl_program program, const std::string& kernel_name_, cl_device_id opencl_device_id)
{
	kernel_name = kernel_name_;

	cl_int result;
	this->kernel = getGlobalOpenCL()->clCreateKernel(program, kernel_name.c_str(), &result);
	if(!this->kernel)
		throw Indigo::Exception("Failed to created kernel '" + kernel_name + "': " + OpenCL::errorString(result));

	// Query work-group size multiple.
	size_t worksize[3];
	result = getGlobalOpenCL()->clGetKernelWorkGroupInfo(
		this->kernel,
		opencl_device_id,
		CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, // param_name
		3 * sizeof(size_t), // param_value_size
		&worksize[0], // param_value
		NULL // param_value_size_ret. Can be null.
	);
	
	this->work_group_size_multiple = (result != CL_SUCCESS) ? 64 : worksize[0];

	this->work_group_size = this->work_group_size_multiple;

	conPrint("work group size multiple for kernel '" + kernel_name + "' = " + toString(this->work_group_size_multiple));
}


void OpenCLKernel::setWorkGroupSize(size_t work_group_size_)
{ 
	assert(work_group_size_ >= 1);
	work_group_size = work_group_size_;
}


void OpenCLKernel::setKernelArgUInt(size_t index, cl_uint val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_uint), &val);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgBuffer(size_t index, cl_mem buffer)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_mem), &buffer);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setNextKernelArg(cl_mem buffer)
{
	setKernelArgBuffer(kernel_arg_index, buffer);
	kernel_arg_index++;
}


double OpenCLKernel::launchKernel(cl_command_queue opencl_command_queue, size_t global_work_size)
{
	// Make sure the work group size we use is <= the global work size.
	const size_t use_work_group_size = myMin(work_group_size, global_work_size);

	//conPrint("launching kernel " + kernel_name);

	cl_event profile_event;
	cl_int result = getGlobalOpenCL()->clEnqueueNDRangeKernel(
		opencl_command_queue,
		this->kernel,
		1,					// dimension
		NULL,				// global_work_offset
		&global_work_size,	// global_work_size
		&use_work_group_size,	// local_work_size (work-group size),
		0,					// num_events_in_wait_list
		NULL,				// event_wait_list
		profile ? &profile_event : NULL		// event
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clEnqueueNDRangeKernel failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

	if(profile)
	{
		result = getGlobalOpenCL()->clWaitForEvents(1, &profile_event);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clWaitForEvents failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		cl_ulong time_start, time_end;
		result = getGlobalOpenCL()->clGetEventProfilingInfo(profile_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clGetEventProfilingInfo failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		result = getGlobalOpenCL()->clGetEventProfilingInfo(profile_event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clGetEventProfilingInfo failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		const double elapsed_ns = (double)time_end - (double)time_start;
		const double elapsed_s = elapsed_ns * 1.0e-9;
		total_exec_time_s += elapsed_s;
		return elapsed_s;
	}
	else
		return 0;
}


#endif // USE_OPENCL
