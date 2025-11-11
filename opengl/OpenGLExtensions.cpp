/*=====================================================================
OpenGLExtensions.cpp
--------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLExtensions.h"


// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt (GL_EXT_memory_object name string) for glCreateMemoryObjectsEXT, glTextureStorageMem2DEXT, GL_TEXTURE_TILING_EXT
// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt (GL_EXT_memory_object_win32 name string) for glImportMemoryWin32HandleEXT
// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_win32_keyed_mutex.txt (GL_EXT_win32_keyed_mutex name string) for glAcquireKeyedMutexWin32EXT


void OpenGLExtensions::init()
{
#if !EMSCRIPTEN && !defined(__APPLE__)
	if(glCreateMemoryObjectsEXT == nullptr)
	{
		glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)gl3wGetProcAddress("glCreateMemoryObjectsEXT"); // from EXT_external_objects
		glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)gl3wGetProcAddress("glDeleteMemoryObjectsEXT"); // from EXT_external_objects
		glTextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)gl3wGetProcAddress("glTextureStorageMem2DEXT"); // from EXT_external_objects

		glImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)gl3wGetProcAddress("glImportMemoryWin32HandleEXT"); // from EXT_external_objects_win32

		glAcquireKeyedMutexWin32EXT = (PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC)gl3wGetProcAddress("glAcquireKeyedMutexWin32EXT"); // from EXT_win32_keyed_mutex
		glReleaseKeyedMutexWin32EXT = (PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC)gl3wGetProcAddress("glReleaseKeyedMutexWin32EXT"); // from EXT_win32_keyed_mutex
	}
#endif
}


#if !EMSCRIPTEN && !defined(__APPLE__)
PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT = nullptr;
PFNGLDELETEMEMORYOBJECTSEXTPROC glDeleteMemoryObjectsEXT = nullptr;
PFNGLTEXTURESTORAGEMEM2DEXTPROC glTextureStorageMem2DEXT = nullptr;

PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT = nullptr;

PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glAcquireKeyedMutexWin32EXT = nullptr;
PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glReleaseKeyedMutexWin32EXT = nullptr;
#endif
