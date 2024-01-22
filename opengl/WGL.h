/*=====================================================================
WGL.h
-----
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"


/*=====================================================================
WGL
---
Windows OpenGL functions
=====================================================================*/
class WGL
{
public:

	void init(); // NOTE: should only call when an OpenGL context is current.
#if defined(_WIN32) && !defined(EMSCRIPTEN)
	// See wglew.h
	typedef HANDLE (WINAPI * PFNWGLDXOPENDEVICENVPROC) (void* dxDevice);
	typedef BOOL (WINAPI * PFNWGLDXCLOSEDEVICENVPROC) (HANDLE hDevice);
	typedef HANDLE (WINAPI * PFNWGLDXREGISTEROBJECTNVPROC) (HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
	typedef BOOL (WINAPI * PFNWGLDXUNREGISTEROBJECTNVPROC) (HANDLE hDevice, HANDLE hObject);
	typedef BOOL (WINAPI * PFNWGLDXLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
	typedef BOOL (WINAPI * PFNWGLDXUNLOCKOBJECTSNVPROC) (HANDLE hDevice, GLint count, HANDLE* hObjects);
	
	PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
	PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
	PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
	PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
	PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
	PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
#endif
};
