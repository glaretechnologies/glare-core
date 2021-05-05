/*=====================================================================
WGL.cpp
-------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WGL.h"


#include "../utils/Exception.h"


void WGL::init()
{
#ifdef _WIN32
	if(wglGetCurrentContext() == NULL)
		throw glare::Exception("WGL::init(): current opengl context is null.");

	wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
	if(!wglDXOpenDeviceNV)
		throw glare::Exception("failed to get wglDXOpenDeviceNV");

	wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)wglGetProcAddress("wglDXCloseDeviceNV");
	if(!wglDXCloseDeviceNV)
		throw glare::Exception("failed to get wglDXCloseDeviceNV");

	wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
	if(!wglDXRegisterObjectNV)
		throw glare::Exception("failed to get wglDXRegisterObjectNV");

	wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
	if(!wglDXUnregisterObjectNV)
		throw glare::Exception("failed to get wglDXUnregisterObjectNV");

	wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
	if(!wglDXLockObjectsNV)
		throw glare::Exception("failed to get wglDXLockObjectsNV");

	wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");
	if(!wglDXUnlockObjectsNV)
		throw glare::Exception("failed to get wglDXUnlockObjectsNV");
#endif
}
