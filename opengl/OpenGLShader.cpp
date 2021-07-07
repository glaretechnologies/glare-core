/*=====================================================================
OpenGLShader.cpp
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "OpenGLShader.h"


#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


static const std::string getLog(GLuint shader)
{
	// Get log length including null terminator
	GLint log_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
		
	std::string log;
	if(log_length > 0)
	{
		log.resize(log_length - 1);
		glGetShaderInfoLog(shader, log_length, NULL, &log[0]);
	}
	return log;
}


OpenGLShader::OpenGLShader(const std::string& path, const std::string& preprocessor_defines, GLenum shader_type)
:	shader(0)
{
	shader = glCreateShader(shader_type);
	if(shader == 0)
		throw glare::Exception("Failed to create OpenGL shader.");

	try
	{
		const std::string shader_src = FileUtils::readEntireFileTextMode(path);

		// Insert the preprocessor defines into the source code, after the first line, which should contain the version string.

		const size_t first_newline_index = shader_src.find('\n');
		if(first_newline_index == std::string::npos)
			throw glare::Exception("Shader source must have at least one newline.");

		const std::string first_line = shader_src.substr(0, first_newline_index + 1); // First line including newline char

		std::string processed_src = first_line;
		processed_src += preprocessor_defines;
		processed_src += shader_src.substr(first_newline_index + 1); // The tail part of the src.


		// Build string pointer and length arrays
		std::vector<const char*> strings;
		std::vector<GLint> lengths; // Length of each line, including possible \n at end of line.
		size_t last_line_start_i = 0;
		for(size_t i=0; i<processed_src.size(); ++i)
		{
			if(processed_src[i] == '\n')
			{
				strings.push_back(processed_src.data() + last_line_start_i);
				const size_t line_len = i - last_line_start_i + 1; // Plus one to include this newline character.
				assert(line_len >= 1);
				lengths.push_back((GLint)line_len);
				// const std::string line(program_source.data() + last_line_start_i, line_len);
				last_line_start_i = i + 1; // Next line starts after this newline char.
			}
		}

		glShaderSource(shader, (GLsizei)strings.size(), &strings[0], &lengths[0]);

		glCompileShader(shader);

		const std::string log = getLog(shader);

		if(!isAllWhitespace(log))
			conPrint("shader log for " + FileUtils::getFilename(path) + ":\n" + log);

		// TEMP: dump full shader to disk
		// FileUtils::writeEntireFileTextMode(FileUtils::getFilename(path), processed_src);

		GLint shader_ok;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
		if(!shader_ok)
			throw glare::Exception("Failed to compile shader " + FileUtils::getFilename(path) + ": " + log);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception(e.what());
	}
}


OpenGLShader::~OpenGLShader()
{
	glDeleteShader(shader);
}
