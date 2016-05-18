#include "IncludeOpenGL.h"
#include "OpenGLTexture.h"


#include "OpenGLEngine.h"


OpenGLTexture::OpenGLTexture()
:	texture_handle(0)
{
}


OpenGLTexture::~OpenGLTexture()
{
	glDeleteTextures(1, &texture_handle);
	texture_handle = 0;
}


void OpenGLTexture::load(size_t tex_xres, size_t tex_yres, const uint8* tex_data, const Reference<OpenGLEngine>& opengl_engine, // bool anisotropic_filtering_supported, float max_anisotropy)
	GLint internal_format,
	GLenum format,
	GLenum type,
	bool nearest_filtering
	)
{
	if(texture_handle)
	{
		glDeleteTextures(1, &texture_handle);
		texture_handle = 0;
	}

	glGenTextures(1, &texture_handle);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(((tex_xres * 3) % 4) != 0)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Tell OpenGL if our texture memory is unaligned

	//glTexImage2D(GL_TEXTURE_2D, 0, 
	//	GL_RGB, // internal format
	//	(GLsizei)tex_xres, (GLsizei)tex_yres, 
	//	0, // border
	//	GL_RGB, // format
	//	GL_UNSIGNED_BYTE, // type
	//	tex_data); // Upload texture to OpenGL
	glTexImage2D(GL_TEXTURE_2D, 0, 
		internal_format, // internal format
		(GLsizei)tex_xres, (GLsizei)tex_yres, 
		0, // border
		format, // format
		type, // type
		tex_data); // Upload texture to OpenGL

	if(nearest_filtering)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else
	{
		// Enable anisotropic texture filtering if supported.
		if(opengl_engine.nonNull() && opengl_engine->anisotropic_filtering_supported)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, opengl_engine->max_anisotropy);

		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}
