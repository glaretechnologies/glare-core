/*=====================================================================
OpenGLTexture.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "TextureAllocator.h"
#include "../graphics/TextureData.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <vector>
#include <string>


class OpenGLEngine;
class OpenGLTexture;
class TextureData;
class Map2D;


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


std::string getStringForGLInternalFormat(GLint internal_format);


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
		Format_RGBA_Linear_Half,
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
		bool has_mipmaps = true,
		int MSAA_samples = -1);

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
	void setMipMapLevelData(int mipmap_level, size_t level_W, size_t level_H, ArrayRef<uint8> tex_data, bool bind_needed);
	
	// Load into a texture that has already had its format, filtering and wrapping modes set.
	void loadIntoExistingTexture(int mipmap_level, size_t tex_xres, size_t tex_yres, size_t row_stride_B, ArrayRef<uint8> tex_data, bool bind_needed);

	void loadRegionIntoExistingTexture(int mipmap_level, size_t x, size_t y, size_t region_w, size_t region_h, size_t src_row_stride_B, ArrayRef<uint8> src_tex_data, bool bind_needed);

	void setTWrappingEnabled(bool wrapping_enabled);

	void buildMipMaps();
	//---------------------------------------------------------------------------------------------------------------------

	void readBackTexture(int mipmap_level, ArrayRef<uint8> buffer);
	
	void bind();
	void unbind();

	bool hasAlpha() const;

	// Will return 0 if texture has not been created yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }

	int MSAASamples() const { return MSAA_samples; }

	size_t getByteSize() const;

	GLenum getTextureTarget() const { return texture_target; } // e.g. GL_TEXTURE_2D or GL_TEXTURE_2D_MULTISAMPLE.

	Filtering getFiltering() const { return filtering; }

	int getNumMipMapLevelsAllocated() const { return num_mipmap_levels_allocated; }


	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount++;
	}

	/// Returns previous reference count
	inline int64 decRefCount()
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

	void textureRefCountDecreasedToOne();


	uint64 getBindlessTextureHandle(); // Get bindless texture handle, and make texture resident if not already.

	static size_t getNumResidentTextures();

	static bool areTextureDimensionsValidForCompression(const Map2D& map);

	GLuint texture_handle;

	static void getGLFormat(Format format, GLint& internal_format, GLenum& gl_format, GLenum& type);
private:
	GLARE_DISABLE_COPY(OpenGLTexture);

	// Create texture, given that xres, yres, MSAA_samples, gl_internal_format etc. have been set.
	void doCreateTexture(ArrayRef<uint8> tex_data, 
		const OpenGLEngine* opengl_engine, // May be null.  Used for querying stuff.
		Wrapping wrapping,
		bool has_mipmaps
	);

	Format format;
	GLint gl_internal_format; // sized GL internal format (num channels)
	GLenum gl_format; // GL format (order of RGBA channels etc..)
	GLenum gl_type; // Type of pixel channel (GL_UNSIGNED_BYTE, GL_HALF_FLOAT etc..)

	Filtering filtering;

	size_t xres, yres; // Will be set after load() etc.. is called, and 0 beforehand.
	int num_mipmap_levels_allocated;
	int MSAA_samples;
	GLenum texture_target; // e.g. GL_TEXTURE_2D or GL_TEXTURE_2D_MULTISAMPLE
public:
	mutable int64 refcount;

	OpenGLEngine* m_opengl_engine;
	OpenGLTextureKey key;

	uint64 bindless_tex_handle;
	bool is_bindless_tex_resident;

	Reference<TextureData> texture_data; // Just stored here for animated textures, so we can upload data for other frames to the GPU texture.

	AllocatedTexViewInfo allocated_tex_view_info;
};


struct TextureParams
{
	TextureParams() : allow_compression(true), use_sRGB(true), use_mipmaps(true), filtering(OpenGLTexture::Filtering_Fancy), wrapping(OpenGLTexture::Wrapping_Repeat) {}

	bool allow_compression;
	bool use_sRGB;
	bool use_mipmaps;
	OpenGLTexture::Filtering filtering;
	OpenGLTexture::Wrapping wrapping;
};


typedef Reference<OpenGLTexture> OpenGLTextureRef;
