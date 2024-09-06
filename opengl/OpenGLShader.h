/*=====================================================================
OpenGLShader.h
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
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
	OpenGLShader(const std::string& path, const std::string& version_directive, const std::string& preprocessor_defines, /*GLenum*/unsigned int shader_type);
	~OpenGLShader();

	std::string getLog();

	GLuint shader;
	const std::string path;
private:
	GLARE_DISABLE_COPY(OpenGLShader);
};


typedef Reference<OpenGLShader> OpenGLShaderRef;
