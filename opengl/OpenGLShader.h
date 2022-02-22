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
	// preprocessor_defines are just inserted directly into the source after the first line.
	OpenGLShader(const std::string& path, const std::string& version_directive, const std::string& preprocessor_defines, GLenum shader_type);
	~OpenGLShader();

	GLuint shader;
private:
	GLARE_DISABLE_COPY(OpenGLShader);
};


typedef Reference<OpenGLShader> OpenGLShaderRef;
