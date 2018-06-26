/*=====================================================================
OpenCLProgram.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <vector>


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

	
	struct Binary
	{
		cl_device_id device_id; // Device the program binary is for.
		std::vector<uint8> data; // Binary data
	};

	// Get binaries for devices associated with this program.
	void getProgramBinaries(std::vector<Binary>& binaries_out);

private:
	cl_program program;
};


typedef Reference<OpenCLProgram> OpenCLProgramRef;
