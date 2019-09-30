/*=====================================================================
OpenCLContext.cpp
-----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "OpenCLContext.h"


#include "OpenCL.h"
#include "ConPrint.h"


OpenCLContext::OpenCLContext(cl_platform_id platform_id)
{
	// Create context
	cl_context_properties cps[3] =
	{
		CL_CONTEXT_PLATFORM,
		(cl_context_properties)platform_id,
		0
	};

	cl_int error_code;
	this->context = ::getGlobalOpenCL()->clCreateContextFromType(
		cps, // properties
		CL_DEVICE_TYPE_ALL, // device type
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);
	if(this->context == 0)
		throw Indigo::Exception("clCreateContextFromType failed: " + OpenCL::errorString(error_code));
}


OpenCLContext::~OpenCLContext()
{
	if(::getGlobalOpenCL()->clReleaseContext(context) != CL_SUCCESS)
	{
		assert(0);
	}
}
