/*=====================================================================
OpenCL.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCLCommandQueue.h"


#if USE_OPENCL


#include "OpenCL.h"


OpenCLCommandQueue::OpenCLCommandQueue(OpenCL& open_cl_, cl_context context, cl_device_id device_id_)
	:	open_cl(open_cl_), device_id(device_id_)
{
	// Create command queue
	cl_int error_code;
	this->command_queue = open_cl.clCreateCommandQueue(
		context,
		device_id,
		0, // CL_QUEUE_PROFILING_ENABLE, // queue properties
		&error_code);

	if(command_queue == 0)
		throw Indigo::Exception("clCreateCommandQueue failed");
}


OpenCLCommandQueue::~OpenCLCommandQueue()
{
	// Free command queue
	if(this->command_queue)
	{
		if(open_cl.clReleaseCommandQueue(this->command_queue) != CL_SUCCESS)
		{
			// It's illegal to throw in a destructor.
			assert(0);
		}
	}
}


#endif // USE_OPENCL

