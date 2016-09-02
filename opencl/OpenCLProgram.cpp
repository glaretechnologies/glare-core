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
