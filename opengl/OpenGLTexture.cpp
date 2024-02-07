/*=====================================================================
OpenGLTexture.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "OpenGLTexture.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "OpenGLMeshRenderData.h"
#include "graphics/TextureProcessing.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT						0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT					0x83F3

// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
#define GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT					0x8C4C
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT				0x8C4D
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT				0x8C4E
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT				0x8C4F

#define GL_TEXTURE_MAX_ANISOTROPY_EXT							0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT						0x84FF
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT					0x8E8F

// For emscripten
#define GL_DEPTH_COMPONENT32F             0x8CAC
#define GL_DEPTH_COMPONENT32              0x81A7
#define GL_TEXTURE_2D_MULTISAMPLE         0x9100
#define GL_BGRA                           0x80E1


OpenGLTexture::OpenGLTexture()
:	texture_handle(0),
	xres(0),
	yres(0),
	MSAA_samples(-1),
	num_mipmap_levels_allocated(0),
	format(Format_SRGB_Uint8),
	refcount(0),
	m_opengl_engine(NULL),
	bindless_tex_handle(0),
	is_bindless_tex_resident(false),
	gl_format(GL_RGB), // Just set some defaults for getByteSize().
	gl_type(GL_UNSIGNED_BYTE),
	gl_internal_format(GL_RGB),
	filtering(Filtering_Fancy),
	texture_target(GL_TEXTURE_2D)
{
	this->allocated_tex_view_info.texture_handle = 0;
}


// Allocate uninitialised texture
OpenGLTexture::OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
	ArrayRef<uint8> tex_data,
	Format format_,
	Filtering filtering_,
	Wrapping wrapping_,
	bool has_mipmaps_,
	int MSAA_samples_)
:	texture_handle(0),
	xres(0),
	yres(0),
	num_mipmap_levels_allocated(0),
	refcount(0),
	m_opengl_engine(opengl_engine),
	bindless_tex_handle(0),
	is_bindless_tex_resident(false),
	texture_target((MSAA_samples_ > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D)
{
	this->allocated_tex_view_info.texture_handle = 0;

	this->format = format_;
	this->filtering = filtering_;
	this->xres = tex_xres;
	this->yres = tex_yres;
	this->MSAA_samples = MSAA_samples_;

	// Work out gl_internal_format etc..
	getGLFormat(format_, this->gl_internal_format, this->gl_format, this->gl_type);

	doCreateTexture(tex_data, opengl_engine, wrapping_, has_mipmaps_);
}


OpenGLTexture::OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
	ArrayRef<uint8> tex_data,
	Format format_,
	GLint gl_internal_format_,
	GLenum gl_format_,
	Filtering filtering_,
	Wrapping wrapping_)
:	texture_handle(0),
	xres(0),
	yres(0),
	MSAA_samples(-1),
	num_mipmap_levels_allocated(0),
	refcount(0),
	m_opengl_engine(opengl_engine),
	bindless_tex_handle(0),
	is_bindless_tex_resident(false),
	texture_target(GL_TEXTURE_2D)
{
	this->allocated_tex_view_info.texture_handle = 0;

	this->format = format_;
	this->gl_internal_format = gl_internal_format_;
	this->gl_format = gl_format_;
	this->filtering = filtering_;
	this->xres = tex_xres;
	this->yres = tex_yres;

	// Work out gl_type
	GLint dummy_gl_internal_format;
	GLenum dummy_gl_format, new_gl_type;
	getGLFormat(format, dummy_gl_internal_format, dummy_gl_format, new_gl_type);
	this->gl_type = new_gl_type;

	doCreateTexture(tex_data, opengl_engine, wrapping_, /*use mipmaps=*/true);
}


OpenGLTexture::~OpenGLTexture()
{
#if USE_TEXTURE_VIEWS
	if(allocated_tex_view_info.texture_handle != 0)
	{
		// If this is a texture view:
		m_opengl_engine->getTextureAllocator().freeTextureView(allocated_tex_view_info);
	}
	else
#endif // USE_TEXTURE_VIEWS
	{
		glDeleteTextures(1, &texture_handle);
		texture_handle = 0;
	}
}


bool OpenGLTexture::hasAlpha() const
{
	//assert(texture_handle != 0);
	return format == Format_SRGBA_Uint8 || format == Format_RGBA_Linear_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_RGBA_Uint8;
}


// internal_format is used as an argument to glTexStorage2D(), so as per https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
// We want to set it to a 'sized internal format'.
void OpenGLTexture::getGLFormat(Format format_, GLint& internal_format, GLenum& gl_format, GLenum& type)
{
	switch(format_)
	{
	case Format_Greyscale_Uint8:
		internal_format = GL_RGB;
		gl_format = GL_RED; // NOTE: this sux, image turns out red.  Improve by converting texture here.
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Greyscale_Float:
		internal_format = GL_R32F;
		gl_format = GL_RED;
		type = GL_FLOAT;
		break;
	case Format_Greyscale_Half:
		internal_format = GL_R16F;
		gl_format = GL_RED;
		type = GL_HALF_FLOAT;
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
		internal_format = GL_RGB8;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_RGBA_Linear_Uint8:
		internal_format = GL_RGBA8;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_RGB_Linear_Float:
		internal_format = GL_RGB32F;
		gl_format = GL_RGB;
		type = GL_FLOAT;
		break;
	case Format_RGB_Linear_Half:
		internal_format = GL_RGB16F;
		gl_format = GL_RGB;
		type = GL_HALF_FLOAT;
		break;
	case Format_Depth_Float:
		internal_format = GL_DEPTH_COMPONENT32F;
		gl_format = GL_DEPTH_COMPONENT;
		type = GL_FLOAT;
		break;
	case Format_Depth_Uint16:
		internal_format = GL_DEPTH_COMPONENT16;
		gl_format = GL_DEPTH_COMPONENT;
		type = GL_UNSIGNED_SHORT;
		break;
	case Format_Compressed_RGB_Uint8:
		internal_format = GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_RGBA_Uint8:
		internal_format = GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_SRGB_Uint8:
		internal_format = GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT;
		gl_format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_SRGBA_Uint8:
		internal_format = GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case Format_Compressed_BC6:
		internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		gl_format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	}
}


static size_t getPixelSizeB(GLenum gl_format, GLenum type)
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

	return component_size * num_components;
}


// Compute number of bytes per pixel used for the texture on the GPU.
static double getInternalPixelSizeB(GLint internal_format)
{
	switch(internal_format)
	{
		case GL_RGB: return 3;
		case GL_R32F: return 4;
		case GL_R16F: return 2;
		case GL_SRGB8: return 3;
		case GL_SRGB8_ALPHA8: return 4;
		case GL_RGB8: return 3;
		case GL_RGBA8: return 4;
		case GL_RGB32F: return 12;
		case GL_RGBA32F: return 16;
		case GL_RGB16F: return 6;
		case GL_DEPTH_COMPONENT32: return 4;
		case GL_DEPTH_COMPONENT16: return 2;
		case GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT: return 0.5; // 8 bytes per 4*4 pixel block
		case GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT: return 1; // 16 bytes per 4*4 pixel block
		case GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT: return 0.5; // 8 bytes per 4*4 pixel block
		case GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return 1; // 16 bytes per 4*4 pixel block
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return 1; // 16 bytes per 4*4 pixel block
		default:
		{
			assert(0);
			return 1;
		}
	};
}


std::string getStringForGLInternalFormat(GLint internal_format)
{
	switch(internal_format)
	{
		case GL_RGB: return "GL_RGB";
		case GL_R32F: return "GL_R32F";
		case GL_R16F: return  "GL_R16F";
		case GL_SRGB8: return  "GL_SRGB8";
		case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
		case GL_RGB8: return "GL_RGB8";
		case GL_RGBA8: return "GL_RGBA8";
		case GL_RGB32F: return "GL_RGB32F";
		case GL_RGBA32F: return "GL_RGBA32F";
		case GL_RGB16F: return "GL_RGB16F";
		case GL_DEPTH_COMPONENT32: return "GL_DEPTH_COMPONENT32";
		case GL_DEPTH_COMPONENT16: return "GL_DEPTH_COMPONENT16";
		case GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT: return "GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT";
		case GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT: return "GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT";
		case GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT: return "GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT";
		case GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return "GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT";
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT";
		default: return "[Unknown]";
	};
}


static size_t getAlignment(size_t data)
{
	if(data % 8 == 0)
		return 8;
	else if(data % 4 == 0)
		return 4;
	else if(data % 2 == 0)
		return 2;
	else
		return 1;
}


// See https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPixelStore.xhtml
// GL_UNPACK_ALIGNMENT specifies the alignment requirements for the start of each pixel row in memory.
// We are assuming that our texture data is tightly packed, with no padding at the end of rows.
// Therefore the row alignment may be 8, 4, 2 or even 1 byte.  We need to tell OpenGL this.
static void setPixelStoreAlignment(const uint8* data, size_t row_stride_B)
{
	const size_t data_alignment = getAlignment((size_t)data);
	const size_t row_stride_alignment = getAlignment((size_t)row_stride_B);
	const size_t alignment = myMin(data_alignment, row_stride_alignment);

	//conPrint("Setting GL_UNPACK_ALIGNMENT to " + toString(alignment));
	glPixelStorei(GL_UNPACK_ALIGNMENT, (GLint)alignment); // Tell OpenGL that our rows are n-byte aligned.
}


// See https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glPixelStorei.xml
// We are assuming that our texture data is tightly packed, with no padding at the end of rows.
// Therefore the row alignment may be 8, 4, 2 or even 1 byte.  We need to tell OpenGL this.
static void setPixelStoreAlignment(size_t tex_xres, GLenum gl_format, GLenum type)
{
	size_t component_size = 1;
	if(type == GL_UNSIGNED_BYTE)
		component_size = 1;
	else if(type == GL_UNSIGNED_SHORT)
		component_size = 2;
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


void OpenGLTexture::createCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const void*>& tex_data, Format format_)
{
	assert(tex_data.size() == 6);

	this->format = format_;
	this->xres = tex_xres;
	this->yres = tex_yres;
	this->num_mipmap_levels_allocated = 1;
	this->texture_target = GL_TEXTURE_CUBE_MAP;

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

static int num_textures_created = 0;
static int num_texture_views_created = 0;
static int num_non_tex_views_created = 0;

// Create texture, given that xres, yres, gl_internal_format etc. have been set.
void OpenGLTexture::doCreateTexture(ArrayRef<uint8> tex_data, 
		const OpenGLEngine* opengl_engine, // May be null.  Used for querying stuff.
		Wrapping wrapping,
		bool use_mipmaps
	)
{
	num_textures_created++;
	//printVar(num_textures_created);
	//conPrint("doCreateTexture(): xres: " + toString(xres) + ", yres: " + toString(yres));
	// xres, yres, gl_internal_format etc. should all have been set.
	assert(xres > 0);
	assert(yres > 0);

#ifdef OSX
	if(this->format == Format_Compressed_BC6)
		throw glare::Exception("Don't support BC6 texture format on Mac");
#endif

	const bool is_MSAA_tex = this->texture_target == GL_TEXTURE_2D_MULTISAMPLE;

#if USE_TEXTURE_VIEWS
	//const bool res_is_valid_for_tex_view = (xres == yres) && Maths::isPowerOfTwo(xres) && (xres >= 4) && (xres <= 1024);
	bool res_is_valid_for_tex_view;
	if(gl_internal_format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
	{
		res_is_valid_for_tex_view = (xres <= 1024);
	}
	else
	{
		res_is_valid_for_tex_view = (xres == yres) && Maths::isPowerOfTwo(xres) && (xres >= 4) && (xres <= 1024);
	}

	const bool internal_format_valid_for_tex_view = 
		gl_internal_format == GL_RGB16F ||
		gl_internal_format == GL_SRGB8 ||
		gl_internal_format == GL_SRGB8_ALPHA8 ||
		gl_internal_format == GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT ||
		gl_internal_format == GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT ||
		gl_internal_format == GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT ||
		gl_internal_format == GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT ||
		gl_internal_format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;

	const bool using_mipmaps = (filtering == Filtering_Fancy) && use_mipmaps;
	bool mipmapping_valid = (gl_internal_format == GL_RGB16F) ? !using_mipmaps : using_mipmaps; // TEMP
	const int num_levels_ = using_mipmaps ? TextureProcessing::computeNumMIPLevels(xres, yres) : 1;

	if((texture_handle == 0) && (opengl_engine != NULL) && res_is_valid_for_tex_view && internal_format_valid_for_tex_view && mipmapping_valid)
	{
		this->allocated_tex_view_info = ((OpenGLEngine*)opengl_engine)->getTextureAllocator().allocTextureView(gl_internal_format, (int)xres, (int)yres, num_levels_);
		this->texture_handle = this->allocated_tex_view_info.texture_handle;
		glBindTexture(texture_target, texture_handle);

		num_texture_views_created++;
		//printVar(num_texture_views_created);
	}
	else
#endif // USE_TEXTURE_VIEWS
	{
		num_non_tex_views_created++;
		// printVar(num_non_tex_views_created);
		// conPrint("===============Not using tex view================");
		// printVar(res_is_valid_for_tex_view);
		// printVar(internal_format_valid_for_tex_view);
		// conPrint("internal format: " + getStringForGLInternalFormat(gl_internal_format));
		// printVar(mipmapping_valid);

		if(texture_handle == 0)
		{
			glGenTextures(1, &texture_handle);
			assert(texture_handle != 0);
			
		}
		
		if(is_MSAA_tex)
		{
#if defined(EMSCRIPTEN)
			// glTexStorage2DMultisample is OpenGL ES 3.1+: https://registry.khronos.org/OpenGL-Refpages/es3.1/html/glTexStorage2DMultisample.xhtml
			assert(0);
#else // else if !EMSCRIPTEN:
			glBindTexture(texture_target, texture_handle);

#if defined(OSX) // glTexStorage2DMultisample isn't defined on Mac.
			glTexImage2DMultisample(texture_target, MSAA_samples, gl_internal_format, (GLsizei)xres, (GLsizei)yres, /*fixedsamplelocations=*/GL_FALSE);
#else
			glTexStorage2DMultisample(texture_target, MSAA_samples, gl_internal_format, (GLsizei)xres, (GLsizei)yres, /*fixedsamplelocations=*/GL_FALSE);
#endif

			this->num_mipmap_levels_allocated = 1;
#endif // end if !EMSCRIPTEN
		}
		else
		{
			glBindTexture(texture_target, texture_handle);

			// Allocate / specify immutable storage for the texture.
			const int num_levels = ((filtering == Filtering_Fancy) && use_mipmaps) ? TextureProcessing::computeNumMIPLevels(xres, yres) : 1;
			glTexStorage2D(texture_target, num_levels, gl_internal_format, (GLsizei)xres, (GLsizei)yres);
			this->num_mipmap_levels_allocated = num_levels;
		}
	}

	if(tex_data.data() != NULL)
	{
		if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_RGB_Uint8 || format == Format_Compressed_RGBA_Uint8)
		{
			// NOTE: xres and yres don't have to be a multiple of 4, as long as we are setting the whole MIP level: See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
			// "CompressedTexSubImage2D will result in an INVALID_OPERATION error only if one of the following conditions occurs:
			// * <width> is not a multiple of four, and <width> plus <xoffset> is not equal to TEXTURE_WIDTH;"
			glCompressedTexSubImage2D(
				texture_target, // target
				0, // LOD level
				0, // xoffset
				0, // yoffset
				(GLsizei)xres, (GLsizei)yres, // width, height
				gl_internal_format, // internal format
				(GLsizei)tex_data.size(),
				tex_data.data()
			);
		}
		else
		{
			assert(tex_data.size() >= (size_t)getInternalPixelSizeB(gl_internal_format) * xres * yres);

			assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
			setPixelStoreAlignment(xres, gl_format, gl_type);

			// NOTE: can't use glTexImage2D on immutable storage, have to use glTexSubImage2D instead.
			glTexSubImage2D(
				texture_target,
				0, // LOD level
				0, // x offset
				0, // y offset
				(GLsizei)xres, // width
				(GLsizei)yres, // height
				gl_format,
				gl_type,
				tex_data.data()
			);
		}
	}

	if(!is_MSAA_tex) // You can't set 'sampler state' params for MSAA textures.
	{
		if(wrapping == Wrapping_Clamp)
		{
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}


		if(filtering == Filtering_Nearest)
		{
			glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else if(filtering == Filtering_Bilinear)
		{
			glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else if(filtering == Filtering_Fancy)
		{
			// Enable anisotropic texture filtering
			glTexParameterf(texture_target, GL_TEXTURE_MAX_ANISOTROPY_EXT, opengl_engine->max_anisotropy);

			if(tex_data.data() != NULL && use_mipmaps)
			{
				// conPrint("Generating mipmaps");
				glGenerateMipmap(texture_target);
			}

			if(use_mipmaps)
				glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			else
				glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else if(filtering == Filtering_PCF)
		{
			glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(texture_target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(texture_target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
		else
		{
			assert(0);
		}
	}
}


void OpenGLTexture::loadIntoExistingTexture(int mipmap_level, size_t tex_xres, size_t tex_yres, size_t row_stride_B, ArrayRef<uint8> tex_data, bool bind_needed)
{
	loadRegionIntoExistingTexture(mipmap_level, /*x=*/0, /*y=*/0, tex_xres, tex_yres, row_stride_B, tex_data, bind_needed);

#if 0
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(tex_data.data() != NULL)
	{
		if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_RGB_Uint8 || format == Format_Compressed_RGBA_Uint8)
		{
			glCompressedTexSubImage2D(
				GL_TEXTURE_2D,
				mipmap_level, // LOD level
				0, // x offset
				0, // y offset
				(GLsizei)tex_xres, (GLsizei)tex_yres,
				gl_internal_format, // internal format
				(GLsizei)tex_data.size(),
				tex_data.data()
			);
		}
		else
		{
			const size_t pixel_size_B = getPixelSizeB(gl_format, gl_type);

			// Set row stride if needed (not tightly packed)
			if(row_stride_B != tex_xres * pixel_size_B) // If not tightly packed:
				glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(row_stride_B / pixel_size_B)); // If greater than 0, GL_UNPACK_ROW_LENGTH defines the number of pixels in a row

			setPixelStoreAlignment(tex_data.data(), row_stride_B);

			// NOTE: can't use glTexImage2D on immutable storage.
			glTexSubImage2D(
				GL_TEXTURE_2D,
				mipmap_level, // LOD level
				0, // x offset
				0, // y offset
				(GLsizei)tex_xres, // width
				(GLsizei)tex_yres, // height
				gl_format,
				gl_type,
				tex_data.data()
			);

			if(row_stride_B != tex_xres * pixel_size_B)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore to default
		}

		// Generate mipmaps if needed
		//if(filtering == Filtering_Fancy)
		//	glGenerateMipmap(GL_TEXTURE_2D);
	}
#endif
}


void OpenGLTexture::loadRegionIntoExistingTexture(int mipmap_level, size_t x, size_t y, size_t region_w, size_t region_h, size_t row_stride_B, ArrayRef<uint8> tex_data, bool bind_needed)
{
	assert(mipmap_level < num_mipmap_levels_allocated);

	if(bind_needed)
		glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(tex_data.data() != NULL)
	{
		if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_RGB_Uint8 || format == Format_Compressed_RGBA_Uint8 ||
			format == Format_Compressed_BC6)
		{
			glCompressedTexSubImage2D(
				GL_TEXTURE_2D,
				mipmap_level, // LOD level
				(GLsizei)x, // x offset
				(GLsizei)y, // y offset
				(GLsizei)region_w, (GLsizei)region_h, // width, height
				gl_internal_format, // internal format
				(GLsizei)tex_data.size(),
				tex_data.data()
			);
		}
		else
		{
			const size_t pixel_size_B = getPixelSizeB(gl_format, gl_type);

			assert(region_w * pixel_size_B <= row_stride_B);
			assert(tex_data.size() >= row_stride_B * region_h);

			// Set row stride if needed (not tightly packed)
			if(row_stride_B != region_w * pixel_size_B) // If not tightly packed:
				glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(row_stride_B / pixel_size_B)); // If greater than 0, GL_UNPACK_ROW_LENGTH defines the number of pixels in a row

			setPixelStoreAlignment(tex_data.data(), row_stride_B);

			glTexSubImage2D(
				GL_TEXTURE_2D,
				mipmap_level, // LOD level
				(GLsizei)x, // x offset
				(GLsizei)y, // y offset
				(GLsizei)region_w, (GLsizei)region_h,
				gl_format,
				gl_type,
				tex_data.data()
			);

			if(row_stride_B != region_w * pixel_size_B)
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore to default
		}
	}
}


void OpenGLTexture::setTWrappingEnabled(bool wrapping_enabled)
{
	glBindTexture(GL_TEXTURE_2D, texture_handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping_enabled ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}


void OpenGLTexture::buildMipMaps()
{
	glBindTexture(GL_TEXTURE_2D, texture_handle);
	glGenerateMipmap(texture_target);
}


void OpenGLTexture::readBackTexture(int mipmap_level, ArrayRef<uint8> buffer)
{
	bind();
#if defined(EMSCRIPTEN)
	assert(0); // glGetTexImage isn't supported in OpenGL ES, try glReadPixels?
#else
	glGetTexImage(GL_TEXTURE_2D, mipmap_level, gl_format, gl_type, (void*)buffer.data());
#endif
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


// Updating existing texture
void OpenGLTexture::setMipMapLevelData(int mipmap_level, size_t level_W, size_t level_H, ArrayRef<uint8> tex_data, bool bind_needed)
{
	if(bind_needed)
		glBindTexture(GL_TEXTURE_2D, texture_handle);

	if(mipmap_level == 0)
	{
		assert(level_W == this->xres && level_H == this->yres);
	}

	if(format == Format_Compressed_SRGB_Uint8 || format == Format_Compressed_SRGBA_Uint8 || format == Format_Compressed_RGB_Uint8 || format == Format_Compressed_RGBA_Uint8 || format == Format_Compressed_BC6)
	{
		glCompressedTexSubImage2D(
			GL_TEXTURE_2D,
			mipmap_level, // LOD level
			0, // xoffset
			0, // yoffset
			(GLsizei)level_W, (GLsizei)level_H,
			gl_internal_format,
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
	else
	{
		assert((uint64)tex_data.data() % 4 == 0); // Assume the texture data is at least 4-byte aligned.
		setPixelStoreAlignment(level_W, gl_format, gl_type);

		glTexSubImage2D(
			GL_TEXTURE_2D,
			mipmap_level, // LOD level
			0, // xoffset
			0, // yoffset
			(GLsizei)level_W, (GLsizei)level_H,
			gl_internal_format,
			(GLsizei)tex_data.size(),
			tex_data.data()
		);
	}
}


size_t OpenGLTexture::getByteSize() const
{
	const double pixel_size_B = getInternalPixelSizeB(gl_internal_format);
	double total_size = xres * yres * pixel_size_B;

	if(num_mipmap_levels_allocated > 1)
		total_size = total_size * 1.333333; // Space for Mipmaps.

	return (size_t)total_size;
}


bool OpenGLTexture::areTextureDimensionsValidForCompression(const Map2D& map)
{
#if EMSCRIPTEN
	// For WebGL, compressed texture dimensions must be multiples of 4: https://github.com/KhronosGroup/WebGL/issues/3621

	if(dynamic_cast<const ImageMapSequenceUInt8*>(&map)) // Handle ImageMapSequenceUInt8 explicitly since it doesn't necessarily have a single width and height.
	{
		const ImageMapSequenceUInt8& seq = static_cast<const ImageMapSequenceUInt8&>(map);

		for(size_t i=0; i<seq.images.size(); ++i)
		{
			const bool dimensions_ok = ((seq.images[i]->getMapWidth() % 4) == 0) && ((seq.images[i]->getMapHeight() % 4) == 0);
			if(!dimensions_ok)
				return false;
		}
		return true;
	}
	else
	{
		return ((map.getMapWidth() % 4) == 0) && ((map.getMapHeight() % 4) == 0);
	}
#else
	return true;
#endif
}


static size_t num_resident_textures = 0;


size_t OpenGLTexture::getNumResidentTextures()
{
	return num_resident_textures;
}


void OpenGLTexture::textureRefCountDecreasedToOne()
{
	// If m_opengl_engine is set, that means this texture was inserted into the opengl_texture ManagerWithCache.
	// The ref count dropping to one means that the only reference held is by the opengl_texture ManagerWithCache.
	// Therefore the texture is not used.
	if(m_opengl_engine)
	{
		m_opengl_engine->textureBecameUnused(this);

#if !defined(OSX) && !defined(EMSCRIPTEN)
		// Since this texture is not being used, we can make it non-resident.
		if((bindless_tex_handle != 0) && is_bindless_tex_resident)
		{
			glMakeTextureHandleNonResidentARB(bindless_tex_handle);
			is_bindless_tex_resident = false;

			num_resident_textures--;
			//conPrint("!!!!!!!!texture became unused, made non resident (num_resident_textures: " + toString(num_resident_textures) + ")");
		}
#endif
	}
}


// Get bindless texture handle, and make texture resident if not already.
uint64 OpenGLTexture::getBindlessTextureHandle()
{
#if defined(OSX) || defined(EMSCRIPTEN)
	assert(0); // Bindless textures aren't supported in OpenGL ES.
	return 0;
#else
	if(bindless_tex_handle == 0)
	{
		bindless_tex_handle = glGetTextureHandleARB(this->texture_handle);

		if(bindless_tex_handle == 0)
			conPrint("glGetTextureHandleARB() failed");
	}

	if(bindless_tex_handle && !is_bindless_tex_resident)
	{
		glMakeTextureHandleResidentARB(bindless_tex_handle);
		is_bindless_tex_resident = true;

		num_resident_textures++;
		//conPrint("Made texture resident (num_resident_textures: " + toString(num_resident_textures) + ")");
	}

	return bindless_tex_handle;
#endif
}
