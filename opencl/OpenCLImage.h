/*=====================================================================
OpenCLImage.h
-------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "OpenCL.h"


/*=====================================================================
OpenCLImage
-----------

=====================================================================*/
class OpenCLImage : public ThreadSafeRefCounted
{
public:
	OpenCLImage();
	OpenCLImage(cl_context context, const cl_image_format* image_format, const cl_image_desc* image_desc, cl_mem_flags flags,
		const void* host_ptr);
	~OpenCLImage();

	void alloc(cl_context context, const cl_image_format* image_format, const cl_image_desc* image_desc, cl_mem_flags flags,
		const void* host_ptr);

	void free();

	cl_mem getOpenCLImage();

private:
	GLARE_DISABLE_COPY(OpenCLImage)

	cl_mem opencl_image;
};

typedef Reference<OpenCLImage> OpenCLImageRef;
