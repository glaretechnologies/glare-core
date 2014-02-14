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


/*=====================================================================
VBO
-------------------

=====================================================================*/
class VBO : public RefCounted
{
public:
	VBO(const float* data, size_t data_num_floats);
	~VBO();

	void bind();
	void unbind();

private:
	GLuint buffer_name;
};


typedef Reference<VBO> VBORef;
