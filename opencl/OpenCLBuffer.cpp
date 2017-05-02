/*=====================================================================
OpenCLBuffer.cpp
----------------
Copyright Glare Technologies Limited 2011 -
Generated at Tue May 15 13:27:16 +0100 2012
=====================================================================*/
#include "OpenCLBuffer.h"


#include "OpenCL.h"
#include "../indigo/globals.h"
#include "../utils/Platform.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


#ifdef OPENCL_MEM_LOG
static size_t OpenCL_global_alloc = 0; // Total number of bytes allocated
//static size_t OpenCL_global_pinned_alloc = 0; // Total number of bytes allocated in pinned host memory
#endif


OpenCLBuffer::OpenCLBuffer()
{
	// Initialise to null state
	size = 0;
	opencl_mem = NULL;
}


OpenCLBuffer::OpenCLBuffer(cl_context context, size_t size_, cl_mem_flags flags)
{
	// Initialise to null state
	size = 0;
	opencl_mem = NULL;

	alloc(context, size_, flags);
}


OpenCLBuffer::~OpenCLBuffer()
{
	free();
}


void OpenCLBuffer::alloc(cl_context context, size_t size_, cl_mem_flags flags)
{
	if(opencl_mem)
		free();

	cl_int result;
	opencl_mem = getGlobalOpenCL()->clCreateBuffer(
		context,
		flags,
		size_, // size
		NULL, // host ptr
		&result
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateBuffer failed: " + OpenCL::errorString(result));

	size = size_;

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc += size_;
	conPrint("OpenCLBuffer::alloc(): size = " + getNiceByteSize(size_) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif
}


void OpenCLBuffer::free()
{
	if(!opencl_mem)
		return;

	cl_int result = getGlobalOpenCL()->clReleaseMemObject(opencl_mem);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clReleaseMemObject failed: " + OpenCL::errorString(result));

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc -= size;
	conPrint("OpenCLBuffer::free(): size = " + getNiceByteSize(size) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif

	// Re-init to initial / null state
	size = 0;
	opencl_mem = NULL;
}


void OpenCLBuffer::allocFrom(cl_context context, const void* const src_ptr, size_t size_, cl_mem_flags flags)
{
	if(opencl_mem)
		free();

	cl_int result;
	opencl_mem = getGlobalOpenCL()->clCreateBuffer(
		context,
		flags | CL_MEM_COPY_HOST_PTR,
		size_, // size
		(void*)src_ptr, // host ptr
		&result
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateBuffer failed: " + OpenCL::errorString(result));

	size = size_;

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc += size;
	conPrint("OpenCLBuffer::allocFrom(): size = " + getNiceByteSize(size) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif
}


void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const void* const src_ptr, size_t size_, cl_bool blocking_write)
{
	assert(size_ <= size);

	cl_int result = getGlobalOpenCL()->clEnqueueWriteBuffer(
		command_queue, // command queue
		opencl_mem, // buffer
		blocking_write, // blocking write
		0, // offset
		size_, // size in bytes
		(void*)src_ptr, // host buffer pointer
		0, // num events in wait list
		NULL, // wait list
		NULL // event
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clEnqueueWriteBuffer failed: " + OpenCL::errorString(result));
}


void OpenCLBuffer::copyFrom(cl_command_queue command_queue, size_t dest_offset, const void* const src_ptr, size_t size_, cl_bool blocking_write)
{
	assert(size_ <= size);

	cl_int result = getGlobalOpenCL()->clEnqueueWriteBuffer(
		command_queue, // command queue
		opencl_mem, // buffer
		blocking_write, // blocking write
		dest_offset, // offset
		size_, // size in bytes
		(void*)src_ptr, // host buffer pointer
		0, // num events in wait list
		NULL, // wait list
		NULL // event
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clEnqueueWriteBuffer failed: " + OpenCL::errorString(result));
}


cl_mem& OpenCLBuffer::getDevicePtr()
{
	return opencl_mem;
}
