/*=====================================================================
OpenCLCommandQueue.h
--------
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include "../utils/PrintOutput.h"
#include "../utils/Platform.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/DynamicLib.h"


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
class OpenCLCommandQueue : public ThreadSafeRefCounted
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

