/*=====================================================================
OpenCLCommandQueue.h
--------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "OpenCLContext.h"
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
OpenCLCommandQueue
------------------
A command queue for a device.
=====================================================================*/
class OpenCLCommandQueue : public ThreadSafeRefCounted
{
public:
	OpenCLCommandQueue(OpenCLContextRef context, cl_device_id device_id);
	~OpenCLCommandQueue();


	cl_command_queue getCommandQueue() { return command_queue; }
	cl_device_id getDeviceID() { return device_id; }

private:
	cl_device_id device_id;
	cl_command_queue command_queue;
	OpenCLContextRef context; // Hang on to context, so the context will be destroyed after this object.
};


typedef Reference<OpenCLCommandQueue> OpenCLCommandQueueRef;
