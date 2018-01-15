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


	void allocFrom(cl_context context, const void* const src_ptr, size_t size_, cl_mem_flags flags);

	template<typename T, size_t align>
	void allocFrom(cl_context context, const js::Vector<T, align>& src_vec, cl_mem_flags flags);

	template<typename T>
	void allocFrom(cl_context context, const std::vector<T>& src_vec, cl_mem_flags flags);


	void copyFrom(cl_command_queue command_queue, const void* const src_ptr, size_t size_, cl_bool blocking_write);

	void copyFrom(cl_command_queue command_queue, size_t dest_offset, const void* const src_ptr, size_t size_, cl_bool blocking_write);

	template<typename T, size_t align>
	void copyFrom(cl_command_queue command_queue, const js::Vector<T, align>& src_vec, cl_bool blocking_write);

	template<typename T>
	void copyFrom(cl_command_queue command_queue, const std::vector<T>& src_vec, cl_bool blocking_write);

	size_t getSize() const { return size; }

	void free();

	cl_mem& getDevicePtr();


private:
	INDIGO_DISABLE_COPY(OpenCLBuffer)

	size_t size;

	cl_mem opencl_mem;
};


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
void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const js::Vector<T, align>& src_vec, cl_bool blocking_write)
{
	copyFrom(command_queue, src_vec.data(), src_vec.size() * sizeof(T), blocking_write);
}


template<typename T>
void OpenCLBuffer::copyFrom(cl_command_queue command_queue, const std::vector<T>& src_vec, cl_bool blocking_write)
{
	copyFrom(command_queue, src_vec.data(), src_vec.size() * sizeof(T), blocking_write);
}
