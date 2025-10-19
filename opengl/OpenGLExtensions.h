/*=====================================================================
OpenGLExtensions.h
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"


typedef void (APIENTRYP PFNGLCREATEMEMORYOBJECTSEXTPROC) (GLsizei n, GLuint *memoryObjects);
typedef void (APIENTRYP PFNGLDELETEMEMORYOBJECTSEXTPROC) (GLsizei n, const GLuint *memoryObjects);
typedef void (APIENTRYP PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, void *handle);
typedef GLboolean (APIENTRYP PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC) (GLuint memory, GLuint64 key, GLuint timeout);
typedef GLboolean (APIENTRYP PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC) (GLuint memory, GLuint64 key);
typedef void (APIENTRYP PFNGLTEXTURESTORAGEMEM2DEXTPROC) (GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);


extern PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT;
extern PFNGLDELETEMEMORYOBJECTSEXTPROC glDeleteMemoryObjectsEXT;
extern PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT;
extern PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glAcquireKeyedMutexWin32EXT;
extern PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glReleaseKeyedMutexWin32EXT;
extern PFNGLTEXTURESTORAGEMEM2DEXTPROC glTextureStorageMem2DEXT;

#define GL_HANDLE_TYPE_D3D11_IMAGE_EXT    0x958B




/*=====================================================================
OpenGLExtensions
----------------

=====================================================================*/
class OpenGLExtensions
{
public:
	static void init(); // NOTE: should only call when an OpenGL context is current.
};
