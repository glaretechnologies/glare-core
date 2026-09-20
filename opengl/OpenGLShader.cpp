/*=====================================================================
OpenGLShader.cpp
----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "OpenGLShader.h"


#include "IncludeOpenGL.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include <tracy/Tracy.hpp>


#define GL_SHADER                         0x82E1
#define GL_COMPLETION_STATUS_KHR          0x91B1


OpenGLShader::OpenGLShader(const std::string& path_, const std::string& version_directive, const std::string& preprocessor_defines, GLenum shader_type)
:	shader(0),
	path(path_)
{
	ZoneScoped; // Tracy profiler

	shader = glCreateShader(shader_type);
	if(shader == 0)
		throw glare::Exception("Failed to create OpenGL shader.");

#if !defined(OSX) && !defined(EMSCRIPTEN)
	const std::string shader_name = FileUtils::getFilename(path_).substr(0, 100);
	glObjectLabel(GL_SHADER, shader, (GLsizei)shader_name.size(), shader_name.c_str());
#endif

	try
	{
		const std::string shader_src = FileUtils::readEntireFileTextMode(path);

		const std::string version = version_directive + "\n";
		const string_view line_directive = "#line 1\n"; // Restart line numbering at one again, for more informative error messages, see https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)#Version ('#line directive')

		const char* shader_strings[] = { version.c_str(),       preprocessor_defines.c_str(),       line_directive.data(),        shader_src.c_str() };
		const GLint lengths[] =        { (GLint)version.size(), (GLint)preprocessor_defines.size(), (GLint)line_directive.size(), (GLint)shader_src.size() };

		glShaderSource(shader, /*count=*/(GLsizei)staticArrayNumElems(shader_strings), shader_strings, lengths);

		glCompileShader(shader); // Start compiling shader

		// Don't request compilation log here or check compile status, in order to allow parallel compilation to take place first.
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


OpenGLShader::~OpenGLShader()
{
	ZoneScoped; // Tracy profiler

	glDeleteShader(shader);
}


bool OpenGLShader::checkCompilingDone()
{
	ZoneScoped; // Tracy profiler

	GLint compiling_done_val = 0;
	glGetShaderiv(shader, GL_COMPLETION_STATUS_KHR, &compiling_done_val);
	return compiling_done_val != 0;
}


std::string OpenGLShader::getLog()
{
	ZoneScoped; // Tracy profiler

	// Get log length including null terminator
	GLint log_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

	std::string log;
	if(log_length > 0)
	{
		log.resize(log_length - 1);
		if(log_length >= 1)
			glGetShaderInfoLog(shader, log_length, NULL, &log[0]);
	}
	return log;
}
