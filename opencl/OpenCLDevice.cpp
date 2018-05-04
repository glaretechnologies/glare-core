/*=====================================================================
OpenCLDevice.cpp
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "OpenCLDevice.h"


#include "OpenCL.h"
#include "../utils/StringUtils.h"


const std::string OpenCLDevice::description() const
{
	return "CPU: " + boolToString(opencl_device_type == CL_DEVICE_TYPE_CPU) +
		", device_name: " + device_name + ", OpenCL platform vendor: " + vendor_name +
		//", driver info: " + OpenCL_driver_info + ", driver version: " + OpenCL_version +
		", global_mem_size: " + getNiceByteSize(global_mem_size) +
		", max_mem_alloc_size: " + toString(max_mem_alloc_size) + " B";
}
