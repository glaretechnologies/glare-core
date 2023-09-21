/*=====================================================================
TextureAllocator.cpp
--------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "TextureAllocator.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "OpenGLMeshRenderData.h"
#include "graphics/TextureProcessing.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT				0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT			0x83F3

// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
#define GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT			0x8C4C
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT		0x8C4D
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT		0x8C4E
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT		0x8C4F


static AllocatedTexViewInfo tryAllocFromArray(TextureViewTexArray* tex_array, int array_index, GLint gl_internal_format, int num_mipmap_levels, TextureViewTexArrayKey& key)
{
	if(!tex_array->free_indices.empty())
	{
		const int index = tex_array->free_indices.back();
		tex_array->free_indices.pop_back();

		GLuint subtexture_handle;
		glGenTextures(1, &subtexture_handle);
		glTextureView(subtexture_handle, GL_TEXTURE_2D, /*orig texture=*/tex_array->texture_handle, /*internal format=*/gl_internal_format, /*min MIPMAP level=*/0, /*num MIPMAP levels=*/num_mipmap_levels, 
			/*min layer=*/index, /*num layers=*/1);

		AllocatedTexViewInfo info;
		info.array_key = key;
		info.array_index = array_index;
		info.index = index;
		info.texture_handle = subtexture_handle;
		return info;
	}
	else
	{
		AllocatedTexViewInfo info;
		info.texture_handle = 0;
		return info;
	}
}


AllocatedTexViewInfo TextureAllocator::allocTextureView(GLint gl_internal_format, int xres, int yres, int num_mipmap_levels)
{
	TextureViewTexArrayKey key;
	key.xres = xres;
	key.internal_format_class = gl_internal_format;

	// Some internal formats are in the same compatibility class, share arrays for these.  See https://www.khronos.org/opengl/wiki/Texture_Storage#Texture_views
	if(gl_internal_format == GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT || gl_internal_format == GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT)
		key.internal_format_class = GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT; // GL_S3TC_DXT1_RGB class
	else if(gl_internal_format == GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT || gl_internal_format == GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
		key.internal_format_class = GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT; // GL_S3TC_DXT5_RGBA class

	if(texture_arrays_map.count(key) == 0)
		conPrint("Creating new TextureViewTexArray for xres: " + toString(xres) + " and gl_internal_format " + getStringForGLInternalFormat(gl_internal_format));

	std::vector<TextureViewTexArray*>& texture_arrays = texture_arrays_map[key];


	// Try and allocate from existing texture array
	for(size_t i=0; i<texture_arrays.size(); ++i)
	{
		AllocatedTexViewInfo info = tryAllocFromArray(texture_arrays[i], /*array index=*/(int)i, gl_internal_format, num_mipmap_levels, key);
		if(info.texture_handle != 0) // If alloc succeeded:
			return info;
	}

	// If we got here, there were no free indices,
	// so create new texture array.
	const int NUM_LAYERS = 256; // Num textures per array

	conPrint("Creating new texture array for xres: " + toString(xres) + " and gl_internal_format " + getStringForGLInternalFormat(gl_internal_format) + 
		", current num textures with key = " + toString(texture_arrays.size() * NUM_LAYERS));

	size_t total_num_tex_arrays = 0;
	for(auto it = texture_arrays_map.begin(); it != texture_arrays_map.end(); ++it)
	{
		total_num_tex_arrays += it->second.size();
	}
	printVar(total_num_tex_arrays);

	TextureViewTexArray* tex_array = new TextureViewTexArray();
	texture_arrays.push_back(tex_array);


	// Allocate 2D texture array, that forms the backing texture for our individual 2D textures
	glGenTextures(1, &tex_array->texture_handle);
	assert(tex_array->texture_handle != 0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex_array->texture_handle);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, /*levels=*/num_mipmap_levels, /*internal format=*/gl_internal_format, /*width=*/xres, /*height=*/yres, /*depth (num array layers)=*/NUM_LAYERS);


	tex_array->next_layer = 0;
	tex_array->num_layers = NUM_LAYERS;
	tex_array->free_indices.resize(NUM_LAYERS);
	for(int i=0; i<NUM_LAYERS; ++i)
		tex_array->free_indices[i] = i;


	// Do actual allocation again of individual layer
	AllocatedTexViewInfo info = tryAllocFromArray(tex_array, /*array index=*/(int)texture_arrays.size() - 1,  gl_internal_format, num_mipmap_levels, key);
	assert(info.texture_handle != 0);
	return info;
}


void TextureAllocator::freeTextureView(AllocatedTexViewInfo& info)
{
	conPrint("Freeing texture view");

	assert(texture_arrays_map.count(info.array_key) > 0);

	std::vector<TextureViewTexArray*>& texture_arrays = texture_arrays_map[info.array_key];

	assert(info.array_index < texture_arrays.size());

	texture_arrays[info.array_index]->free_indices.push_back(info.index);


	glDeleteTextures(1, &info.texture_handle);
}