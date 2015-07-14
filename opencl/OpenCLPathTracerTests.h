/*=====================================================================
OpenCLPathTracerTests.h
-----------------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#pragma once


class gpuDeviceInfo;


/*=====================================================================
OpenCLPathTracerTests
---------------------
Runs some tests, written in OpenCL, in the kernel OpenCLPathTracingTestKernel.cl,
that test various things from the OpenCL code used in the OpenCL path tracer.
=====================================================================*/
class OpenCLPathTracerTests
{
public:
	OpenCLPathTracerTests();
	~OpenCLPathTracerTests();

	static void test();

private:
	static void runTestsOnDevice(const gpuDeviceInfo& opencl_device);
};
