/*=====================================================================
OpenGLShader.h
--------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>


/*=====================================================================
OpenGLShader
------------

=====================================================================*/
class OpenGLShader : public RefCounted
{
public:
	OpenGLShader(const std::string& path, const std::string& preprocessor_defines, GLenum shader_type);
	~OpenGLShader();

	GLuint shader;
private:
	INDIGO_DISABLE_COPY(OpenGLShader);
};


typedef Reference<OpenGLShader> OpenGLShaderRef;
