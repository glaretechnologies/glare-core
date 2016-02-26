/*=====================================================================
VBO.h
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-02-13 22:44:00 +0000
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"


/*=====================================================================
VBO
-------------------

=====================================================================*/
class VBO : public RefCounted
{
public:
	VBO(const void* data, size_t size, GLenum buffer_type = GL_ARRAY_BUFFER);
	~VBO();

	void bind();
	void unbind();

	const size_t getSize() const{ return size; }
private:
	INDIGO_DISABLE_COPY(VBO)

	GLuint buffer_name;
	GLenum buffer_type;
	size_t size; // in bytes
};


typedef Reference<VBO> VBORef;
