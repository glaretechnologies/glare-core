/*=====================================================================
OpenGLShader.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "../utils/RefCounted.h"
#include <string>


/*=====================================================================
OpenGLShader
----------

=====================================================================*/
class OpenGLShader : public RefCounted
{
public:
	OpenGLShader(const std::string& path, GLenum shader_type);
	~OpenGLShader();

	GLuint shader;
private:
	INDIGO_DISABLE_COPY(OpenGLShader);
};
