/*=====================================================================
OpenCLPlatform.h
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "OpenCLDevice.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <vector>


/*=====================================================================
OpenCLPlatform
--------------

=====================================================================*/
class OpenCLPlatform : public ThreadSafeRefCounted
{
public:
	OpenCLPlatform(cl_platform_id platform);
	~OpenCLPlatform();

	cl_platform_id getPlatformID() { return platform_id; }

	std::string name;
	std::string version;
	std::vector<OpenCLDeviceRef> devices;
private:
	cl_platform_id platform_id;
};


typedef Reference<OpenCLPlatform> OpenCLPlatformRef;
