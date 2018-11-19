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

	if(size_ > 0)
	{
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
	}

	size = size_;

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc += size_;
	conPrint("OpenCLBuffer::alloc(): size = " + getNiceByteSize(size_) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif
}


void OpenCLBuffer::allocOrResize(cl_context context, size_t new_size, cl_mem_flags flags)
{
	if(size != new_size) // If existing size is wrong (including if not allocated at all yet):
		alloc(context, new_size, flags);
}


void OpenCLBuffer::allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const void* const src_ptr, 
	size_t new_size, cl_mem_flags flags, bool blocking_write)
{
	allocOrResize(context, new_size, flags);
	copyFrom(command_queue, src_ptr, new_size, blocking_write);
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

	if(size_ > 0)
	{
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
	}

	size = size_;

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc += size;
	conPrint("OpenCLBuffer::allocFrom(): size = " + getNiceByteSize(size) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif
}


void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const void* const src_ptr, size_t size_, bool blocking_write)
{
	copyFrom(command_queue, /*dest offset=*/0, src_ptr, size_, blocking_write);
}


void OpenCLBuffer::copyFrom(cl_command_queue command_queue, size_t dest_offset, const void* const src_ptr, size_t size_, bool blocking_write)
{
	assert(size_ <= size);

	cl_int result = getGlobalOpenCL()->clEnqueueWriteBuffer(
		command_queue, // command queue
		opencl_mem, // device buffer
		blocking_write ? CL_TRUE : CL_FALSE, // blocking write
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


void OpenCLBuffer::readTo(cl_command_queue command_queue, void* const dest_ptr, size_t size_, bool blocking_read)
{
	assert(size_ <= size);

	cl_int result = getGlobalOpenCL()->clEnqueueReadBuffer(
		command_queue,
		opencl_mem, // device buffer
		blocking_read ? CL_TRUE : CL_FALSE, // blocking read
		0, // offset
		size_, // size in bytes.
		dest_ptr, // host buffer pointer
		0, // num events in wait list
		NULL, // wait list
		NULL // event
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));
}


cl_uint OpenCLBuffer::getRefCount()
{
	cl_uint count = 0;
	const cl_int result = getGlobalOpenCL()->clGetMemObjectInfo(opencl_mem, 
		CL_MEM_REFERENCE_COUNT,
		sizeof(cl_uint),
		&count,
		NULL
	);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clGetMemObjectInfo failed: " + OpenCL::errorString(result));
	return count;
}
