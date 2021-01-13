/*=====================================================================
OpenCLKernel.cpp
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "OpenCLKernel.h"


#include "OpenCL.h"
#include "OpenCLBuffer.h"
#include "../utils/ConPrint.h"
#include "../utils/Platform.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


OpenCLKernel::OpenCLKernel(OpenCLProgramRef program, const std::string& kernel_name, cl_device_id opencl_device_id, bool profile_)
{
	kernel = 0;
	kernel_arg_index = 0;
	work_group_size[0] = work_group_size[1] = work_group_size[2] = 1;
	use_explicit_work_group_size = true;
	work_group_size_multiple = 1;
	total_exec_time_s = 0;
	profile = profile_;
	used_program = program;

	createKernel(program, kernel_name, opencl_device_id);
}


OpenCLKernel::~OpenCLKernel()
{
	cl_int result = getGlobalOpenCL()->clReleaseKernel(kernel);
	if(result != CL_SUCCESS)
		conPrint("Warning: clReleaseKernel failed: " + OpenCL::errorString(result));
}


void OpenCLKernel::createKernel(OpenCLProgramRef program, const std::string& kernel_name_, cl_device_id opencl_device_id)
{
	kernel_name = kernel_name_;
	used_program = program;

	cl_int result;
	this->kernel = getGlobalOpenCL()->clCreateKernel(program->getProgram(), kernel_name.c_str(), &result);
	if(!this->kernel)
		throw glare::Exception("Failed to created kernel '" + kernel_name + "': " + OpenCL::errorString(result));

	// Query work-group size multiple.  This is the "preferred multiple of workgroup size for launch", a performance hint.
	size_t size_multiple;
	result = getGlobalOpenCL()->clGetKernelWorkGroupInfo(
		this->kernel,
		opencl_device_id,
		CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, // param_name
		sizeof(size_t), // param_value_size
		&size_multiple, // param_value
		NULL // param_value_size_ret. Can be null.
	);
	
	this->work_group_size_multiple = (result != CL_SUCCESS) ? 64 : size_multiple;

	// Guess a good default work group size.  
	// NOTE: this could be improved for dimensions > 1, where we want the product of work group sizes for each dimension to be a multiple of work_group_size_multiple,
	// And maybe want a non-flat work group size.
	this->work_group_size[0] = this->work_group_size_multiple;
	this->work_group_size[1] = 1;
	this->work_group_size[2] = 1;

	// conPrint("work group size multiple for kernel '" + kernel_name + "' = " + toString(this->work_group_size_multiple));
}


void OpenCLKernel::setWorkGroupSize(size_t work_group_size_)
{ 
	assert(work_group_size_ >= 1);
	work_group_size[0] = work_group_size_;
	work_group_size[1] = 1;
	work_group_size[2] = 1;
}


void OpenCLKernel::setWorkGroupSize2D(size_t work_group_size_x, size_t work_group_size_y)
{ 
	assert(work_group_size_x >= 1);
	assert(work_group_size_y >= 1);
	work_group_size[0] = work_group_size_x;
	work_group_size[1] = work_group_size_y;
	work_group_size[2] = 1;
}


void OpenCLKernel::setWorkGroupSize3D(size_t work_group_size_x, size_t work_group_size_y, size_t work_group_size_z)
{
	assert(work_group_size_x >= 1);
	assert(work_group_size_y >= 1);
	assert(work_group_size_z >= 1);
	work_group_size[0] = work_group_size_x;
	work_group_size[1] = work_group_size_y;
	work_group_size[2] = work_group_size_z;
}


void OpenCLKernel::setKernelArgInt(size_t index, cl_int val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_int), &val);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgUInt(size_t index, cl_uint val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_uint), &val);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgULong(size_t index, cl_ulong val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_ulong), &val);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgFloat(size_t index, cl_float val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_float), &val);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgDouble(size_t index, cl_double val)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_double), &val);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setKernelArgBuffer(size_t index, OpenCLBuffer& buffer)
{
	setKernelArgBuffer(index, buffer.getDevicePtr());
}


void OpenCLKernel::setKernelArgBuffer(size_t index, cl_mem buffer)
{
	cl_int result = getGlobalOpenCL()->clSetKernelArg(this->kernel, (cl_uint)index, sizeof(cl_mem), &buffer);
	if(result != CL_SUCCESS)
		throw glare::Exception("Failed to set kernel arg for kernel '" + kernel_name + "', index " + toString(index) + ": " + OpenCL::errorString(result));
}


void OpenCLKernel::setNextKernelArg(OpenCLBuffer& buffer)
{
	setNextKernelArg(buffer.getDevicePtr());
}


void OpenCLKernel::setNextKernelArg(cl_mem buffer)
{
	setKernelArgBuffer(kernel_arg_index, buffer);
	kernel_arg_index++;
}


void OpenCLKernel::setNextKernelArgInt(cl_int val)
{
	setKernelArgInt(kernel_arg_index, val);
	kernel_arg_index++;
}


void OpenCLKernel::setNextKernelArgUInt(cl_uint val)
{
	setKernelArgUInt(kernel_arg_index, val);
	kernel_arg_index++;
}


void OpenCLKernel::setNextKernelArgFloat(cl_float val)
{
	setKernelArgFloat(kernel_arg_index, val);
	kernel_arg_index++;
}


// Returns execution time in seconds if profiling was enabled, or 0 otherwise.
double OpenCLKernel::doLaunchKernel(cl_command_queue opencl_command_queue, int dim, const size_t* global_work_size)
{
	// conPrint("launching kernel " + kernel_name);

	// Make sure the work group size we use is <= the global work size.
	size_t use_local_work_size[3];
	for(int i=0; i<dim; ++i)
		use_local_work_size[i] = myMin(work_group_size[i], global_work_size[i]);

	cl_event profile_event;
	cl_int result = getGlobalOpenCL()->clEnqueueNDRangeKernel(
		opencl_command_queue,
		this->kernel,
		dim,					// dimension
		NULL,					// global_work_offset
		global_work_size,		// global_work_size
		use_explicit_work_group_size ? use_local_work_size : NULL,	// local_work_size (work-group size),
		0,						// num_events_in_wait_list
		NULL,					// event_wait_list
		profile ? &profile_event : NULL		// event
	);
	if(result != CL_SUCCESS)
		throw glare::Exception("clEnqueueNDRangeKernel failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

	if(profile)
	{
		result = getGlobalOpenCL()->clWaitForEvents(1, &profile_event);
		if(result != CL_SUCCESS)
			throw glare::Exception("clWaitForEvents failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		cl_ulong time_start, time_end;
		result = getGlobalOpenCL()->clGetEventProfilingInfo(profile_event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
		if(result != CL_SUCCESS)
			throw glare::Exception("clGetEventProfilingInfo failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		result = getGlobalOpenCL()->clGetEventProfilingInfo(profile_event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
		if(result != CL_SUCCESS)
			throw glare::Exception("clGetEventProfilingInfo failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		result = getGlobalOpenCL()->clReleaseEvent(profile_event);
		if(result != CL_SUCCESS)
			throw glare::Exception("clReleaseEvent failed for kernel '" + kernel_name + "': " + OpenCL::errorString(result));

		const double elapsed_ns = (double)time_end - (double)time_start;
		const double elapsed_s = elapsed_ns * 1.0e-9;

		//conPrint(rightSpacePad("Kernel " + kernel_name + " exec took ", 50) + doubleToStringNSigFigs(elapsed_s * 1.0e3, 5) + " ms");
		total_exec_time_s += elapsed_s;
		return elapsed_s;
	}
	else
		return 0;
}


double OpenCLKernel::launchKernel(cl_command_queue opencl_command_queue, size_t global_work_size)
{
	return doLaunchKernel(opencl_command_queue, /*dim=*/1, &global_work_size);
}


double OpenCLKernel::launchKernel2D(cl_command_queue opencl_command_queue, size_t global_work_size_w, size_t global_work_size_h)
{
	const size_t dims[2] = { global_work_size_w , global_work_size_h };
	return doLaunchKernel(opencl_command_queue, /*dim=*/2, dims);
}


double OpenCLKernel::launchKernel3D(cl_command_queue opencl_command_queue, size_t global_work_size_w, size_t global_work_size_h, size_t global_work_size_d)
{
	const size_t dims[3] = { global_work_size_w , global_work_size_h, global_work_size_d };
	return doLaunchKernel(opencl_command_queue, /*dim=*/3, dims);
}
