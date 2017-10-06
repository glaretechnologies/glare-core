/*=====================================================================
OpenCLProgram.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCLProgram.h"


#include "OpenCL.h"


OpenCLProgram::OpenCLProgram(cl_program program_)
:	program(program_)
{
	
}


OpenCLProgram::~OpenCLProgram()
{
	if(program)
	{
		cl_int res = ::getGlobalOpenCL()->clReleaseProgram(program);
		if(res != CL_SUCCESS)
		{
			assert(0);
		}
	}
}


void OpenCLProgram::getProgramInfo(cl_program_info param_name, size_t param_value_size, void* param_value)
{
	cl_int result = ::getGlobalOpenCL()->clGetProgramInfo(
		program,
		param_name,
		param_value_size, // param value size
		param_value, // param value
		NULL // param_value_size_ret
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clGetProgramInfo failed: " + OpenCL::errorString(result));
}
