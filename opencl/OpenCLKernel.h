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
	OpenCLKernel(cl_program program, const std::string& kernel_name, cl_device_id opencl_device_id);
	~OpenCLKernel();


	//NOTE: opencl_device_id is just used for querying work group size.
	void createKernel(cl_program program, const std::string& kernel_name, cl_device_id opencl_device_id);


	void setKernelArgUInt(size_t index, cl_uint val);
	void setKernelArgBuffer(size_t index, cl_mem buffer);

	void setNextKernelArg(cl_mem buffer);
	size_t getNextKernelIndex() const { return kernel_arg_index; }


	void launchKernel(cl_command_queue opencl_command_queue, size_t global_work_size);


	cl_kernel getKernel() { return kernel; }

private:
	INDIGO_DISABLE_COPY(OpenCLKernel)

	std::string kernel_name;
	cl_kernel kernel;
	size_t work_size;

	size_t kernel_arg_index;
};


typedef Reference<OpenCLKernel> OpenCLKernelRef;


#endif // USE_OPENCL
