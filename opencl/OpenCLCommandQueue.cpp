/*=====================================================================
OpenCL.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCLCommandQueue.h"


#include "OpenCL.h"


OpenCLCommandQueue::OpenCLCommandQueue(OpenCLContextRef& context_, cl_device_id device_id_, bool enable_profiling)
:	context(context_), device_id(device_id_)
{
	// Create command queue
	cl_int error_code;
	this->command_queue = ::getGlobalOpenCL()->clCreateCommandQueue(
		context->getContext(),
		device_id,
		enable_profiling ? CL_QUEUE_PROFILING_ENABLE : 0, // queue properties
		&error_code);

	if(command_queue == 0)
		throw Indigo::Exception("clCreateCommandQueue failed: " + OpenCL::errorString(error_code));
}


OpenCLCommandQueue::~OpenCLCommandQueue()
{
	// Free command queue
	if(this->command_queue)
	{
		if(::getGlobalOpenCL()->clReleaseCommandQueue(this->command_queue) != CL_SUCCESS)
		{
			// It's illegal to throw in a destructor.
			assert(0);
		}
	}
}

