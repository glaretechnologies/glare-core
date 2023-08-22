/*=====================================================================
OpenCLContext.h
---------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "OpenCLDevice.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"

#ifdef OSX
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#endif


/*=====================================================================
OpenCLContext
-------------
An OpenCL context
=====================================================================*/
class OpenCLContext : public ThreadSafeRefCounted
{
public:
	OpenCLContext(cl_platform_id platform_id, bool enable_opengl_interop = false);
	OpenCLContext(OpenCLDeviceRef& opencl_device, bool enable_opengl_interop = false);
	~OpenCLContext();

	cl_context getContext() { return context; }

private:
	cl_context context;
};


typedef Reference<OpenCLContext> OpenCLContextRef;
