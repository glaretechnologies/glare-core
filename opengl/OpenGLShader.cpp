/*=====================================================================
OpenGLShader.cpp
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include <GL/glew.h>
#include "OpenGLShader.h"


#include "../utils/FileUtils.h"
#include "../utils/Exception.h"


OpenGLShader::OpenGLShader(const std::string& path, GLenum shader_type)
:	shader(0)
{
	shader = glCreateShader(shader_type);

	try
	{
		const std::string shader_src = FileUtils::readEntireFileTextMode(path);

		// Build string pointer and length arrays
		std::vector<const char*> strings;
		std::vector<GLint> lengths; // Length of each line, including possible \n at end of line.
		size_t last_line_start_i = 0;
		for(size_t i=0; i<shader_src.size(); ++i)
		{
			if(shader_src[i] == '\n')
			{
				strings.push_back(shader_src.data() + last_line_start_i);
				const size_t line_len = i - last_line_start_i + 1; // Plus one to include this newline character.
				assert(line_len >= 1);
				lengths.push_back((GLint)line_len);
				// const std::string line(program_source.data() + last_line_start_i, line_len);
				last_line_start_i = i + 1; // Next line starts after this newline char.
			}
		}

		glShaderSource(shader, (GLsizei)strings.size(), &strings[0], &lengths[0]);

		glCompileShader(shader);

	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
}


OpenGLShader::~OpenGLShader()
{
	glDeleteShader(shader);
}
