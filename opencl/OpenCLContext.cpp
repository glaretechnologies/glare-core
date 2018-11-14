/*=====================================================================
OpenCLContext.cpp
-----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "OpenCLContext.h"


#include "OpenCL.h"
#include "OpenCLProgramCache.h"
#include "ConPrint.h"


OpenCLContext::OpenCLContext(cl_platform_id platform_id)
{
	// Create command queue
	cl_context_properties cps[3] =
    {
        CL_CONTEXT_PLATFORM,
		(cl_context_properties)platform_id,
        0
    };

	cl_int error_code;

	this->context = ::getGlobalOpenCL()->clCreateContextFromType(
		cps, // properties
		CL_DEVICE_TYPE_ALL, // CL_DEVICE_TYPE_CPU, // TEMP CL_DEVICE_TYPE_GPU, // device type
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);
	if(this->context == 0)
		throw Indigo::Exception("clCreateContextFromType failed: " + OpenCL::errorString(error_code));

	program_cache = new OpenCLProgramCache();
}


OpenCLContext::~OpenCLContext()
{
	program_cache = NULL; // Destroy program cache

	if(::getGlobalOpenCL()->clReleaseContext(context) != CL_SUCCESS)
	{
		assert(0);
	}
}
