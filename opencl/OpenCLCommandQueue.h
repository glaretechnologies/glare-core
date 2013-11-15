/*=====================================================================
OpenCLCommandQueue.h
--------
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include "../indigo/PrintOutput.h"
#include "../utils/platform.h"
#include "../utils/DynamicLib.h"
#include "../indigo/gpuDeviceInfo.h"


#if USE_OPENCL


#ifdef OSX
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#endif


class OpenCL;


/*=====================================================================
OpenCLCommandQueue
------------------
A command queue for a device.
=====================================================================*/
class OpenCLCommandQueue : public RefCounted
{
public:
	OpenCLCommandQueue(OpenCL& open_cl, cl_context context, cl_device_id device_id);
	~OpenCLCommandQueue();


	cl_command_queue getCommandQueue() { return command_queue; }
	cl_device_id getDeviceID() { return device_id; }

private:
	OpenCL& open_cl;
	cl_device_id device_id;
	cl_command_queue command_queue;
};


#endif // USE_OPENCL

