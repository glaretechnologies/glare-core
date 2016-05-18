/*=====================================================================
OpenGLProgram.cpp
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "OpenGLProgram.h"


#include "OpenGLShader.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


static const std::string getLog(GLuint program)
{
	GLint log_length = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
		
	std::string log;
	log.resize(log_length);
	glGetProgramInfoLog(program, log_length, NULL, &log[0]);
	return log;
}


OpenGLProgram::OpenGLProgram(const std::string& prog_name, const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader)
:	program(0)
{
	program = glCreateProgram();
	if(program == 0)
		throw Indigo::Exception("Failed to create OpenGL program '" + prog_name + "'.");

    glAttachShader(program, vert_shader->shader);
    glAttachShader(program, frag_shader->shader);

	// NOTE: the switching on shader name is a hack, improve:
	// Bind shader input variables.
	if(prog_name == "env")
	{
		glBindAttribLocation(program, 0, "position_in");
		glBindAttribLocation(program, 2, "texture_coords_0_in");
	}
	else if(prog_name == "transparent")
	{
		glBindAttribLocation(program, 0, "position_in");
		glBindAttribLocation(program, 1, "normal_in");
	}
	else if(prog_name == "phong")
	{
		glBindAttribLocation(program, 0, "position_in");
		glBindAttribLocation(program, 1, "normal_in");
		glBindAttribLocation(program, 2, "texture_coords_0_in");
	}
	else if(prog_name == "overlay")
	{
		glBindAttribLocation(program, 0, "position_in");
		glBindAttribLocation(program, 2, "texture_coords_0_in");
	}
	else if(prog_name == "depth")
	{
		glBindAttribLocation(program, 0, "position_in");
	}
	else
	{
		assert(0);
	}




    glLinkProgram(program);

	const std::string log = getLog(program);

	if(!isAllWhitespace(log))
		conPrint("shader program '" + prog_name + "' log:\n" + log);

	GLint program_ok;
	glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
    if(!program_ok)
		throw Indigo::Exception("Failed to link shader program '" + prog_name + "': " + log);

	model_matrix_loc  = glGetUniformLocation(program, "model_matrix");
	view_matrix_loc   = glGetUniformLocation(program, "view_matrix");
	proj_matrix_loc   = glGetUniformLocation(program, "proj_matrix");
	normal_matrix_loc = glGetUniformLocation(program, "normal_matrix");
}


OpenGLProgram::~OpenGLProgram()
{
	glDeleteProgram(program);
}


void OpenGLProgram::useProgram()
{
	glUseProgram(program);
}


void OpenGLProgram::useNoPrograms()
{
	glUseProgram(0);
}


int OpenGLProgram::getUniformLocation(const std::string& name)
{
	return glGetUniformLocation(program, name.c_str());
}
