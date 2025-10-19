/*=====================================================================
OpenGLMemoryObject.h
--------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


/*=====================================================================
OpenGLMemoryObject
------------------
Allows use of something like a Direct 3D texture as the backing memory for e.g.
an OpenGL texture.

See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt for glCreateMemoryObjectsEXT, glTextureStorageMem2DEXT, GL_TEXTURE_TILING_EXT
See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt for glImportMemoryWin32HandleEXT
See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_win32_keyed_mutex.txt for glAcquireKeyedMutexWin32EXT
=====================================================================*/
class OpenGLMemoryObject : public RefCounted
{
public:
	OpenGLMemoryObject();
	~OpenGLMemoryObject();
	GLARE_DISABLE_COPY(OpenGLMemoryObject);


	void importD3D11ImageFromHandle(/*HANDLE*/void* handle);

	GLuint mem_obj;
};


// Locks a memory object, assuming that the memory object is backed by a keyed-mutex lockable object.
class OpenGLMemoryObjectLock
{
public:
	OpenGLMemoryObjectLock(Reference<OpenGLMemoryObject> mem_ob);
	~OpenGLMemoryObjectLock();

private:
	Reference<OpenGLMemoryObject> mem_ob;
};


typedef Reference<OpenGLMemoryObject> OpenGLMemoryObjectRef;
