/*=====================================================================
OpenGLMemoryObject.cpp
----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLMemoryObject.h"


#include "OpenGLExtensions.h"
#include "IncludeOpenGL.h"
#include <tracy/Tracy.hpp>


#if !EMSCRIPTEN && !defined(__APPLE__)
#define OPENGL_MEMORY_OBJECT_SUPPORT 1
#endif


OpenGLMemoryObject::OpenGLMemoryObject()
:	mem_obj(0)
{
#if OPENGL_MEMORY_OBJECT_SUPPORT
	glCreateMemoryObjectsEXT(1, &mem_obj);
#endif
}


OpenGLMemoryObject::~OpenGLMemoryObject()
{
#if OPENGL_MEMORY_OBJECT_SUPPORT
	glDeleteMemoryObjectsEXT(1, &mem_obj);
#endif
}


void OpenGLMemoryObject::importD3D11ImageFromHandle(void* shared_handle)
{
	ZoneScoped; // Tracy profiler

#ifdef _WIN32
	// NOTE: KMT, not GL_HANDLE_TYPE_D3D11_IMAGE_EXT: Direct3DUtils::getSharedHandleForTexture() returns a legacy shared handle.  See the comment in
	// Direct3DUtils::copyTextureToNewShareableTexture() for why we don't share with NT handles.
	glImportMemoryWin32HandleEXT(mem_obj, /*size (ignored)=*/0, GL_HANDLE_TYPE_D3D11_IMAGE_KMT_EXT, shared_handle);
#endif
}


// NOTE: this only works on a memory object imported from a resource created with a keyed mutex, and only where the driver exposes
// GL_EXT_win32_keyed_mutex, which AMD's don't.  The entry points are null when the extension is absent, so check before calling.
// Textures shared by Direct3DUtils don't use a keyed mutex, so this shouldn't be used on those.
OpenGLMemoryObjectLock::OpenGLMemoryObjectLock(Reference<OpenGLMemoryObject> mem_ob_)
:	mem_ob(mem_ob_)
{
#ifdef _WIN32
	if(glAcquireKeyedMutexWin32EXT)
		glAcquireKeyedMutexWin32EXT(/*memory=*/mem_ob->mem_obj, /*key=*/0, /*timeout=*/INFINITE);
#endif
}


OpenGLMemoryObjectLock::~OpenGLMemoryObjectLock()
{
#ifdef _WIN32
	if(glReleaseKeyedMutexWin32EXT)
		glReleaseKeyedMutexWin32EXT(mem_ob->mem_obj, 0);
#endif
}
