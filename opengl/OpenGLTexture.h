/*=====================================================================
OpenGLTexture.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <vector>
#include <string>


class OpenGLEngine;


struct OpenGLTextureKey
{
	OpenGLTextureKey() {}
	explicit OpenGLTextureKey(const std::string& path_) : path(path_) {}
	std::string path;

	bool operator < (const OpenGLTextureKey& other) const { return path < other.path; }
	bool operator == (const OpenGLTextureKey& other) const { return path == other.path; }
};


struct OpenGLTextureKeyHash
{
	size_t operator () (const OpenGLTextureKey& key) const
	{
		std::hash<std::string> h;
		return h(key.path);
	}
};


// Instead of inheriting from RefCounted, will implement custom decRefCount() etc.. that calls textureBecameUnused().
class OpenGLTexture
{
public:
	enum Filtering
	{
		Filtering_Nearest,
		Filtering_Bilinear,
		Filtering_Fancy, // Trilinear + anisotropic filtering if available
		Filtering_PCF
	};

	enum Wrapping
	{
		Wrapping_Repeat,
		Wrapping_Clamp
	};

	enum Format
	{
		Format_Greyscale_Uint8,
		Format_Greyscale_Float,
		Format_Greyscale_Half,
		Format_SRGB_Uint8,
		Format_SRGBA_Uint8,
		Format_RGB_Linear_Uint8,
		Format_RGBA_Linear_Uint8,
		Format_RGB_Linear_Float,
		Format_RGB_Linear_Half,
		Format_Depth_Float,
		Format_Depth_Uint16,
		Format_Compressed_RGB_Uint8,
		Format_Compressed_RGBA_Uint8,
		Format_Compressed_SRGB_Uint8,
		Format_Compressed_SRGBA_Uint8,
		Format_Compressed_BC6 // BC6 half-float unsigned format: e.g. GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT
	};

	OpenGLTexture();

	// Create texture.  Uploads tex_data to texture if tex_data is non-null.
	OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
		ArrayRef<uint8> tex_data,
		Format format,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat,
		bool has_mipmaps = true);

	// Create texture, specify exact GL formats
	OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
		ArrayRef<uint8> tex_data,
		Format format,
		GLint gl_internal_format,
		GLenum gl_format,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat);

	~OpenGLTexture();

	
	void createCubeMap(size_t tex_xres, size_t tex_yres, const std::vector<const void*>& tex_data, Format format);


	//--------------------------------------------- Updating existing texture ---------------------------------------------
	void setMipMapLevelData(int mipmap_level, size_t level_W, size_t level_H, ArrayRef<uint8> tex_data); // Texture should be bound beforehand
	
	// Load into a texture that has already had its format, filtering and wrapping modes set.
	void loadIntoExistingTexture(size_t tex_xres, size_t tex_yres, size_t row_stride_B, ArrayRef<uint8> tex_data);

	void loadRegionIntoExistingTexture(size_t x, size_t y, size_t w, size_t h, size_t row_stride_B, ArrayRef<uint8> tex_data);

	void setTWrappingEnabled(bool wrapping_enabled);
	//---------------------------------------------------------------------------------------------------------------------
	
	void bind();
	void unbind();

	bool hasAlpha() const;

	// Will return 0 if texture has not been created yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }

	size_t getByteSize() const;


	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount++;
	}

	/// Returns previous reference count
	inline int64 decRefCount() const
	{ 
		const int64 prev_ref_count = refcount;
		refcount--;
		assert(refcount >= 0);

		if(refcount == 1)
			textureRefCountDecreasedToOne();

		return prev_ref_count;
	}

	inline int64 getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}

	void textureRefCountDecreasedToOne() const;


	uint64 getBindlessTextureHandle();


	GLuint texture_handle;

private:
	GLARE_DISABLE_COPY(OpenGLTexture);

	// Create texture, given that xres, yres, gl_internal_format etc. have been set.
	void doCreateTexture(ArrayRef<uint8> tex_data, 
		const OpenGLEngine* opengl_engine, // May be null.  Used for querying stuff.
		Wrapping wrapping,
		bool has_mipmaps
	);

	static void getGLFormat(Format format, GLint& internal_format, GLenum& gl_format, GLenum& type);

	Format format;
	GLint gl_internal_format; // sized GL internal format (num channels)
	GLenum gl_format; // GL format (order of RGBA channels etc..)
	GLenum gl_type; // Type of pixel channel (GL_UNSIGNED_BYTE, GL_HALF_FLOAT etc..)

	Filtering filtering;

	size_t xres, yres; // Will be set after load() etc.. is called, and 0 beforehand.
	int num_mipmap_levels_allocated;
public:
	mutable int64 refcount;

	OpenGLEngine* m_opengl_engine;
	OpenGLTextureKey key;

	uint64 bindless_tex_handle;
};


typedef Reference<OpenGLTexture> OpenGLTextureRef;
