/*=====================================================================
OpenCLBuffer.h
--------------
Copyright Glare Technologies Limited 2011 -
Generated at Tue May 15 13:27:16 +0100 2012
=====================================================================*/
#pragma once


#if USE_OPENCL


#include "../utils/Vector.h"
#include "OpenCL.h"


/*=====================================================================
OpenCLBuffer
------------

=====================================================================*/
class OpenCLBuffer
{
public:
	OpenCLBuffer(OpenCL& opencl_);
	OpenCLBuffer(OpenCL& opencl_, size_t size_, cl_mem_flags flags);
	~OpenCLBuffer();


	void alloc(size_t size_, cl_mem_flags flags);

	void allocFrom(const void * const src_ptr, size_t size_, cl_mem_flags flags);

	template<typename T, size_t align>
	void allocFrom(const js::Vector<T, align>& src_vec, cl_mem_flags flags);

	template<typename T>
	void allocFrom(const std::vector<T>& src_vec, cl_mem_flags flags);


	void copyFrom(const void * const src_ptr, size_t size_, cl_command_queue command_queue, cl_bool blocking_write);

	template<typename T, size_t align>
	void copyFrom(const js::Vector<T, align>& src_vec, cl_command_queue command_queue, cl_bool blocking_write);

	template<typename T>
	void copyFrom(const std::vector<T>& src_vec, cl_command_queue command_queue, cl_bool blocking_write);

	//void copyFromAsync(const void * const src_ptr, size_t size_, CUstream& stream);


	void copyTo(void * const dst_ptr, size_t size_);
	//void copyToAsync(void * const dst_ptr, size_t size_, CUstream& stream);

	void free();

	cl_mem& getDevicePtr();


private:

	// No copy allowed
	OpenCLBuffer(const OpenCLBuffer& c);
	OpenCLBuffer& operator = (const OpenCLBuffer& c);


	OpenCL& opencl;

	size_t size;

	cl_mem opencl_mem;
};


template<typename T, size_t align>
void OpenCLBuffer::allocFrom(const js::Vector<T, align>& src_vec, cl_mem_flags flags_)
{
	allocFrom(&src_vec[0], src_vec.size() * sizeof(T), flags_);
}


template<typename T>
void OpenCLBuffer::allocFrom(const std::vector<T>& src_vec, cl_mem_flags flags_)
{
	allocFrom(&src_vec[0], src_vec.size() * sizeof(T), flags_);
}


template<typename T, size_t align>
void OpenCLBuffer::copyFrom(const js::Vector<T, align>& src_vec, cl_command_queue command_queue, cl_bool blocking_write)
{
	copyFrom(&src_vec[0], src_vec.size() * sizeof(T), command_queue, blocking_write);
}


template<typename T>
void OpenCLBuffer::copyFrom(const std::vector<T>& src_vec, cl_command_queue command_queue, cl_bool blocking_write)
{
	copyFrom(&src_vec[0], src_vec.size() * sizeof(T), command_queue, blocking_write);
}


#endif // USE_OPENCL
