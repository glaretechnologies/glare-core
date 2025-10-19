/*=====================================================================
OpenGLExtensions.cpp
--------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLExtensions.h"


#include "GL/gl3w.h"


// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt for glCreateMemoryObjectsEXT, glTextureStorageMem2DEXT, GL_TEXTURE_TILING_EXT
// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt for glImportMemoryWin32HandleEXT
// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_win32_keyed_mutex.txt for glAcquireKeyedMutexWin32EXT


void OpenGLExtensions::init()
{
	if(glCreateMemoryObjectsEXT == nullptr)
	{
		glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)gl3wGetProcAddress("glCreateMemoryObjectsEXT");
		glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC)gl3wGetProcAddress("glDeleteMemoryObjectsEXT");
		glImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)gl3wGetProcAddress("glImportMemoryWin32HandleEXT");
		glAcquireKeyedMutexWin32EXT = (PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC)gl3wGetProcAddress("glAcquireKeyedMutexWin32EXT");
		glReleaseKeyedMutexWin32EXT = (PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC)gl3wGetProcAddress("glReleaseKeyedMutexWin32EXT");
		glTextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)gl3wGetProcAddress("glTextureStorageMem2DEXT");
	}
}


PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT = nullptr;
PFNGLDELETEMEMORYOBJECTSEXTPROC glDeleteMemoryObjectsEXT = nullptr;
PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT = nullptr;
PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glAcquireKeyedMutexWin32EXT = nullptr;
PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glReleaseKeyedMutexWin32EXT = nullptr;
PFNGLTEXTURESTORAGEMEM2DEXTPROC glTextureStorageMem2DEXT = nullptr;
