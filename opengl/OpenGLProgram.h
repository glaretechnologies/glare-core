/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <GL/gl.h>
#include <string>
class OpenGLShader;


/*=====================================================================
OpenGLShader
----------

=====================================================================*/
class OpenGLProgram : public RefCounted
{
public:
	OpenGLProgram(const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader);
	~OpenGLProgram();

	void useProgram();

	static void useNoPrograms();

	int getUniformLocation(const std::string& name);

	GLuint program;
private:
	INDIGO_DISABLE_COPY(OpenGLProgram);
public:
	int model_matrix_loc;
	int view_matrix_loc;
	int proj_matrix_loc;
	int normal_matrix_loc;
};
