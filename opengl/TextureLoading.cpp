/*=====================================================================
TextureLoading.cpp
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "TextureLoading.h"


#include "OpenGLEngine.h"
#include "OpenGLTexture.h"
#include "../graphics/ImageMap.h"
#include "../graphics/DXTCompression.h"
#include "../graphics/CompressedImage.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/IncludeHalf.h"
#include <vector>


#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT			0x8E8F

// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT				0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT			0x83F3


// Load the built texture data into OpenGL.
Reference<OpenGLTexture> TextureLoading::createUninitialisedOpenGLTexture(const TextureData& texture_data, const Reference<OpenGLEngine>& opengl_engine,
	OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping, bool use_sRGB)
{
	const int frame_i = 0;
	
	if(!texture_data.frames[0].compressed_data.empty()) // If the texture data is compressed data we compressed ourselves:
	{
		const size_t W = texture_data.W;
		const size_t H = texture_data.H;
		const size_t bytes_pp = texture_data.bytes_pp;

		const OpenGLTexture::Format format = (bytes_pp == 3) ? OpenGLTexture::Format_Compressed_SRGB_Uint8 : OpenGLTexture::Format_Compressed_SRGBA_Uint8;

		return new OpenGLTexture(W, H, opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0), /*format=*/format, filtering, wrapping);
	}
	else // Else if we don't have data we compressed ourselves, image will not have been processed and is just stored in converted_image.
	{
		runtimeCheck(texture_data.frames[frame_i].converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.

		return createUninitialisedTextureForMap2D(*texture_data.frames[frame_i].converted_image, opengl_engine, filtering, wrapping, use_sRGB);
	}
}


// If an 8-bit map is passed to this function, it should not use compressed formats.
Reference<OpenGLTexture> TextureLoading::createUninitialisedTextureForMap2D(const Map2D& map2d, const Reference<OpenGLEngine>& opengl_engine, 
	OpenGLTexture::Filtering filtering, OpenGLTexture::Wrapping wrapping, bool use_sRGB)
{
	if(dynamic_cast<const ImageMapUInt8*>(&map2d))
	{
		OpenGLTexture::Format format;
		if(map2d.numChannels() <= 3)
			format = (opengl_engine->are_8bit_textures_sRGB && use_sRGB) ? OpenGLTexture::Format_SRGB_Uint8 : OpenGLTexture::Format_RGB_Linear_Uint8;
		else if(map2d.numChannels() == 4)
			format = (opengl_engine->are_8bit_textures_sRGB && use_sRGB) ? OpenGLTexture::Format_SRGBA_Uint8 : OpenGLTexture::Format_RGBA_Linear_Uint8;
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(map2d.numChannels()));

		return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
			format, filtering, wrapping, /*use_mipmaps=*/false
		);
	}
	else if(dynamic_cast<const ImageMap<uint16, UInt16ComponentValueTraits>*>(&map2d))
	{
		// 16-bit textures should have been converted to 8-bit textures already.  (In TextureProcessing::buildTextureData()).
		throw glare::Exception("createUninitialisedTextureForMap2D(): not handling 16-bit textures");
	}
	else if(dynamic_cast<const ImageMapFloat*>(&map2d))
	{
		const ImageMapFloat* imagemap = static_cast<const ImageMapFloat*>(&map2d);

		if(imagemap->getN() == 1)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTexture::Format_Greyscale_Float, 
				filtering, wrapping,  /*use_mipmaps=*/false
			);
		}
		else if(imagemap->getN() == 3)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTexture::Format_RGB_Linear_Float,
				filtering, wrapping,  /*use_mipmaps=*/false
			);
		}
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));
	}
	else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d))
	{
		const ImageMap<half, HalfComponentValueTraits>* imagemap = static_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d);

		if(imagemap->getN() == 1)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTexture::Format_Greyscale_Half,
				filtering, wrapping, /*use_mipmaps=*/false
			);
		}
		else if(imagemap->getN() == 3)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTexture::Format_RGB_Linear_Half,
				filtering, wrapping, /*use_mipmaps=*/false
			);
		}
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));
	}
	else if(dynamic_cast<const CompressedImage*>(&map2d))
	{
		const CompressedImage* compressed_image = static_cast<const CompressedImage*>(&map2d);

		const bool has_mipmaps = compressed_image->mipmap_level_data.size() > 1;

		Reference<OpenGLTexture> opengl_tex;
		size_t bytes_per_block;
		if(compressed_image->gl_internal_format == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
		{
			opengl_tex = new OpenGLTexture(compressed_image->getMapWidth(), compressed_image->getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTexture::Format_Compressed_BC6, 
				filtering, wrapping, has_mipmaps);

			bytes_per_block = 16; // "Both formats use 4x4 pixel blocks, and each block in both compression format is 128-bits in size" - See https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression
		}
		else if(compressed_image->gl_internal_format == GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT)
		{
			opengl_tex = new OpenGLTexture(compressed_image->getMapWidth(), compressed_image->getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				opengl_engine->GL_EXT_texture_sRGB_support ? OpenGLTexture::Format_Compressed_SRGB_Uint8 : OpenGLTexture::Format_Compressed_RGB_Uint8,
				filtering, wrapping, has_mipmaps);
			bytes_per_block = 8;
		}
		else if(compressed_image->gl_internal_format == GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT)
		{
			opengl_tex = new OpenGLTexture(compressed_image->getMapWidth(), compressed_image->getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				opengl_engine->GL_EXT_texture_sRGB_support ? OpenGLTexture::Format_Compressed_SRGBA_Uint8 : OpenGLTexture::Format_Compressed_RGBA_Uint8,
				filtering, wrapping, has_mipmaps);
			bytes_per_block = 16;
		}
		else
			throw glare::Exception("Unhandled internal format for compressed image.");

		for(size_t i=0; i<compressed_image->mipmap_level_data.size(); ++i)
		{
			const size_t level_w = myMax((size_t)1, compressed_image->getMapWidth()  >> i);
			const size_t level_h = myMax((size_t)1, compressed_image->getMapHeight() >> i);

			// Check mipmap_level_data is the correct size for this mipmap level.
			const size_t num_xblocks = Maths::roundedUpDivide(level_w, (size_t)4);
			const size_t num_yblocks = Maths::roundedUpDivide(level_h, (size_t)4);
			const size_t expected_size_B = num_xblocks * num_yblocks * bytes_per_block;
			if(expected_size_B != compressed_image->mipmap_level_data[i].size())
				throw glare::Exception("Compressed image data was wrong size.");
		}

		return opengl_tex;
	}
	else
	{
		throw glare::Exception("Unhandled texture type for texture: (B/pixel: " + toString(map2d.getBytesPerPixel()) + ", numChannels: " + toString(map2d.numChannels()) + ")");
	}
}


// Load the texture data for frame_i into an existing OpenGL texture.  Used for animated images.
void TextureLoading::loadIntoExistingOpenGLTexture(Reference<OpenGLTexture>& opengl_tex, const TextureData& texture_data, size_t frame_i)
{
	opengl_tex->bind();

	const size_t W = texture_data.W;
	const size_t H = texture_data.H;
	const size_t bytes_pp = texture_data.bytes_pp;
	if(!texture_data.frames[frame_i].compressed_data.empty()) // If the texture data is using a compressed texture format:
	{
		for(size_t k=0; k<texture_data.level_offsets.size(); ++k) // For each mipmap level:
		{
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));
			const size_t level_compressed_size = DXTCompression::getCompressedSizeBytes(level_W, level_H, bytes_pp);

			opengl_tex->setMipMapLevelData((int)k, level_W, level_H, /*tex data=*/ArrayRef<uint8>(&texture_data.frames[frame_i].compressed_data[texture_data.level_offsets[k]], level_compressed_size), /*bind_needed=*/false);
		}

		//opengl_tex->unbind();
	}
	else // Else if not using a compressed texture format:
	{
		runtimeCheck(texture_data.frames[frame_i].converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.
		if(dynamic_cast<const ImageMapUInt8*>(texture_data.frames[frame_i].converted_image.ptr()))
		{
			const ImageMapUInt8* image_map_uint8 = static_cast<const ImageMapUInt8*>(texture_data.frames[frame_i].converted_image.ptr());

			opengl_tex->loadIntoExistingTexture(/*mipmap level=*/0, W, H, 
				W * texture_data.frames[frame_i].converted_image->getBytesPerPixel(), // row stride (B)
				ArrayRef<uint8>(image_map_uint8->getData(), image_map_uint8->getDataSize()), /*bind_needed=*/false);
		}
		else
		{
			runtimeCheck(false);
		}
	}
}


void TextureLoading::initialiseTextureLoadingProgress(const std::string& path, const Reference<OpenGLEngine>& opengl_engine, const OpenGLTextureKey& key, bool use_sRGB,
	const Reference<TextureData>& tex_data, OpenGLTextureLoadingProgress& loading_progress)
{
	assert(tex_data.nonNull());

	const OpenGLTexture::Filtering filtering = (tex_data->num_mip_levels > 1) ? OpenGLTexture::Filtering_Fancy : OpenGLTexture::Filtering_Bilinear;

	Reference<OpenGLTexture> opengl_tex = TextureLoading::createUninitialisedOpenGLTexture(*tex_data, opengl_engine.ptr(), filtering, OpenGLTexture::Wrapping_Repeat, use_sRGB);
	opengl_tex->key = key;

	loading_progress.path = path;
	loading_progress.tex_data = tex_data;
	loading_progress.opengl_tex = opengl_tex;
	
	loading_progress.num_mip_levels = tex_data->num_mip_levels;
	loading_progress.next_mip_level = 0;
	loading_progress.level_next_y = 0;
}


// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
// In general xoffset and yoffset need to be a multiple of 4.
// region_h needs to be a multiple of 4, *unless* it's the last vertical region before we get to the end of the image.
static void doPartialUploadOfLevel(OpenGLTextureLoadingProgress& loading_progress, Reference<OpenGLTexture> opengl_tex, 
	size_t level, size_t level_W, size_t level_H, size_t block_size, ArrayRef<uint8> level_data, size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes)
{
	assert(total_bytes_uploaded_in_out < max_total_upload_bytes);

	const size_t rounded_up_H = Maths::roundUpToMultipleOfPowerOf2(level_H, block_size);
	runtimeCheck(rounded_up_H > 0);
	const size_t compressed_bytes_per_row = level_data.size() / rounded_up_H;
	assert(level_data.size() % compressed_bytes_per_row == 0);

	const size_t max_num_bytes = max_total_upload_bytes - total_bytes_uploaded_in_out; // Maximum number of bytes before we hit max_num_upload_bytes

	// Compute max_num_rows: max number of rows we can upload before we hit MAX_UPLOAD_SIZE, approximately.  
	runtimeCheck(compressed_bytes_per_row > 0);
	const size_t raw_num_rows = Maths::roundedUpDivide(max_num_bytes, compressed_bytes_per_row); // Round up so we always upload at least 1 row.
	const size_t max_num_rows = Maths::roundUpToMultipleOfPowerOf2(raw_num_rows, block_size); // Round up to a multiple of the block size if we are using a block-based compression format, 
	// since opengl requires the region height to be a multiple of the format's block height.
	runtimeCheck(max_num_rows > 0);
	
	const size_t cur_y = loading_progress.level_next_y;
	assert(cur_y < level_H);
	const size_t rows_remaining = level_H - cur_y;
	const size_t rows_to_upload = myMin(rows_remaining, max_num_rows);
	runtimeCheck(rows_to_upload > 0);
	//if(rows_to_upload == 0)
	//	return rows_to_upload;

	const size_t src_offset = cur_y * compressed_bytes_per_row;
	const size_t rows_to_upload_size_B = Maths::roundUpToMultipleOfPowerOf2(rows_to_upload, block_size) * compressed_bytes_per_row; // Rounding to account for compression block size.
	runtimeCheck(src_offset + rows_to_upload_size_B <= level_data.size());

	// conPrint("Uploading mip level " + toString(k) + ", rows [" + toString(cur_y) + ", " + toString(cur_y + rows_to_upload) + ") (" + toString(rows_to_upload) + " rows) of tex " + opengl_tex->key.path);

	opengl_tex->loadRegionIntoExistingTexture((int)level, /*x=*/0, /*y=*/cur_y, /*region_w=*/level_W, /*region_h=*/rows_to_upload, /*row stride (B) (not used for compressed textures)=*/compressed_bytes_per_row, 
		ArrayRef<uint8>(level_data.data() + src_offset/*&level_data[src_offset]*/, rows_to_upload_size_B), // tex data
		/*bind_needed=*/false
	);

	total_bytes_uploaded_in_out += rows_to_upload_size_B;
	loading_progress.level_next_y = cur_y + rows_to_upload;
	if(loading_progress.level_next_y >= level_H) // If we have uploaded all rows for this MIP level:
	{
		loading_progress.next_mip_level++; // Advance to row 0 of next MIP level.
		loading_progress.level_next_y = 0;
	}
}


void TextureLoading::partialLoadTextureIntoOpenGL(const Reference<OpenGLEngine>& opengl_engine, OpenGLTextureLoadingProgress& loading_progress,
	size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes)
{
	Reference<TextureData> texture_data = loading_progress.tex_data;
	Reference<OpenGLTexture> opengl_tex = loading_progress.opengl_tex;
	runtimeCheck(texture_data.nonNull());
	runtimeCheck(opengl_tex.nonNull());

	// conPrint("partialLoadTextureIntoOpenGL() (key: " + opengl_tex->key.path + ")");

	opengl_tex->bind();

	const size_t frame_i = 0;
	if(!texture_data->frames[frame_i].compressed_data.empty()) // If the texture data has data we compressed in TextureProcessing
	{
		while((loading_progress.next_mip_level < loading_progress.num_mip_levels) && (total_bytes_uploaded_in_out < max_total_upload_bytes))
		{
			const size_t W = texture_data->W;
			const size_t H = texture_data->H;
				
			const size_t k = loading_progress.next_mip_level;
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

			runtimeCheck(k < texture_data->level_offsets.size());
			runtimeCheck(texture_data->level_offsets[k] < texture_data->frames[frame_i].compressed_data.size());

			const size_t level_data_end = ((k + 1) < texture_data->level_offsets.size()) ? texture_data->level_offsets[k + 1] : texture_data->frames[frame_i].compressed_data.size();
			const size_t level_data_size = level_data_end - texture_data->level_offsets[k];

			doPartialUploadOfLevel(loading_progress, opengl_tex, k, level_W, level_H, /*block size=*/4, 
				ArrayRef<uint8>(texture_data->frames[frame_i].compressed_data.data() + texture_data->level_offsets[k], level_data_size), total_bytes_uploaded_in_out, max_total_upload_bytes);
		}
	}
	else
	{
		runtimeCheck(texture_data->frames[frame_i].converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.

		const Map2D* src_map = texture_data->frames[frame_i].converted_image.ptr();

		// Handle compressed images differently.
		if(dynamic_cast<const CompressedImage*>(src_map))
		{
			const CompressedImage* compressed_im = static_cast<const CompressedImage*>(src_map);

			while((loading_progress.next_mip_level < loading_progress.num_mip_levels) && (total_bytes_uploaded_in_out < max_total_upload_bytes))
			{
				const size_t W = compressed_im->width;
				const size_t H = compressed_im->height;

				const size_t k = loading_progress.next_mip_level;
				runtimeCheck(k < compressed_im->mipmap_level_data.size());
				const glare::AllocatorVector<uint8, 16>& level_data = compressed_im->mipmap_level_data[k];

				const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
				const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

				doPartialUploadOfLevel(loading_progress, opengl_tex, k, level_W, level_H, /*block size=*/4, ArrayRef<uint8>(level_data.data(), level_data.size()), total_bytes_uploaded_in_out, max_total_upload_bytes);
			}
		}
		else // else if src_map is not a CompressedImage:
		{
			while((loading_progress.next_mip_level < loading_progress.num_mip_levels) && (total_bytes_uploaded_in_out < max_total_upload_bytes))
			{
				const size_t W = src_map->getMapWidth();
				const size_t H = src_map->getMapHeight();
				const size_t k = loading_progress.next_mip_level;
				const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
				const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

				const void* src_data = NULL;
				size_t src_data_size = 0;
				if(dynamic_cast<const ImageMap<uint16, UInt16ComponentValueTraits>*>(src_map))
				{
					assert(0); // 16-bit textures should have been converted to 8-bit already.
				}
				else if(dynamic_cast<const ImageMapUInt8*>(src_map))
				{
					src_data		= static_cast<const ImageMapUInt8*>(src_map)->getData();
					src_data_size	= static_cast<const ImageMapUInt8*>(src_map)->getDataSize() * sizeof(uint8);
				}
				else if(dynamic_cast<const ImageMapFloat*>(src_map))
				{
					src_data		= static_cast<const ImageMapFloat*>(src_map)->getData();
					src_data_size	= static_cast<const ImageMapFloat*>(src_map)->getDataSize() * sizeof(float);
				}
				else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map))
				{
					src_data		= static_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map)->getData();
					src_data_size	= static_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map)->getDataSize() * sizeof(half);
				}
				else
				{
					runtimeCheck(0);
				}

				doPartialUploadOfLevel(loading_progress, opengl_tex, k, level_W, level_H, /*block size=*/1, ArrayRef<uint8>((const uint8*)src_data, src_data_size), total_bytes_uploaded_in_out, max_total_upload_bytes);
			}
		}
	}

	if(loading_progress.done())
	{
		if(texture_data->frames.size() > 1)
		{
			// If this is texture data for an animated texture (Gif), then keep it around.
			// We need to keep around animated texture data like this, for now, since during animation different frames will be loaded into the OpenGL texture from the tex data.
			opengl_tex->texture_data = texture_data;

			// Other texture data can be discarded now it has been uploaded to the GPU/OpenGL.
		}

		opengl_engine->addOpenGLTexture(opengl_tex->key, opengl_tex);
	}
}
