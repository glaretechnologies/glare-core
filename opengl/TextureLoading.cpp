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
#include "../utils/FileUtils.h"
#include "../maths/CheckedMaths.h"
#include <tracy/Tracy.hpp>
#include <vector>


Reference<OpenGLTexture> TextureLoading::createUninitialisedOpenGLTexture(const TextureData& texture_data, const Reference<OpenGLEngine>& opengl_engine, const TextureParams& texture_params)
{
	ZoneScoped; // Tracy profiler
	if(!texture_data.converted_image) // If the texture data is processed data we processed ourselves:
	{
		const size_t W = texture_data.W;
		const size_t H = texture_data.H;

		OpenGLTextureFormat format = texture_data.format;

		// Remove or add sRGB non-linearity as needed
		if(!texture_params.use_sRGB)
		{
			if(format == OpenGLTextureFormat::Format_SRGB_Uint8)
				format = OpenGLTextureFormat::Format_RGB_Linear_Uint8;
			else if(format == OpenGLTextureFormat::Format_SRGBA_Uint8)
				format = OpenGLTextureFormat::Format_RGBA_Linear_Uint8;

			else if(format == OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_DXT_RGB_Uint8;
			else if(format == OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_DXT_RGBA_Uint8;

			else if(format == OpenGLTextureFormat::Format_Compressed_ETC2_SRGB_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_ETC2_RGB_Uint8;
			else if(format == OpenGLTextureFormat::Format_Compressed_ETC2_SRGBA_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_ETC2_RGBA_Uint8;
		}
		else
		{
			if(format == OpenGLTextureFormat::Format_RGB_Linear_Uint8)
				format = OpenGLTextureFormat::Format_SRGB_Uint8;
			else if(format == OpenGLTextureFormat::Format_RGBA_Linear_Uint8)
				format = OpenGLTextureFormat::Format_SRGBA_Uint8;

			else if(format == OpenGLTextureFormat::Format_Compressed_DXT_RGB_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8;
			else if(format == OpenGLTextureFormat::Format_Compressed_DXT_RGBA_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8;

			else if(format == OpenGLTextureFormat::Format_Compressed_ETC2_RGB_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_ETC2_SRGB_Uint8;
			else if(format == OpenGLTextureFormat::Format_Compressed_ETC2_RGBA_Uint8)
				format = OpenGLTextureFormat::Format_Compressed_ETC2_SRGBA_Uint8;
		}

		/*OpenGLTextureFormat format;
		if(texture_data.data_is_compressed)
		{
			if(texture_params.use_sRGB)
				format = (bytes_pp == 3) ? OpenGLTexture::Format_Compressed_SRGB_Uint8 : OpenGLTexture::Format_Compressed_SRGBA_Uint8;
			else
				format = (bytes_pp == 3) ? OpenGLTexture::Format_Compressed_RGB_Uint8 : OpenGLTexture::Format_Compressed_RGBA_Uint8;
		}
		else
		{
			if(texture_params.use_sRGB)
				format = (bytes_pp == 3) ? OpenGLTexture::Format_SRGB_Uint8 : OpenGLTexture::Format_SRGBA_Uint8;
			else
				format = (bytes_pp == 3) ? OpenGLTexture::Format_RGB_Linear_Uint8 : OpenGLTexture::Format_RGBA_Linear_Uint8;
		}*/

		if(texture_data.isArrayTexture())
		{
			// Array texture!
			return new OpenGLTexture(W, H, opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0), /*format=*/format, texture_params.filtering, texture_params.wrapping, 
				/*has mipmaps=*/texture_data.numMipLevels() > 1, /*MSAA samples=*/-1, /*num array images=*/(int)texture_data.num_array_images);
		}
		else
		{
			return new OpenGLTexture(W, H, opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0), /*format=*/format, texture_params.filtering, texture_params.wrapping);
		}
	}
	else // Else if we don't have data we processed ourselves, image will not have been processed and is just stored in converted_image.
	{
		runtimeCheck(texture_data.converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.

		return createUninitialisedTextureForMap2D(*texture_data.converted_image, opengl_engine, texture_params);
	}
}


// If an 8-bit map is passed to this function, it should not use compressed formats.
Reference<OpenGLTexture> TextureLoading::createUninitialisedTextureForMap2D(const Map2D& map2d, const Reference<OpenGLEngine>& opengl_engine, const TextureParams& texture_params)
{
	if(dynamic_cast<const ImageMapUInt8*>(&map2d))
	{
		OpenGLTextureFormat format;
		if(map2d.numChannels() == 1)
			format = OpenGLTextureFormat::Format_Greyscale_Uint8;
		else if(map2d.numChannels() <= 3)
			format = texture_params.use_sRGB ? OpenGLTextureFormat::Format_SRGB_Uint8 : OpenGLTextureFormat::Format_RGB_Linear_Uint8;
		else if(map2d.numChannels() == 4)
			format = texture_params.use_sRGB ? OpenGLTextureFormat::Format_SRGBA_Uint8 : OpenGLTextureFormat::Format_RGBA_Linear_Uint8;
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(map2d.numChannels()));

		return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
			format, texture_params.filtering, texture_params.wrapping, /*use_mipmaps=*/false
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

		const bool use_mipmaps = texture_params.filtering == OpenGLTexture::Filtering_Fancy;

		if(imagemap->getN() == 1)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_Greyscale_Float, 
				texture_params.filtering, texture_params.wrapping,  /*use_mipmaps=*/use_mipmaps
			);
		}
		else if(imagemap->getN() == 3)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_RGB_Linear_Float,
				texture_params.filtering, texture_params.wrapping,  /*use_mipmaps=*/use_mipmaps
			);
		}
		else if(imagemap->getN() == 4)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_RGBA_Linear_Float,
				texture_params.filtering, texture_params.wrapping,  /*use_mipmaps=*/use_mipmaps
			);
		}
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));
	}
	else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d))
	{
		const ImageMap<half, HalfComponentValueTraits>* imagemap = static_cast<const ImageMap<half, HalfComponentValueTraits>*>(&map2d);

		const bool use_mipmaps = texture_params.filtering == OpenGLTexture::Filtering_Fancy;

		if(imagemap->getN() == 1)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_Greyscale_Half,
				texture_params.filtering, texture_params.wrapping, /*use_mipmaps=*/use_mipmaps
			);
		}
		else if(imagemap->getN() == 3)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_RGB_Linear_Half,
				texture_params.filtering, texture_params.wrapping, /*use_mipmaps=*/use_mipmaps
			);
		}
		else if(imagemap->getN() == 4)
		{
			return new OpenGLTexture(map2d.getMapWidth(), map2d.getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
				OpenGLTextureFormat::Format_RGBA_Linear_Half,
				texture_params.filtering, texture_params.wrapping, /*use_mipmaps=*/use_mipmaps
			);
		}
		else
			throw glare::Exception("Texture has unhandled number of components: " + toString(imagemap->getN()));
	}
	else if(dynamic_cast<const CompressedImage*>(&map2d))
	{
		const CompressedImage* compressed_image = static_cast<const CompressedImage*>(&map2d);

		const bool has_mipmaps = compressed_image->texture_data->numMipLevels() > 1;

		return new OpenGLTexture(compressed_image->getMapWidth(), compressed_image->getMapHeight(), opengl_engine.ptr(), /*tex data=*/ArrayRef<uint8>(NULL, 0),
			compressed_image->texture_data->format, 
			texture_params.filtering, texture_params.wrapping, has_mipmaps);
	}
	else
	{
		//throw glare::Exception("Unhandled texture type for texture: (B/pixel: " + toString(map2d.getBytesPerPixel()) + ", numChannels: " + toString(map2d.numChannels()) + ")");
		throw glare::Exception("Unhandled texture type for texture: (numChannels: " + toString(map2d.numChannels()) + ")");
	}
}


// Load the texture data for frame_i into an existing OpenGL texture.  Used for animated images.
void TextureLoading::loadIntoExistingOpenGLTexture(Reference<OpenGLTexture>& opengl_tex, const TextureData& texture_data, size_t frame_i)
{
	ZoneScoped; // Tracy profiler

	opengl_tex->bind();

	const size_t W = texture_data.W;
	const size_t H = texture_data.H;
	if(!texture_data.mipmap_data.empty()) // If the texture data has been processed into mip levels:
	{
		for(size_t k=0; k<texture_data.level_offsets.size(); ++k) // For each mipmap level:
		{
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));
			const size_t level_offset = texture_data.level_offsets[k].offset + texture_data.frame_size_B * frame_i; // Offset for level k and frame frame_i.
			const size_t level_size   = texture_data.level_offsets[k].level_size;

			runtimeCheck(CheckedMaths::addUnsignedInts(level_offset, level_size) <= texture_data.mipmap_data.size());

			Timer timer;
			opengl_tex->setMipMapLevelData((int)k, level_W, level_H, /*tex data=*/ArrayRef<uint8>(&texture_data.mipmap_data[level_offset], level_size), /*bind_needed=*/false);
			conPrint("Setting mip level " + toString(k) + " took " + timer.elapsedStringMSWIthNSigFigs());
		}
	}
	else // Else if not using a compressed texture format:
	{
		runtimeCheck(texture_data.converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.
		if(const ImageMapUInt8* image_map_uint8 = dynamic_cast<const ImageMapUInt8*>(texture_data.converted_image.ptr()))
		{
			opengl_tex->loadIntoExistingTexture(/*mipmap level=*/0, W, H, 
				W * texture_data.converted_image->numChannels(), // row stride (B)
				ArrayRef<uint8>(image_map_uint8->getData(), image_map_uint8->getDataSize()), /*bind_needed=*/false);
		}
		else
		{
			runtimeCheck(false);
		}
	}

	opengl_tex->unbind();
}


void TextureLoading::initialiseTextureLoadingProgress(const std::string& path, const Reference<OpenGLEngine>& opengl_engine, const OpenGLTextureKey& key, const TextureParams& texture_params, 
	const Reference<TextureData>& tex_data, OpenGLTextureLoadingProgress& loading_progress)
{
	ZoneScoped; // Tracy profiler
	assert(tex_data.nonNull());

	Reference<OpenGLTexture> opengl_tex = TextureLoading::createUninitialisedOpenGLTexture(*tex_data, opengl_engine.ptr(), texture_params);
	opengl_tex->key = key;
#if BUILD_TESTS
	// Only call opengl_tex->setDebugName when running in RenderDoc, can be slow otherwise.
	if(opengl_engine->running_in_renderdoc)
		opengl_tex->setDebugName(FileUtils::getFilename(path).substr(0, 100)); // AMD drivers generate errors if the debug name is too long.
#endif

	loading_progress.path = path;
	loading_progress.tex_data = tex_data;
	loading_progress.opengl_tex = opengl_tex;
	
	loading_progress.num_mip_levels = myMin((size_t)opengl_tex->getNumMipMapLevelsAllocated(), tex_data->numMipLevels());
	loading_progress.next_mip_level = 0;
	loading_progress.level_next_y = 0;
}


// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
// In general xoffset and yoffset need to be a multiple of 4.
// region_h needs to be a multiple of 4, *unless* it's the last vertical region before we get to the end of the image.
static void doPartialUploadOfLevel(OpenGLTextureLoadingProgress& loading_progress, Reference<OpenGLTexture> opengl_tex, bool data_is_compressed,
	size_t level, size_t level_W, size_t level_H, size_t level_D, size_t block_size, ArrayRef<uint8> level_data, size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes)
{
	assert(total_bytes_uploaded_in_out < max_total_upload_bytes);
	const size_t max_num_bytes = max_total_upload_bytes - total_bytes_uploaded_in_out; // Maximum number of bytes before we hit max_num_upload_bytes

	if(data_is_compressed)
	{
		if(level_D > 1)
		{
			// Just upload whole image/slices at a time for now.
			const size_t slice_size_B = level_data.size() / level_D;
			const size_t slices_remaining = level_D - loading_progress.level_next_z;
			runtimeCheck(slice_size_B > 0);
			const size_t slices_to_upload = myMin(slices_remaining, Maths::roundedUpDivide(max_num_bytes, slice_size_B));
			const size_t src_offset = slice_size_B * loading_progress.level_next_z;
			opengl_tex->loadRegionIntoExistingTexture((int)level, /*x=*/0, /*y=*/0, /*z=*/loading_progress.level_next_z, /*region_w=*/level_W, /*region_h=*/level_H, /*region depth=*/slices_to_upload, /*row stride (B) (not used for compressed textures)=*/4, 
				level_data.getSliceChecked(src_offset, slice_size_B * slices_to_upload), // tex data
				/*bind_needed=*/false
			);

			total_bytes_uploaded_in_out += slice_size_B * slices_to_upload;

			loading_progress.level_next_z += slices_to_upload;
			if(loading_progress.level_next_z >= level_D)
			{
				loading_progress.next_mip_level++; // Advance to image/slice 0 of next MIP level.
				loading_progress.level_next_z = 0;
			}
		}
		else
		{
			const size_t rounded_up_H = Maths::roundUpToMultipleOfPowerOf2(level_H, block_size);
			runtimeCheck(rounded_up_H > 0);
			const size_t compressed_bytes_per_row = level_data.size() / rounded_up_H;
			assert(level_data.size() % compressed_bytes_per_row == 0);

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

			opengl_tex->loadRegionIntoExistingTexture((int)level, /*x=*/0, /*y=*/cur_y, /*z=*/0, /*region_w=*/level_W, /*region_h=*/rows_to_upload, /*region depth=*/level_D, /*row stride (B) (not used for compressed textures)=*/compressed_bytes_per_row, 
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
	}
	else
	{
		if(level_D > 1)
		{
			throw glare::Exception("Loading uncompressed array textures not currently supported.");
			// TODO
		}
		else
		{
			const size_t bytes_per_row = level_data.size() / level_H;
			assert(level_data.size() % bytes_per_row == 0);

			// Compute max_num_rows: max number of rows we can upload before we hit MAX_UPLOAD_SIZE, approximately.  
			runtimeCheck(bytes_per_row > 0);
			const size_t max_num_rows = Maths::roundedUpDivide(max_num_bytes, bytes_per_row); // Round up so we always upload at least 1 row.
			runtimeCheck(max_num_rows > 0);

			const size_t cur_y = loading_progress.level_next_y;
			assert(cur_y < level_H);
			const size_t rows_remaining = level_H - cur_y;
			const size_t rows_to_upload = myMin(rows_remaining, max_num_rows);
			runtimeCheck(rows_to_upload > 0);

			const size_t src_offset = cur_y * bytes_per_row;
			const size_t rows_to_upload_size_B = rows_to_upload * bytes_per_row;
			runtimeCheck(src_offset + rows_to_upload_size_B <= level_data.size());

			// conPrint("Uploading mip level " + toString(k) + ", rows [" + toString(cur_y) + ", " + toString(cur_y + rows_to_upload) + ") (" + toString(rows_to_upload) + " rows) of tex " + opengl_tex->key.path);

			opengl_tex->loadRegionIntoExistingTexture((int)level, /*x=*/0, /*y=*/cur_y, /*z=*/0, /*region_w=*/level_W, /*region_h=*/rows_to_upload, /*region depth=*/level_D, /*row stride (B)=*/bytes_per_row, 
				ArrayRef<uint8>(level_data.data() + src_offset, rows_to_upload_size_B), // tex data
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
	}
}


void TextureLoading::partialLoadTextureIntoOpenGL(OpenGLTextureLoadingProgress& loading_progress,
	size_t& total_bytes_uploaded_in_out, size_t max_total_upload_bytes)
{
	ZoneScoped; // Tracy profiler
	Reference<TextureData> texture_data = loading_progress.tex_data;
	Reference<OpenGLTexture> opengl_tex = loading_progress.opengl_tex;
	runtimeCheck(texture_data.nonNull());
	runtimeCheck(opengl_tex.nonNull());

	// conPrint("partialLoadTextureIntoOpenGL() (key: " + opengl_tex->key.path + ")");

	opengl_tex->bind();

	// NOTE: assuming we are always loading from frame 0.

	if(!texture_data->converted_image) // If the texture data has data we compressed in TextureProcessing
	{
		while((loading_progress.next_mip_level < loading_progress.num_mip_levels) && (total_bytes_uploaded_in_out < max_total_upload_bytes))
		{
			const size_t W = texture_data->W;
			const size_t H = texture_data->H;
				
			const size_t k = loading_progress.next_mip_level;
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

			runtimeCheck(k < texture_data->level_offsets.size());

			const size_t level_offset = texture_data->level_offsets[k].offset;
			const size_t level_size   = texture_data->level_offsets[k].level_size;
			
			runtimeCheck(level_offset < texture_data->mipmap_data.size());
			runtimeCheck(level_offset + level_size <= texture_data->mipmap_data.size());

			const size_t level_D = texture_data->D;

			const ArrayRef<uint8> level_data(texture_data->mipmap_data.data() + level_offset, level_size);
			
			doPartialUploadOfLevel(loading_progress, opengl_tex, texture_data->isCompressed(), k, level_W, level_H, level_D, /*block size=*/4, 
				level_data, total_bytes_uploaded_in_out, max_total_upload_bytes);
		}
	}
	else
	{
		runtimeCheck(texture_data->converted_image.nonNull()); // We should have a reference to the original (or converted) uncompressed image.

		const Map2D* src_map = texture_data->converted_image.ptr();
		while((loading_progress.next_mip_level < loading_progress.num_mip_levels) && (total_bytes_uploaded_in_out < max_total_upload_bytes))
		{
			const size_t W = src_map->getMapWidth();
			const size_t H = src_map->getMapHeight();
			const size_t k = loading_progress.next_mip_level;
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));
			const size_t level_D = texture_data->D;

			const void* src_data = NULL;
			size_t src_data_size = 0;
			if(dynamic_cast<const ImageMap<uint16, UInt16ComponentValueTraits>*>(src_map))
			{
				assert(0); // 16-bit textures should have been converted to 8-bit already.
			}
			else if(dynamic_cast<const ImageMapUInt8*>(src_map))
			{
				src_data		= static_cast<const ImageMapUInt8*>(src_map)->getData();
				src_data_size	= static_cast<const ImageMapUInt8*>(src_map)->getDataSizeB();
			}
			else if(dynamic_cast<const ImageMapFloat*>(src_map))
			{
				src_data		= static_cast<const ImageMapFloat*>(src_map)->getData();
				src_data_size	= static_cast<const ImageMapFloat*>(src_map)->getDataSizeB();
			}
#ifndef NO_EXR_SUPPORT
			else if(dynamic_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map))
			{
				src_data		= static_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map)->getData();
				src_data_size	= static_cast<const ImageMap<half, HalfComponentValueTraits>*>(src_map)->getDataSizeB();
			}
#endif
			else
			{
				runtimeCheck(0);
			}

			doPartialUploadOfLevel(loading_progress, opengl_tex, /*data_is_compressed=*/false, k, level_W, level_H, level_D, /*block size=*/1, ArrayRef<uint8>((const uint8*)src_data, src_data_size), total_bytes_uploaded_in_out, max_total_upload_bytes);
		}
	}

	if(loading_progress.done())
	{
		// Now that we have loaded all the texture data into OpenGL, if we didn't compute all mipmap level data ourselves, and we need it for trilinear filtering, then get the driver to do it.
		if((texture_data->W > 1 || texture_data->H > 1) && texture_data->numMipLevels() == 1 && opengl_tex->getFiltering() == OpenGLTexture::Filtering_Fancy)
		{
			conPrint("INFO: Getting driver to build MipMaps for texture '" + loading_progress.path + "'...");
			opengl_tex->buildMipMaps();
		}


		if(texture_data->isMultiFrame())
		{
			// If this is texture data for an animated texture (Gif), then keep it around.
			// We need to keep around animated texture data like this, for now, since during animation different frames will be loaded into the OpenGL texture from the tex data.
			opengl_tex->texture_data = texture_data;

			// Other texture data can be discarded now it has been uploaded to the GPU/OpenGL.
		}
	}
}
