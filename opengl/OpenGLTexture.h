/*=====================================================================
OpenGLTexture.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "OpenGLTextureKey.h"
#include "OpenGLMemoryObject.h"
#include "TextureAllocator.h"
#include "../graphics/TextureData.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include "../utils/STLArenaAllocator.h"
#include <vector>
#include <string>


class OpenGLEngine;
class OpenGLTexture;
class TextureData;
class Map2D;


// Made OpenGLTextureKey just be a typedef for std::string for now, to reduce copying of strings.
//struct OpenGLTextureKey
//{
//	OpenGLTextureKey() {}
//	explicit OpenGLTextureKey(const std::string& path_) : path(path_) {}
//	std::string path;
//
//	bool operator < (const OpenGLTextureKey& other) const { return path < other.path; }
//	bool operator == (const OpenGLTextureKey& other) const { return path == other.path; }
//};
//
//
//struct OpenGLTextureKeyHash
//{
//	size_t operator () (const OpenGLTextureKey& key) const
//	{
//		std::hash<std::string> h;
//		return h(key.path);
//	}
//};


std::string getStringForGLInternalFormat(GLint internal_format);
std::string getStringForTextureTarget(GLenum texture_target);


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
		Wrapping_Clamp,
		Wrapping_MirroredRepeat
	};

	OpenGLTexture();

	// Create texture.  Uploads tex_data to texture if tex_data is non-null.
	OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
		ArrayRef<uint8> tex_data,
		OpenGLTextureFormat format,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat,
		bool has_mipmaps = true,
		int MSAA_samples = -1,
		int num_array_images = 0,  // 0 if not a array, >= 1 if an array texture.
		Reference<OpenGLMemoryObject> mem_object = nullptr // non-null if this texture should be backed by a memory object
	); 

	// Create texture, specify exact GL formats
	OpenGLTexture(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine,
		ArrayRef<uint8> tex_data,
		OpenGLTextureFormat format,
		GLint gl_internal_format,
		GLenum gl_format,
		Filtering filtering,
		Wrapping wrapping = Wrapping_Repeat);

	~OpenGLTexture();

	
	void createCubeMap(size_t tex_xres, size_t tex_yres, OpenGLEngine* opengl_engine, const std::vector<const void*>& tex_data, OpenGLTextureFormat format, Filtering filtering);


	//--------------------------------------------- Updating existing texture ---------------------------------------------
	void setMipMapLevelData(int mipmap_level, size_t level_W, size_t level_H, ArrayRef<uint8> tex_data, bool bind_needed);
	
	// Load into a texture that has already had its format, filtering and wrapping modes set.
	void loadIntoExistingTexture(int mipmap_level, size_t tex_xres, size_t tex_yres, size_t row_stride_B, ArrayRef<uint8> tex_data, bool bind_needed);

	// src_tex_data must be a valid pointer, or a PBO must be bound.
	void loadRegionIntoExistingTexture(int mipmap_level, size_t x, size_t y, size_t z, size_t region_w, size_t region_h, size_t region_d, size_t src_row_stride_B, ArrayRef<uint8> src_tex_data, bool bind_needed);

	void setTWrappingEnabled(bool wrapping_enabled);

	void buildMipMaps();

	void setDebugName(const std::string& name);

	void clearRegion2D(int mipmap_level, size_t x, size_t y, size_t region_w, size_t region_h, void* data);
	void clearRegion3D(int mipmap_level, size_t x, size_t y, size_t z, size_t region_w, size_t region_h, size_t region_d, void* data);
	//---------------------------------------------------------------------------------------------------------------------

	void readBackTexture(int mipmap_level, ArrayRef<uint8> buffer);
	
	void bind();
	void unbind();

	bool hasAlpha() const;

	// Will return 0 if texture has not been created yet.
	size_t xRes() const { return xres; }
	size_t yRes() const { return yres; }

	int MSAASamples() const { return MSAA_samples; }

	size_t getTotalStorageSizeB() const { return total_storage_size_B; }

	GLenum getTextureTarget() const { return texture_target; } // e.g. GL_TEXTURE_2D, GL_TEXTURE_2D_MULTISAMPLE or GL_TEXTURE_2D_ARRAY.

	Filtering getFiltering() const { return filtering; }

	int getNumMipMapLevelsAllocated() const { return num_mipmap_levels_allocated; }

	OpenGLTextureFormat getFormat() const { return format; }
	GLint getInternalFormat() const { return gl_internal_format; }

	bool hasMultiFrameTextureData() const { return texture_data && texture_data->isMultiFrame(); }


	/// Increment reference count
	inline void incRefCount() const
	{ 
		assert(refcount >= 0);
		refcount++;
	}

	/// Returns previous reference count
	inline int64 decRefCount()
	{ 
		const int64 prev_ref_count = refcount.decrement();
		assert(prev_ref_count >= 1);

		if(prev_ref_count == 2)
			textureRefCountDecreasedToOne();

		return prev_ref_count;
	}

	inline int64 getRefCount() const
	{ 
		assert(refcount >= 0);
		return refcount; 
	}

	void textureRefCountDecreasedToOne();

	uint64 getBindlessTextureHandle(); // Create bindless texture handle if not created already, and make texture resident if not already.  Then return handle.

	void createBindlessTextureHandle(); // Get bindless texture handle, don't make resident.

	void makeNonResidentIfResident();

	static size_t getNumResidentTextures();

	static bool areTextureDimensionsValidForCompression(const Map2D& map);

	GLuint texture_handle;

	static void getGLFormat(OpenGLTextureFormat format, GLint& internal_format, GLenum& gl_format, GLenum& type);
private:
	size_t computeTotalStorageSizeB() const;

	GLARE_DISABLE_COPY(OpenGLTexture);

	// Create texture, given that xres, yres, MSAA_samples, gl_internal_format etc. have been set.
	void doCreateTexture(ArrayRef<uint8> tex_data, 
		const OpenGLEngine* opengl_engine, // May be null.  Used for querying stuff.
		bool has_mipmaps
	);

	OpenGLTextureFormat format;
	GLint gl_internal_format; // sized GL internal format (num channels)
	GLenum gl_format; // GL format (order of RGBA channels etc..)
	GLenum gl_type; // Type of pixel channel (GL_UNSIGNED_BYTE, GL_HALF_FLOAT etc..)

	Filtering filtering;
	Wrapping wrapping;

	size_t xres, yres; // Will be set after load() etc.. is called, and 0 beforehand.
	int num_array_images;
	int num_mipmap_levels_allocated;
	int MSAA_samples;
	Reference<OpenGLMemoryObject> mem_object;
	GLenum texture_target; // e.g. GL_TEXTURE_2D, GL_TEXTURE_2D_MULTISAMPLE or GL_TEXTURE_2D_ARRAY
	size_t total_storage_size_B;
public:
	mutable glare::AtomicInt refcount;

	OpenGLEngine* m_opengl_engine;
	OpenGLTextureKey key;
	bool inserted_into_opengl_textures; // Is this texture currently an element of OpenGLEngine::opengl_textures?  Used in textureRefCountDecreasedToOne().

	uint64 bindless_tex_handle;
	bool is_bindless_tex_resident;

	Reference<TextureData> texture_data; // Just stored here for animated textures, so we can upload data for other frames to the GPU texture.

	AllocatedTexViewInfo allocated_tex_view_info;
};


struct TextureParams
{
	TextureParams() : allow_compression(true), use_sRGB(true), use_mipmaps(true), convert_float_to_half(true), filtering(OpenGLTexture::Filtering_Fancy), wrapping(OpenGLTexture::Wrapping_Repeat) {}

	bool allow_compression;
	bool use_sRGB;
	bool use_mipmaps;
	bool convert_float_to_half;
	OpenGLTexture::Filtering filtering;
	OpenGLTexture::Wrapping wrapping;
};


typedef Reference<OpenGLTexture> OpenGLTextureRef;
