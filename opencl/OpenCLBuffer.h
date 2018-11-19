/*=====================================================================
OpenCLBuffer.h
--------------
Copyright Glare Technologies Limited 2016 -
Generated at Tue May 15 13:27:16 +0100 2012
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "OpenCL.h"


/*=====================================================================
OpenCLBuffer
------------

=====================================================================*/
class OpenCLBuffer
{
public:
	OpenCLBuffer();
	OpenCLBuffer(cl_context context, size_t size_, cl_mem_flags flags);
	~OpenCLBuffer();


	void alloc(cl_context context, size_t size_, cl_mem_flags flags);

	// If existing buffer is wrong size, or is not allocated yet, (re)allocate.
	void allocOrResize(cl_context context, size_t size_, cl_mem_flags flags);

	// If existing buffer is wrong size, or is not allocated yet, (re)allocate.
	// Then copy data from host buffer (in src_ptr) to device buffer.
	void allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const void* const src_ptr, size_t new_size, cl_mem_flags flags, bool blocking_write);

	template<typename T, size_t align>
	void allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const js::Vector<T, align>& src_vec, cl_mem_flags flags, bool blocking_write);

	template<typename T>
	void allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const std::vector<T>& src_vec, cl_mem_flags flags, bool blocking_write);


	// CL_MEM_COPY_HOST_PTR will be added to flags in all allocFrom() definitions.
	void allocFrom(cl_context context, const void* const src_ptr, size_t size_, cl_mem_flags flags);

	template<typename T, size_t align>
	void allocFrom(cl_context context, const js::Vector<T, align>& src_vec, cl_mem_flags flags);

	template<typename T>
	void allocFrom(cl_context context, const std::vector<T>& src_vec, cl_mem_flags flags);


	// Copy data from a host buffer to a device buffer.  Calls clEnqueueWriteBuffer().
	void copyFrom(cl_command_queue command_queue, const void* const src_ptr, size_t size_, bool blocking_write);

	void copyFrom(cl_command_queue command_queue, size_t dest_offset, const void* const src_ptr, size_t size_, bool blocking_write);

	template<typename T, size_t align>
	void copyFrom(cl_command_queue command_queue, const js::Vector<T, align>& src_vec, bool blocking_write);

	template<typename T>
	void copyFrom(cl_command_queue command_queue, const std::vector<T>& src_vec, bool blocking_write);


	// Copy data from a device buffer back to a host buffer.  Calls clEnqueueReadBuffer().
	void readTo(cl_command_queue command_queue, void* const dest_ptr, size_t size_, bool blocking_read);


	size_t getSize() const { return size; }

	void free();

	cl_mem getDevicePtr() { return opencl_mem; }

	cl_uint getRefCount();

private:
	INDIGO_DISABLE_COPY(OpenCLBuffer)

	size_t size;

	cl_mem opencl_mem;
};


template<typename T, size_t align>
void OpenCLBuffer::allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const js::Vector<T, align>& src_vec, cl_mem_flags flags_, bool blocking_write)
{
	allocOrResizeAndCopyFrom(context, command_queue, src_vec.data(), src_vec.size() * sizeof(T), flags_, blocking_write);
}

template<typename T>
void OpenCLBuffer::allocOrResizeAndCopyFrom(cl_context context, cl_command_queue command_queue, const std::vector<T>& src_vec, cl_mem_flags flags_, bool blocking_write)
{
	allocOrResizeAndCopyFrom(context, command_queue, src_vec.data(), src_vec.size() * sizeof(T), flags_, blocking_write);
}


template<typename T, size_t align>
void OpenCLBuffer::allocFrom(cl_context context, const js::Vector<T, align>& src_vec, cl_mem_flags flags_)
{
	allocFrom(context, src_vec.data(), src_vec.size() * sizeof(T), flags_);
}

template<typename T>
void OpenCLBuffer::allocFrom(cl_context context, const std::vector<T>& src_vec, cl_mem_flags flags_)
{
	allocFrom(context, src_vec.data(), src_vec.size() * sizeof(T), flags_);
}


template<typename T, size_t align>
void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const js::Vector<T, align>& src_vec, bool blocking_write)
{
	copyFrom(command_queue, src_vec.data(), src_vec.size() * sizeof(T), blocking_write);
}

template<typename T>
void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const std::vector<T>& src_vec, bool blocking_write)
{
	copyFrom(command_queue, src_vec.data(), src_vec.size() * sizeof(T), blocking_write);
}
