/*=====================================================================
OpenCLKernel.h
--------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


#if USE_OPENCL


#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "OpenCL.h"


/*=====================================================================
OpenCLKernel
------------

=====================================================================*/
class OpenCLKernel : public ThreadSafeRefCounted
{
public:
	OpenCLKernel(cl_program program, const std::string& kernel_name, cl_device_id opencl_device_id, bool profile);
	~OpenCLKernel();


	//NOTE: opencl_device_id is just used for querying work group size.
	void createKernel(cl_program program, const std::string& kernel_name, cl_device_id opencl_device_id);

	size_t getWorkGroupSizeMulitple() const { return work_group_size; }
	size_t getWorkGroupSize() const { return work_group_size; }
	void setWorkGroupSize(size_t work_group_size_);

	void setKernelArgInt(size_t index, cl_int val);
	void setKernelArgUInt(size_t index, cl_uint val);
	void setKernelArgFloat(size_t index, cl_float val);
	void setKernelArgBuffer(size_t index, cl_mem buffer);

	void setNextKernelArg(cl_mem buffer);
	void setNextKernelArgUInt(cl_uint val);

	void resetKernelArgIndex() { kernel_arg_index = 0; }
	size_t getNextKernelIndex() const { return kernel_arg_index; }

	// Returns execution time in seconds if profiling was enabled, or 0 otherwise.
	double launchKernel(cl_command_queue opencl_command_queue, size_t global_work_size);


	cl_kernel getKernel() { return kernel; }

	double getTotalExecTimeS() const { return total_exec_time_s; }
private:
	INDIGO_DISABLE_COPY(OpenCLKernel)

	std::string kernel_name;
	cl_kernel kernel;
	size_t work_group_size_multiple;
	size_t work_group_size;

	size_t kernel_arg_index;

	double total_exec_time_s;
	bool profile;
};


typedef Reference<OpenCLKernel> OpenCLKernelRef;


#endif // USE_OPENCL
