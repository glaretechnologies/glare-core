/*=====================================================================
OpenCLProgram.cpp
----------
File created by ClassTemplate on Mon Nov 02 17:13:50 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "OpenCLProgram.h"


#include "OpenCL.h"


OpenCLProgram::OpenCLProgram(cl_program program_, OpenCLContextRef& context_)
:	program(program_),
	context(context_)
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


void OpenCLProgram::getProgramBinaries(std::vector<Binary>& binaries_out)
{
	cl_uint num_devices; // number of devices associated with program.
	getProgramInfo(CL_PROGRAM_NUM_DEVICES,
		sizeof(num_devices), // param value size
		&num_devices // param value
	);

	std::vector<cl_device_id> queried_device_ids(num_devices); // The list of devices associated with the program object.
	getProgramInfo(CL_PROGRAM_DEVICES, 
		queried_device_ids.size() * sizeof(cl_device_id), // param value size
		queried_device_ids.data() // param value
	);

	// Get binary sizes
	std::vector<size_t> binary_sizes(num_devices); // Size in bytes of the program binary for each device associated with program.
	getProgramInfo(CL_PROGRAM_BINARY_SIZES,
		binary_sizes.size() * sizeof(size_t), // param value size
		binary_sizes.data() // param value
	);

	// Allocate space for binaries
	binaries_out.resize(num_devices);

	for(size_t i=0; i<binaries_out.size(); ++i)
	{
		binaries_out[i].device_id = queried_device_ids[i];
		binaries_out[i].data.resize(binary_sizes[i]);
	}

	std::vector<unsigned char*> binary_pointers(binaries_out.size());
	for(size_t i=0; i<binary_pointers.size(); ++i)
		binary_pointers[i] = (unsigned char*)binaries_out[i].data.data();

	// Get actual binaries
	getProgramInfo(CL_PROGRAM_BINARIES,
		binary_pointers.size() * sizeof(unsigned char*), // param value size
		binary_pointers.data() // param value
	);
}
