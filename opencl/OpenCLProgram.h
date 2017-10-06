/*=====================================================================
OpenCLProgram.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


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
OpenCLProgram
-------------
An OpenCL program
=====================================================================*/
class OpenCLProgram : public ThreadSafeRefCounted
{
public:
	OpenCLProgram(cl_program program);
	~OpenCLProgram();

	cl_program getProgram() { return program; }


	void getProgramInfo(cl_program_info param_name, size_t param_value_size, void* param_value);

private:
	cl_program program;
};


typedef Reference<OpenCLProgram> OpenCLProgramRef;
