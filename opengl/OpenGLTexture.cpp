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
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT					0x8E8F


OpenGLTexture::OpenGLTexture()
:	texture_handle(0),
	xres(0),
	yres(0),
	loaded_size(0),
	format(Format_SRGB_Uint8)
{
}


// Allocate uninitialised texture
OpenGLTexture::OpenGLTexture(size_t tex_xres, size_t tex_yres, const OpenGLEngine* opengl_engine,
	Format format_,
	Filtering filtering_,
	Wrapping wrapping_)
:	texture_handle(0),
	xres(0),
	yres(0),
	loaded_size(0)
{
	load(tex_xres, tex_yres, ArrayRef<uint8>(NULL, 0), opengl_engine, format_, filtering_, wrapping_);
}


OpenGLTexture::OpenGLTexture(size_t tex_xres, size_t tex_yres, const OpenGLEngine* opengl_engine,
	Format format_,
	GLint gl_internal_format_,
	GLenum gl_format_,
	Filtering filtering_,
	Wrapping wrapping_)
:	texture_handle(0),
	xres(0),
	yres(0),
	loaded_size(0)
{
	loadWithFormats(tex_xres, tex_yres, ArrayRef<uint8>(NULL, 0), opengl_engine, format_, gl_internal_format_, gl_format_, filtering_, wrapping_);
}


OpenGLTexture::~OpenGLTexture()
{
	glDeleteTextures(1, &texture_handle);
	texture_handle = 0;
}


bool OpenGLTexture::hasAlpha() const
{
	//assert(texture_handle != 0);
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
		//assert(0); // getGLFormat() shouldn't be called for compressed formats
		internal_format = GL_SRGB8;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_SRGBA_Uint8:
		//assert(0); // getGLFormat() shouldn't be called for compressed formats
		internal_format = GL_SRGB8;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_BC6:
		//assert(0); // getGLFormat() shouldn't be called for compressed formats
		internal_format = GL_SRGB8;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	}
}


// See https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glPixelStorei.xml
// We are assuming that our texture data is tightly packed, with no padding at the end of rows.
// Therefore the row alignment may be 8, 4, 2 or even 1 byte.  We need to tell OpenGL this.
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
	else if(gl_format == GL_BGRA)
		num_components = 4;
	else
	{
		assert(0);
	}

	const size_t row_stride = component_size * num_components * tex_xres; // in bytes
	GLint alignment;
	if(row_stride % 8 == 0)
	{
		alignment = 8;
	}
	else if(row_stride % 4 == 0)
	{
		alignment = 4;
	}
	else if(row_stride % 2 == 0)
	{
		alignment = 2;
	}
	else
	{
		alignment = 1;
	}

	//conPrint("Setting GL_UNPACK_ALIGNMENT to " + toString(alignment));
	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment); // Tell OpenGL that our rows are n-byte aligned.
}


void OpenGLTexture::loadCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const void*>& tex_data, Format format_)
{
	assert(tex_data.size() == 6);

	this->format = format_;
	this->xres = tex_xres;
	this->yres = tex_yres;

	if(texture_handle)
	{
		glDeleteTextures(1, &texture_handle);
		texture_handle = 0;
	}

	glGenTextures(1, &texture_handle);
	assert(texture_handle != 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture_handle);


	getGLFormat(format_, this->gl_internal_format, this->gl_format, this->gl_type);

	setPixelStoreAlignment(tex_xres, gl_format, gl_type);
	
	for(int i=0; i<6; ++i)
	{
		// Upload texture to OpenGL
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, // target
			0, // LOD level
			gl_internal_format, // internal format
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


void OpenGLTexture::load(size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data)
{
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
		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(tex_xres, gl_format, gl_type);

		glTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			gl_internal_format, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			gl_format, // format
			gl_type, // type
			tex_data.data()
		);
	}

	// Gen mipmaps if needed
	if(filtering == Filtering_Fancy)
		glGenerateMipmap(GL_TEXTURE_2D);
}


void OpenGLTexture::load(size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data, const OpenGLEngine* opengl_engine,
	Format format_,
	Filtering filtering_,
	Wrapping wrapping
	)
{
	this->format = format_;
	this->filtering = filtering_;
	this->xres = tex_xres;
	this->yres = tex_yres;
	this->loaded_size = tex_data.size(); // NOTE: wrong for empty tex_data case (e.g. for shadow mapping textures)

	// Work out gl_internal_format etc..
	getGLFormat(format_, this->gl_internal_format, this->gl_format, this->gl_type);

	if(texture_handle == 0)
	{
		glGenTextures(1, &texture_handle);
		assert(texture_handle != 0);
	}

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
		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(tex_xres, gl_format, gl_type);

		glTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			gl_internal_format, // internal format
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
		// Enable anisotropic texture filtering
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


void OpenGLTexture::loadWithFormats(size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data, 
	const OpenGLEngine* opengl_engine, // May be null.  Used for querying stuff.
	Format format_,
	GLint gl_internal_format_,
	GLenum gl_format_,
	Filtering filtering_,
	Wrapping wrapping
)
{
	this->format = format_;
	this->gl_internal_format = gl_internal_format_;
	this->gl_format = gl_format_;
	this->filtering = filtering_;
	this->xres = tex_xres;
	this->yres = tex_yres;
	this->loaded_size = tex_data.size(); // NOTE: wrong for empty tex_data case (e.g. for shadow mapping textures)


	// Work out gl_type
	GLint dummy_gl_internal_format;
	GLenum dummy_gl_format, new_gl_type;
	getGLFormat(format, dummy_gl_internal_format, dummy_gl_format, new_gl_type);
	this->gl_type = new_gl_type;

	if(texture_handle == 0)
	{
		glGenTextures(1, &texture_handle);
		assert(texture_handle != 0);
	}

	glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8)
	{
		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			gl_internal_format, // internal format
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
	else
	{
		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(tex_xres, gl_format, gl_type);

		glTexImage2D(
			GL_TEXTURE_2D,
			0, // LOD level
			gl_internal_format, // internal format
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
		// Enable anisotropic texture filtering
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


void OpenGLTexture::bind()
{
	assert(texture_handle != 0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);
}


void OpenGLTexture::unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
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
	/*if(texture_handle == 0)
	{
		GLenum err = glGetError();
		if(err == GL_INVALID_VALUE)
			conPrint("GL_INVALID_VALUE");
		if(err == GL_INVALID_OPERATION)
			conPrint("GL_INVALID_OPERATION");
		printVar(err);
	}*/
	assert(texture_handle != 0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);
}


void OpenGLTexture::setMipMapLevelData(int mipmap_level, size_t tex_xres, size_t tex_yres, ArrayRef<uint8> tex_data)
{
	if(mipmap_level == 0)
	{
		this->xres = tex_xres;
		this->yres = tex_yres;
	}
	this->loaded_size += tex_data.size();

	if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_BC6)
	{
		GLint internal_format = 0;
		if(format == Format_Compressed_SRGB_Uint8)
			internal_format = GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT;
		else if(format == Format_Compressed_SRGBA_Uint8)
			internal_format = GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
		else if(format == Format_Compressed_BC6)
			internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		else
		{
			assert(0);
		}

		glCompressedTexImage2D(
			GL_TEXTURE_2D,
			mipmap_level, // LOD level
			internal_format,
			(GLsizei)tex_xres, (GLsizei)tex_yres,
			0, // border
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
	else
	{
		// NOTE: currently we don't hit this code path, because we only explictly set mipmap level data for compressed images.
		assert(0);

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
	}
}


void OpenGLTexture::setTexParams(const Reference<OpenGLEngine>& opengl_engine,
	Filtering filtering_,
	Wrapping wrapping
)
{
	this->filtering = filtering_;

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


size_t OpenGLTexture::getByteSize() const
{
	return this->loaded_size;
}
