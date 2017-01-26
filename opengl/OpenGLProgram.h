/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
class OpenGLShader;


/*=====================================================================
OpenGLProgram
-------------
=====================================================================*/
class OpenGLProgram : public RefCounted
{
public:
	OpenGLProgram(const std::string& prog_name, const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader);
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

	Reference<OpenGLShader> vert_shader;
	Reference<OpenGLShader> frag_shader;
};
