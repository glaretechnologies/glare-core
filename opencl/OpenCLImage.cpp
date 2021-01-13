/*=====================================================================
OpenCLImage.cpp
---------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "OpenCLImage.h"


#include "OpenCL.h"
#include "../indigo/globals.h"
#include "../utils/Platform.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


OpenCLImage::OpenCLImage()
{
	// Initialise to null state
	opencl_image = NULL;
}


OpenCLImage::OpenCLImage(cl_context context, const cl_image_format* image_format, const cl_image_desc* image_desc, cl_mem_flags flags,
	const void* host_ptr)
{
	// Initialise to null state
	opencl_image = NULL;

	alloc(context, image_format, image_desc, flags, host_ptr);
}


OpenCLImage::~OpenCLImage()
{
	free();
}


void OpenCLImage::alloc(cl_context context, const cl_image_format* image_format, const cl_image_desc* image_desc, cl_mem_flags flags,
	const void* host_ptr)
{
	if(opencl_image)
		free();

	cl_int result;
	opencl_image = getGlobalOpenCL()->clCreateImage(
		context,
		flags,
		image_format,
		image_desc,
		(void*)host_ptr, // host ptr
		&result
	);
	if(result != CL_SUCCESS)
		throw glare::Exception("clCreateImage failed: " + OpenCL::errorString(result));
}


void OpenCLImage::free()
{
	if(!opencl_image)
		return;

	cl_int result = getGlobalOpenCL()->clReleaseMemObject(opencl_image);
	if(result != CL_SUCCESS)
		throw glare::Exception("clReleaseMemObject failed: " + OpenCL::errorString(result));

	// Re-init to initial / null state
	opencl_image = NULL;
}



cl_mem OpenCLImage::getOpenCLImage()
{
	return opencl_image;
}
