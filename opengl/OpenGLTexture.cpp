#include "OpenGLTexture.h"


#include "OpenGLEngine.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
//#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT					0x83F0
#define GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT					0x8C4C
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT				0x8C4D
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT				0x8C4E
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT				0x8C4F

#define GL_TEXTURE_MAX_ANISOTROPY_EXT							0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT						0x84FF


OpenGLTexture::OpenGLTexture()
:	texture_handle(0)
{
}


OpenGLTexture::~OpenGLTexture()
{
	glDeleteTextures(1, &texture_handle);
	texture_handle = 0;
}


bool OpenGLTexture::hasAlpha() const
{
	assert(texture_handle != 0);
	return format == Format_SRGBA_Uint8 || format == Format_RGBA_Linear_Uint8 || format == Format_Compressed_SRGBA_Uint8;
}


void OpenGLTexture::getGLFormat(Format format_, GLint& internal_format, GLenum& gl_format, GLenum& type)
{
	switch(format_)
	{
	case Format_Greyscale_Uint8:
		internal_format = GL_RGB;
		gl_format = GL_RED; // NOTE: this sux, image turns out red.  Improve by converting texture here.
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_SRGB_Uint8:
		internal_format = GL_SRGB8;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_SRGBA_Uint8:
		internal_format = GL_SRGB8_ALPHA8;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_RGB_Linear_Uint8:
		internal_format = GL_RGB;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_RGBA_Linear_Uint8:
		internal_format = GL_RGBA;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_RGB_Linear_Float:
		internal_format = GL_RGBA32F;
		gl_format = GL_RGB;
		type = GL_FLOAT;
		break;
	case Format_RGB_Linear_Half:
		internal_format = GL_RGB; // NOTE: this right?
		gl_format = GL_RGB;
		type = GL_HALF_FLOAT;
		break;
	case Format_Depth_Float:
		internal_format = GL_DEPTH_COMPONENT;
		gl_format = GL_DEPTH_COMPONENT;
		type = GL_FLOAT;
		break;
	case Format_Compressed_SRGB_Uint8:
		assert(0); // getGLFormat() shouldn't be called for compressed formats
		internal_format = GL_SRGB8;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_SRGBA_Uint8:
		assert(0); // getGLFormat() shouldn't be called for compressed formats
		internal_format = GL_SRGB8;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	}
}


static void setPixelStoreAlignment(size_t tex_xres, GLenum gl_format, GLenum type)
{
	size_t component_size = 1;
	if(type == GL_UNSIGNED_BYTE)
		component_size = 1;
	else if(type == GL_HALF_FLOAT)
		component_size = 2;
	else if(type == GL_FLOAT)
		component_size = 4;
	else
	{
		assert(0);
	}

	size_t num_components = 1;
	if(gl_format == GL_RED || gl_format == GL_DEPTH_COMPONENT)
		num_components = 1;
	else if(gl_format == GL_RGB)
		num_components = 3;
	else if(gl_format == GL_RGBA)
		num_components = 4;
	else
	{
		assert(0);
	}

	const size_t row_stride = component_size * num_components * tex_xres; // in bytes
	if(row_stride % 4 != 0)
	{
		if(row_stride % 2 == 0)
		{
			//conPrint("Setting GL_UNPACK_ALIGNMENT to 2.");
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2); // Tell OpenGL that our rows are 2-byte aligned.
		}
		else
		{
			//conPrint("Setting GL_UNPACK_ALIGNMENT to 1.");
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Tell OpenGL that our rows are 1-byte aligned.
		}
	}
	// else if 4-byte aligned, leave at the default 4-byte alignment.
}


void OpenGLTexture::loadCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const void*>& tex_data, Format format_)
{
	assert(tex_data.size() == 6);

	this->format = format_;

	if(texture_handle)
	{
		glDeleteTextures(1, &texture_handle);
		texture_handle = 0;
	}

	glGenTextures(1, &texture_handle);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture_handle);


	GLint internal_format;
	GLenum gl_format, gl_type;
	getGLFormat(format_, internal_format, gl_format, gl_type);

	setPixelStoreAlignment(tex_xres, gl_format, gl_type);
	
	for(int i=0; i<6; ++i)
	{
		// Upload texture to OpenGL
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, // target
			0, // LOD level
			internal_format, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			gl_format, // format
			gl_type, // type
			tex_data[i]
		); 
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);  

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


void OpenGLTexture::load(size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data, const Reference<OpenGLEngine>& opengl_engine,
	Format format_,
	Filtering filtering,
	Wrapping wrapping
	)
{
	this->format = format_;

	if(texture_handle == 0)
		glGenTextures(1, &texture_handle);

	glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8)
	{
		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			(format == Format_Compressed_SRGB_Uint8) ? GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
	else
	{
		GLint internal_format;
		GLenum gl_format, gl_type;
		getGLFormat(format_, internal_format, gl_format, gl_type);

		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(tex_xres, gl_format, gl_type);

		glTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			internal_format, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			gl_format, // format
			gl_type, // type
			tex_data.data()
		);
	}

	if(wrapping == Wrapping_Clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}


	if(filtering == Filtering_Nearest)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else if(filtering == Filtering_Bilinear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else if(filtering == Filtering_Fancy)
	{
		// Enable anisotropic texture filtering if supported.
		if(opengl_engine.nonNull() && opengl_engine->anisotropic_filtering_supported)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, opengl_engine->max_anisotropy);

		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		assert(0);
	}
}


void OpenGLTexture::makeGLTexture(Format format_)
{
	this->format = format_;

	if(texture_handle)
	{
		glDeleteTextures(1, &texture_handle);
		texture_handle = 0;
	}

	glGenTextures(1, &texture_handle);
	glBindTexture(GL_TEXTURE_2D, texture_handle);
}


void OpenGLTexture::setMipMapLevelData(int mipmap_level, size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data)
{
	if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8)
	{
		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			mipmap_level, // LOD level
			(format == Format_Compressed_SRGB_Uint8) ? GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
	else
	{
		GLint internal_format;
		GLenum gl_format, gl_type;
		getGLFormat(format, internal_format, gl_format, gl_type);

		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(tex_xres, gl_format, gl_type);

		glTexImage2D(
			GL_TEXTURE_2D,
			mipmap_level, // LOD level
			internal_format, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			gl_format, // format
			gl_type, // type
			tex_data.data()
		);

		glGenerateMipmap(GL_TEXTURE_2D);//TEMP
	}
}


void OpenGLTexture::setTexParams(const Reference<OpenGLEngine>& opengl_engine,
	Filtering filtering,
	Wrapping wrapping
)
{
	if(wrapping == Wrapping_Clamp)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}


	if(filtering == Filtering_Nearest)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else if(filtering == Filtering_Bilinear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else if(filtering == Filtering_Fancy)
	{
		// Enable anisotropic texture filtering if supported.
		if(opengl_engine.nonNull() && opengl_engine->anisotropic_filtering_supported)
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, opengl_engine->max_anisotropy);

		//glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		assert(0);
	}
}
