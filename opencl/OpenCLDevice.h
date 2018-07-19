/*=====================================================================
OpenCLDevice.h
--------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <string>

#ifdef OSX
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#endif


class OpenCLPlatform;


class OpenCLDevice : public ThreadSafeRefCounted
{
public:
	const std::string description() const;

	OpenCLPlatform* platform;

	cl_device_id opencl_device_id;
	cl_platform_id opencl_platform_id;
	cl_device_type opencl_device_type;

	std::string device_name;
	std::string vendor_name;
	// id is a unique identifier given to identical devices (devices with same name and vendor).
	int64 id;

	size_t global_mem_size;
	size_t max_mem_alloc_size;
	size_t compute_units;
	size_t clock_speed;

	bool supports_GL_interop;
	bool supports_doubles; // Does this device support double-precision floating-point?
};


typedef Reference<OpenCLDevice> OpenCLDeviceRef;


struct OpenCLDeviceLessThanName
{
	inline bool operator() (const OpenCLDeviceRef& lhs, const OpenCLDeviceRef& rhs) const { return lhs->device_name < rhs->device_name; }
};


struct OpenCLDeviceLessThanVendor
{
	inline bool operator() (const OpenCLDeviceRef& lhs, const OpenCLDeviceRef& rhs) const { return lhs->vendor_name < rhs->vendor_name; }
};
