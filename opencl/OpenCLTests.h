/*=====================================================================
OpenCLTests.h
-------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


class gpuDeviceInfo;


/*=====================================================================
OpenCLTests
-----------
Runs some tests, written in OpenCL, in the kernel OpenCLPathTracingTestKernel.cl,
that test various things from the OpenCL code used in the OpenCL path tracer.
=====================================================================*/
class OpenCLTests
{
public:
	OpenCLTests();
	~OpenCLTests();

	static void test();

private:
	static void runTestsOnDevice(const gpuDeviceInfo& opencl_device);
};
