/*=====================================================================
OpenCLPlatform.cpp
------------------
=====================================================================*/
#include "OpenCLPlatform.h"


#include "OpenCL.h"


OpenCLPlatform::OpenCLPlatform(cl_platform_id platform_)
:	platform_id(platform_)
{
}


OpenCLPlatform::~OpenCLPlatform()
{
}
