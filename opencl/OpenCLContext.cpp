/*=====================================================================
OpenCLContext.cpp
-----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "OpenCLContext.h"


#include "OpenCL.h"
#include "ConPrint.h"


static std::vector<cl_context_properties> makePropertyList(cl_platform_id platform_id, bool enable_opengl_interop)
{
	std::vector<cl_context_properties> props;
	props.push_back(CL_CONTEXT_PLATFORM);
	props.push_back((cl_context_properties)platform_id);

#if OPENCL_OPENGL_INTEROP
	if(enable_opengl_interop)
	{
		if(getGlobalOpenCL()->current_gl_context == 0)
			throw glare::Exception("OpenCLContext: Current GL context is NULL when creating an OpenCL context with OpenGL interop");
		if(getGlobalOpenCL()->current_DC == 0)
			throw glare::Exception("OpenCLContext: Current window device context (HDC) is NULL when creating an OpenCL context with OpenGL interop");

		props.push_back(CL_GL_CONTEXT_KHR);
		props.push_back(getGlobalOpenCL()->current_gl_context);

		props.push_back(CL_WGL_HDC_KHR);
		props.push_back(getGlobalOpenCL()->current_DC);
	}
#endif

	props.push_back(0); // Terminating property

	return props;
}


OpenCLContext::OpenCLContext(cl_platform_id platform_id, bool enable_opengl_interop)
{
	std::vector<cl_context_properties> props = makePropertyList(platform_id, enable_opengl_interop);

	cl_int error_code;
	this->context = ::getGlobalOpenCL()->clCreateContextFromType(
		props.data(), // properties
		CL_DEVICE_TYPE_ALL, // device type
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);
	if(this->context == 0)
		throw glare::Exception("clCreateContextFromType failed: " + OpenCL::errorString(error_code));
}


OpenCLContext::OpenCLContext(OpenCLDeviceRef& opencl_device, bool enable_opengl_interop)
{
	std::vector<cl_context_properties> props = makePropertyList(opencl_device->opencl_platform_id, enable_opengl_interop);

	cl_int error_code;
	this->context = ::getGlobalOpenCL()->clCreateContext(
		props.data(), // properties
		1, // num devices
		&opencl_device->opencl_device_id, // devices
		NULL, // pfn notify
		NULL, // user data
		&error_code
	);
	if(this->context == 0)
		throw glare::Exception("clCreateContext failed: " + OpenCL::errorString(error_code));
}


OpenCLContext::~OpenCLContext()
{
	if(::getGlobalOpenCL()->clReleaseContext(context) != CL_SUCCESS)
	{
		assert(0);
	}
}
