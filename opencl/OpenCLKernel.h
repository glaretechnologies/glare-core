/*=====================================================================
OpenCLKernel.h
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "OpenCL.h"
#include "OpenCLProgram.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class OpenCLBuffer;


/*=====================================================================
OpenCLKernel
------------

=====================================================================*/
class OpenCLKernel : public ThreadSafeRefCounted
{
public:
	OpenCLKernel(OpenCLProgramRef program, const std::string& kernel_name, cl_device_id opencl_device_id, bool profile);
	~OpenCLKernel();


	//NOTE: opencl_device_id is just used for querying work group size.
	void createKernel(OpenCLProgramRef program, const std::string& kernel_name, cl_device_id opencl_device_id);

	size_t getWorkGroupSizeMulitple() const { return work_group_size_multiple; }
	size_t getWorkGroupSize() const { return work_group_size[0]; }
	void setWorkGroupSize(size_t work_group_size_);
	void setWorkGroupSize2D(size_t work_group_size_x, size_t work_group_size_y);
	void setWorkGroupSize3D(size_t work_group_size_x, size_t work_group_size_y, size_t work_group_size_z);

	// If set to false, the driver is free to pick a work group size.
	// use_explicit_work_group_size is true by default.
	void setUseExplicitWorkGroupSize(bool use_explicit) { use_explicit_work_group_size = use_explicit; } 

	void setKernelArgInt(size_t index, cl_int val);
	void setKernelArgUInt(size_t index, cl_uint val);
	void setKernelArgULong(size_t index, cl_ulong val);
	void setKernelArgFloat(size_t index, cl_float val);
	void setKernelArgDouble(size_t index, cl_double val);
	void setKernelArgBuffer(size_t index, OpenCLBuffer& buffer);
	void setKernelArgBuffer(size_t index, cl_mem buffer);

	void setNextKernelArg(OpenCLBuffer& buffer);
	void setNextKernelArg(cl_mem buffer);
	void setNextKernelArgInt(cl_int val);
	void setNextKernelArgUInt(cl_uint val);
	void setNextKernelArgFloat(cl_float val);

	void resetKernelArgIndex() { kernel_arg_index = 0; }
	size_t getNextKernelArgIndex() const { return kernel_arg_index; }

	// Returns execution time in seconds if profiling was enabled, or 0 otherwise.
	double launchKernel(cl_command_queue opencl_command_queue, size_t global_work_size);

	double launchKernel2D(cl_command_queue opencl_command_queue, size_t global_work_size_w, size_t global_work_size_h);

	double launchKernel3D(cl_command_queue opencl_command_queue, size_t global_work_size_w, size_t global_work_size_h, size_t global_work_size_d);


	cl_kernel getKernel() { return kernel; }

	double getTotalExecTimeS() const { return total_exec_time_s; }
private:
	double doLaunchKernel(cl_command_queue opencl_command_queue, int dim, const size_t* global_work_size);
	GLARE_DISABLE_COPY(OpenCLKernel)

	std::string kernel_name;
	cl_kernel kernel;
	size_t work_group_size_multiple;
	size_t work_group_size[3];
	bool use_explicit_work_group_size;

	size_t kernel_arg_index;

	double total_exec_time_s;
	bool profile;

	OpenCLProgramRef used_program; // Hang on to program, so the program will be destroyed after this object.
};


typedef Reference<OpenCLKernel> OpenCLKernelRef;
