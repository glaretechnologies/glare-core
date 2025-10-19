/*=====================================================================
OpenGLMemoryObject.cpp
----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLMemoryObject.h"


#include "OpenGLExtensions.h"
#include "IncludeOpenGL.h"


OpenGLMemoryObject::OpenGLMemoryObject()
:	mem_obj(0)
{
	glCreateMemoryObjectsEXT(1, &mem_obj);
}


OpenGLMemoryObject::~OpenGLMemoryObject()
{
	glDeleteMemoryObjectsEXT(1, &mem_obj);
}


void OpenGLMemoryObject::importD3D11ImageFromHandle(void* shared_handle)
{
#ifdef _WIN32
	glImportMemoryWin32HandleEXT(mem_obj, /*size (ignored)=*/0, GL_HANDLE_TYPE_D3D11_IMAGE_EXT, shared_handle);
#endif
}


OpenGLMemoryObjectLock::OpenGLMemoryObjectLock(Reference<OpenGLMemoryObject> mem_ob_)
:	mem_ob(mem_ob_)
{
#ifdef _WIN32
	glAcquireKeyedMutexWin32EXT(/*memory=*/mem_ob->mem_obj, /*key=*/0, /*timeout=*/INFINITE);
#endif
}


OpenGLMemoryObjectLock::~OpenGLMemoryObjectLock()
{
#ifdef _WIN32
	glReleaseKeyedMutexWin32EXT(mem_ob->mem_obj, 0);
#endif
}
