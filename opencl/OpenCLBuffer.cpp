/*=====================================================================
OpenCLBuffer.cpp
----------------
Copyright Glare Technologies Limited 2011 -
Generated at Tue May 15 13:27:16 +0100 2012
=====================================================================*/
#include "OpenCLBuffer.h"


#if USE_OPENCL


#include "OpenCL.h"
#include "../indigo/globals.h"
#include "../utils/platform.h"
#include "../utils/Exception.h"
#include "../utils/stringutils.h"


#ifdef OPENCL_MEM_LOG
static size_t OpenCL_global_alloc = 0; // Total number of bytes allocated
//static size_t OpenCL_global_pinned_alloc = 0; // Total number of bytes allocated in pinned host memory
#endif


OpenCLBuffer::OpenCLBuffer(OpenCL& opencl_)
:	opencl(opencl_)
{
	// Initialise to null state
	size = 0;
	opencl_mem = NULL;
}


OpenCLBuffer::OpenCLBuffer(OpenCL& opencl_, size_t size_, cl_mem_flags flags)
:	opencl(opencl_)
{
	// Initialise to null state
	size = 0;
	opencl_mem = NULL;

	alloc(size_, flags);
}


OpenCLBuffer::~OpenCLBuffer()
{
	free();
}


void OpenCLBuffer::alloc(size_t size_, cl_mem_flags flags)
{
	if(opencl_mem)
		free();

	cl_int result;
	opencl_mem = opencl.clCreateBuffer(
		opencl.context,
		flags,
		size_, // size
		NULL, // host ptr
		&result
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateBuffer failed");

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

	if(opencl.clReleaseMemObject(opencl_mem) != CL_SUCCESS)
		throw Indigo::Exception("clReleaseMemObject failed");

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc -= size;
	conPrint("OpenCLBuffer::free(): size = " + getNiceByteSize(size) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif

	// Re-init to initial / null state
	size = 0;
	opencl_mem = NULL;
}


void OpenCLBuffer::allocFrom(const void * const src_ptr, size_t size_, cl_mem_flags flags)
{
	if(opencl_mem)
		free();

	cl_int result;
	opencl_mem = opencl.clCreateBuffer(
		opencl.context,
		flags | CL_MEM_COPY_HOST_PTR,
		size_, // size
		(void*)src_ptr, // host ptr
		&result
		);
	if(result != CL_SUCCESS)
		throw Indigo::Exception("clCreateBuffer failed");

	size = size_;

#ifdef OPENCL_MEM_LOG
	OpenCL_global_alloc += size_;
	conPrint("OpenCLBuffer::allocFrom(): size = " + getNiceByteSize(size_) + ", total = " + getNiceByteSize(OpenCL_global_alloc));
#endif
}


void OpenCLBuffer::copyFrom(const void * const src_ptr, size_t size_, cl_command_queue command_queue, cl_bool blocking_write)
{
	assert(size_ <= size);

	cl_int result = opencl.clEnqueueWriteBuffer(
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


//void OpenCLBuffer::copyFromAsync(const void * const src_ptr, size_t size_, CUstream& stream)
//{
//	assert(size_ <= size);
//
//	CUresult result = cuda.cuMemcpyHtoDAsync(cuda_device_ptr, src_ptr, size_, stream);
//	if(result != CUDA_SUCCESS)
//		throw Indigo::Exception("cuMemcpyHtoDAsync failed");
//}


void OpenCLBuffer::copyTo(void * const dst_ptr, size_t size_)
{
	assert(size_ <= size);

	//CUresult result = cuda.cuMemcpyDtoH(dst_ptr, cuda_device_ptr, size_);
	//if(result != CUDA_SUCCESS)
	//	throw Indigo::Exception("cuMemcpyDtoH failed");
}


//void OpenCLBuffer::copyToAsync(void * const dst_ptr, size_t size_, CUstream& stream)
//{
//	assert(size_ <= size);
//
//	CUresult result = cuda.cuMemcpyDtoHAsync(dst_ptr, cuda_device_ptr, size_, stream);
//	if(result != CUDA_SUCCESS)
//		throw Indigo::Exception("cuMemcpyDtoHAsync failed");
//}


cl_mem& OpenCLBuffer::getDevicePtr()
{
	assert(opencl_mem);
	return opencl_mem;
}


#endif // USE_OPENCL
