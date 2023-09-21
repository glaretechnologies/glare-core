/*=====================================================================
TextureAllocator.h
------------------
Copyright Glare Technologies Limited 2023 -


Allocates textures as texture views into a texture array.

Can be used to reduce the number of actual textures.
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../graphics/TextureData.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include "../utils/AllocatorVector.h"
#include <vector>
#include <string>
#include <map>


// Currently disabled, this texture allocator is only needed on AMD, and multi-draw-indirect rendering seems busted on AMD anyway, so we don't need bindless textures on AMD,
// especially since the drivers crash hard with too many bindless textures resident.
#define USE_TEXTURE_VIEWS 0


struct TextureViewTexArray
{
	GLuint texture_handle;
	int num_layers;
	int next_layer;
	//std::set<int> free_indices;
	std::vector<int> free_indices;
};


struct TextureViewTexArrayKey
{
	int xres;
	GLint internal_format_class;

	bool operator < (const TextureViewTexArrayKey& other) const
	{
		if(xres < other.xres)
			return true;
		else if(xres > other.xres)
			return false;
		else
			return internal_format_class < other.internal_format_class;
	}
};


struct AllocatedTexViewInfo
{
	TextureViewTexArrayKey array_key;
	int array_index; // Index of array
	int index; // Index in array
	GLuint texture_handle;
};


class TextureAllocator
{
public:
	AllocatedTexViewInfo allocTextureView(GLint gl_internal_format, int xres, int yres, int num_mipmap_levels);
	void freeTextureView(AllocatedTexViewInfo& info);

private:
	std::map<TextureViewTexArrayKey, std::vector<TextureViewTexArray*>> texture_arrays_map;	
};
