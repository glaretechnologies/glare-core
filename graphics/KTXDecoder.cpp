/*=====================================================================
KTXDecoder.cpp
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "KTXDecoder.h"


#include "imformatdecoder.h"
#include "CompressedImage.h"
#include "VKFormat.h"
#include "IncludeOpenGL.h"
#include "../utils/FileInStream.h"
#include "../utils/FileOutStream.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Vector.h"
#include "../utils/PlatformUtils.h"
#include "../utils/BufferViewInStream.h"
#include "../utils/ArrayRef.h"
#include "../maths/CheckedMaths.h"
#include "../maths/mathstypes.h"
#include <memory.h>
#include <limits>
#include <zstd.h>


#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT					0x8E8F
// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT						0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT					0x83F3

// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt
#define GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT					0x8C4C
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT				0x8C4D
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT				0x8C4E
#define GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT				0x8C4F


// Adapted from https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
// NOTE: untested
static inline uint32 swapByteOrder(const uint32 x)
{
	return (x >> 24) |
		((x << 8) & 0x00FF0000u) |
		((x >> 8) & 0x0000FF00u) |
		(x << 24);
}


static inline uint32 readUInt32(BufferViewInStream& file, bool swap_endianness)
{
	const uint32 x = file.readUInt32();
	return swap_endianness ? swapByteOrder(x) : x;
}


static uint8 ktx_file_id [12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
static uint8 ktx2_file_id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };


// throws ImFormatExcep on failure
Reference<Map2D> KTXDecoder::decode(const std::string& path, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(path);
		return decodeFromBuffer(file.fileData(), file.fileSize(), mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> KTXDecoder::decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator)
{
	try
	{
		BufferViewInStream file(ArrayRef<uint8>((const uint8*)data, size));

		uint8 identifier[12];
		file.readData(identifier, 12);
		if(memcmp(identifier, ktx_file_id, 12) != 0)
			throw glare::Exception("Invalid file id");

		const uint32 endianness = file.readUInt32();
		bool swap_endianness;
		if(endianness == 0x04030201u)
			swap_endianness = false;
		else if(endianness == 0x01020304u)
			swap_endianness = true;
		else
			throw glare::Exception("invalid endianness value.");

		/*const uint32 glType =*/ readUInt32(file, swap_endianness);
		/*const uint32 glTypeSize =*/ readUInt32(file, swap_endianness);
		/*const uint32 glFormat =*/ readUInt32(file, swap_endianness);
		const uint32 glInternalFormat = readUInt32(file, swap_endianness);
		/*const uint32 glBaseInternalFormat =*/ readUInt32(file, swap_endianness);
		const uint32 pixelWidth = readUInt32(file, swap_endianness);
		const uint32 pixelHeight = readUInt32(file, swap_endianness);
		const uint32 pixelDepth = readUInt32(file, swap_endianness);
		const uint32 numberOfArrayElements = readUInt32(file, swap_endianness);
		const uint32 numberOfFaces = readUInt32(file, swap_endianness);
		const uint32 numberOfMipmapLevels = readUInt32(file, swap_endianness);
		const uint32 bytesOfKeyValueData = readUInt32(file, swap_endianness);

		if(numberOfArrayElements > 0)
			throw glare::Exception("numberOfArrayElements > 0 not supported.");

		if(pixelDepth > 1)
			throw glare::Exception("pixelDepth > 1 not supported.");

		if(numberOfFaces != 1)
			throw glare::Exception("numberOfFaces != 1 not supported.");

		if(pixelWidth > 1000000)
			throw ImFormatExcep("Invalid width: " + toString(pixelWidth));
		if(pixelHeight > 1000000)
			throw ImFormatExcep("Invalid height: " + toString(pixelHeight));

		const size_t max_num_pixels = 1 << 27;
		if(((size_t)pixelWidth * (size_t)pixelHeight) > max_num_pixels)
			throw ImFormatExcep("invalid width, height, num_images: (too many pixels): " + toString(pixelWidth) + ", " + toString(pixelHeight));

		// Skip key-value data
		file.advanceReadIndex(bytesOfKeyValueData);

		const uint32 use_num_mipmap_levels = myMax(1u, numberOfMipmapLevels);
		if(use_num_mipmap_levels > 32)
			throw glare::Exception("Too many mipmap levels.");

		if(use_num_mipmap_levels > TextureData::computeNumMipLevels(pixelWidth, pixelHeight))
			throw glare::Exception("Too many mipmap levels.");


		OpenGLTextureFormat format;
		if(glInternalFormat == GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_RGB_Uint8;
		}
		else if(glInternalFormat == GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_RGBA_Uint8;
		}
		else if(glInternalFormat == GL_EXT_COMPRESSED_SRGB_S3TC_DXT1_EXT)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8;
		}
		else if(glInternalFormat == GL_EXT_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8;
		}
		else if(glInternalFormat == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
		{
			format = OpenGLTextureFormat::Format_Compressed_BC6H;
		}
		else
			throw glare::Exception("unhandled glInternalFormat: " + toString(glInternalFormat));

		CompressedImageRef image = new CompressedImage(pixelWidth, pixelHeight, format);
		image->setAllocator(mem_allocator);
		image->texture_data->level_offsets.resize(use_num_mipmap_levels);


		// Compute level offsets and total data size.
		size_t offset = 0;
		for(uint32 lvl = 0; lvl < use_num_mipmap_levels; ++lvl)
		{
			const size_t expected_blocks = TextureData::computeNum4PixelBlocksForLevel(pixelWidth, pixelHeight, lvl);
			const size_t expected_level_size = expected_blocks * bytesPerBlock(image->texture_data->format);

			image->texture_data->level_offsets[lvl].offset = offset;
			image->texture_data->level_offsets[lvl].level_size = expected_level_size;

			offset += expected_level_size;
		}
		const size_t total_data_size = offset;

		
		image->texture_data->mipmap_data.resize(total_data_size);
		image->texture_data->frame_size_B = total_data_size;
		MutableArrayRef<uint8> data_ref(image->texture_data->mipmap_data.data(), image->texture_data->mipmap_data.size());

		// for each mipmap_level in numberOfMipmapLevels
		for(uint32 lvl = 0; lvl < use_num_mipmap_levels; ++lvl)
		{
			const uint32 image_size = readUInt32(file, swap_endianness);

			if(image_size != image->texture_data->level_offsets[lvl].level_size)
				throw glare::Exception("Unexpected mip image size.");

			if(!file.canReadNBytes((size_t)image_size))
				throw glare::Exception("MIP image size is too large");

			file.readDataChecked(/*dest buf=*/data_ref, /*dest offset=*/image->texture_data->level_offsets[lvl].offset, image_size);

			// Skip mipPadding
			file.setReadIndex(Maths::roundUpToMultipleOfPowerOf2(file.getReadIndex(), (size_t)4));
		}

		return image;
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


// See http://github.khronos.org/KTX-Specification/
Reference<Map2D> KTXDecoder::decodeKTX2(const std::string& path, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(path);
		return decodeKTX2FromBuffer(file.fileData(), file.fileSize(), mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> KTXDecoder::decodeKTX2FromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator)
{
	try
	{
		BufferViewInStream file(ArrayRef<uint8>((const uint8*)data, size));

		uint8 identifier[12];
		file.readData(identifier, 12);
		if(memcmp(identifier, ktx2_file_id, 12) != 0)
			throw glare::Exception("Invalid file id");

		const uint32 vkFormat					= file.readUInt32();
		/*const uint32 glTypeSize  =*/			  file.readUInt32();
		const uint32 pixelWidth					= file.readUInt32(); // The size of the texture image for level 0, in pixels.
		const uint32 pixelHeight				= file.readUInt32();
		const uint32 pixelDepth					= file.readUInt32(); // For 2D and cubemap textures, pixelDepth must be 0.
		const uint32 layerCount				= file.readUInt32(); // layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.
		const uint32 faceCount					= file.readUInt32(); // faceCount specifies the number of cubemap faces
		const uint32 levelCount					= file.readUInt32(); // levelCount specifies the number of levels in the Mip Level Array
		const uint32 supercompressionScheme		= file.readUInt32();

		if(pixelDepth > 1)
			throw glare::Exception("pixelDepth > 1 not supported.");

		if(faceCount != 1)
			throw glare::Exception("faceCount != 1 not supported.");

		if(!(supercompressionScheme == 0 || supercompressionScheme == 2))
			throw glare::Exception("Only supercompressionScheme 0 and 2 supported (none and zstd).");

		if(layerCount > 100000) // Fail on excessively large files.
			throw glare::Exception("Invalid layerCount: " + toString(layerCount));

		if(pixelWidth > 1000000)
			throw ImFormatExcep("Invalid width: " + toString(pixelWidth));
		if(pixelHeight > 1000000)
			throw ImFormatExcep("Invalid height: " + toString(pixelHeight));

		const size_t max_num_pixels = 1 << 27;
		if(((size_t)pixelWidth * (size_t)pixelHeight) > max_num_pixels)
			throw ImFormatExcep("invalid width, height, num_images: (too many pixels): " + toString(pixelWidth) + ", " + toString(pixelHeight));

		/*const uint32 dfdByteOffset =*/ file.readUInt32();
		/*const uint32 dfdByteLength =*/ file.readUInt32();
		const uint32 kvdByteOffset = file.readUInt32();
		const uint32 kvdByteLength = file.readUInt32();
		/*const uint64 sgdByteOffset =*/ file.readUInt64();
		/*const uint64 sgdByteLength =*/ file.readUInt64();

		// Read level index
		struct LevelData
		{
			uint64 byteOffset; // The offset from the start of the file of the first byte of image data for mip level p.
			uint64 byteLength; // The total size of the data for supercompressed mip level p.
			uint64 uncompressedByteLength; // levels[p].uncompressedByteLength is the number of bytes of pixel data in LOD levelp after reflation from supercompression.
		};

		const size_t use_num_mipmap_levels = myMax<size_t>(1, levelCount);
		if(use_num_mipmap_levels > 32)
			throw glare::Exception("Too many mipmap levels.");
		std::vector<LevelData> level_data(use_num_mipmap_levels);

		file.readData(level_data.data(), level_data.size() * sizeof(LevelData));

		// Read the key/value data, looking for a KTXanimData entry (https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_ktxanimdata)
		bool have_anim_data = false;
		uint32 anim_duration = 0, anim_timescale = 0;
		if(kvdByteLength > 0)
		{
			file.setReadIndex(kvdByteOffset);
			if(!file.canReadNBytes(kvdByteLength))
				throw glare::Exception("Invalid kvdByteLength.");
			const size_t kvd_end = (size_t)kvdByteOffset + kvdByteLength;

			while(file.getReadIndex() + sizeof(uint32) <= kvd_end)
			{
				const uint32 key_and_value_byte_length = file.readUInt32();
				if(key_and_value_byte_length > kvd_end - file.getReadIndex())
					throw glare::Exception("Invalid keyAndValueByteLength.");

				const char* key_and_value = (const char*)file.currentReadPtr();
				const char anim_key[] = "KTXanimData"; // sizeof() includes the terminating null, which is part of the key.
				if((key_and_value_byte_length == sizeof(anim_key) + 3 * sizeof(uint32)) && (std::memcmp(key_and_value, anim_key, sizeof(anim_key)) == 0))
				{
					uint32 anim_data[3]; // duration, timescale, loopcount
					std::memcpy(anim_data, key_and_value + sizeof(anim_key), sizeof(anim_data));
					anim_duration  = anim_data[0];
					anim_timescale = anim_data[1];
					have_anim_data = true;
				}

				file.advanceReadIndex(Maths::roundUpToMultipleOfPowerOf2<size_t>(key_and_value_byte_length, 4)); // Skip the key and value, and valuePadding.
			}
		}

		// An array texture with a KTXanimData entry is an animated texture, with one layer per frame.  Other array textures are not supported.
		if((layerCount > 0) && !have_anim_data)
			throw glare::Exception("layerCount > 0 not supported, except for animated textures.");
		const size_t num_frames = (layerCount > 0) ? layerCount : 1;
		if(num_frames > 1)
		{
			if(anim_duration == 0 || anim_timescale == 0)
				throw glare::Exception("Invalid KTXanimData.");
		}

		OpenGLTextureFormat format;
		if(vkFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK)
		{
			format = OpenGLTextureFormat::Format_Compressed_BC6H;
		}
		else if(vkFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK) // Aka DXT1 (DXT without alpha)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8;
		}
		else if(vkFormat == VK_FORMAT_BC3_UNORM_BLOCK) // Aka DXT5 (DXT with alpha)
		{
			format = OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8;
		}
		else
			throw glare::Exception("Unhandled vkFormat " + toString(vkFormat) + ".");

		CompressedImageRef image = new CompressedImage(pixelWidth, pixelHeight, format);
		image->setAllocator(mem_allocator);

		// Build level_offsets.  These are the offsets within a single frame: for a multi-frame (animated) texture, mipmap_data holds all MIP levels of frame 0, 
		// then all MIP levels of frame 1, etc. (see TextureData.h), while the KTX2 file holds all frames of MIP level 0, then all frames of MIP level 1, etc.
		size_t offset = 0;
		for(size_t i=0; i<use_num_mipmap_levels; ++i)
		{
			const size_t expected_blocks = TextureData::computeNum4PixelBlocksForLevel(pixelWidth, pixelHeight, i);
			const size_t frame_level_size = expected_blocks * bytesPerBlock(image->texture_data->format); // Size of the level for a single frame.  pixelWidth * pixelHeight is bounded above, so this doesn't overflow.
			if(level_data[i].uncompressedByteLength != (uint64)frame_level_size * (uint64)num_frames) // Use 64-bit maths, as size_t may be 32-bit (e.g. in Emscripten)
				throw glare::Exception("Unexpected mip image size.");

			TextureData::LevelOffsetData tex_level_data;
			tex_level_data.offset = offset; // Compute offset we will store at
			tex_level_data.level_size = frame_level_size;
			image->texture_data->level_offsets.push_back(tex_level_data);

			offset += frame_level_size;
		}

		const size_t frame_size = offset;
		if((uint64)frame_size * (uint64)num_frames > 1000000000) // Fail on excessively large files.  Use 64-bit maths, as size_t may be 32-bit.
			throw glare::Exception("Total size is too large.");
		const size_t total_size = frame_size * num_frames;

		image->texture_data->mipmap_data.resizeNoCopy(total_size);
		image->texture_data->frame_size_B = frame_size;
		MutableArrayRef<uint8> data_ref(image->texture_data->mipmap_data.data(), image->texture_data->mipmap_data.size());

		if(num_frames > 1)
		{
			const double frame_time_s = (double)anim_duration / (double)anim_timescale;
			image->texture_data->num_frames = num_frames;
			image->texture_data->frame_durations_equal = true;
			image->texture_data->recip_frame_duration = 1.0 / frame_time_s;
			image->texture_data->last_frame_end_time = frame_time_s * num_frames;
		}

		std::vector<uint8> multi_frame_level_buf; // For multi-frame textures, the data for all frames of the current MIP level, before it is copied to the frame-major layout.

		// Read mip levels in reverse order, since lowest level mipmap (smallest) should be first in file.
		for(int lvl = (int)use_num_mipmap_levels - 1; lvl >= 0; --lvl)
		{
			file.setReadIndex(level_data[lvl].byteOffset); // TODO: check this has a reasonable value - past header etc.

			const size_t level_size = level_data[lvl].uncompressedByteLength; // Size for all frames.  Bounded by total_size check above.

			// For a single frame, read directly into mipmap_data.  Otherwise read into multi_frame_level_buf.
			if(num_frames > 1)
				multi_frame_level_buf.resize(level_size);
			MutableArrayRef<uint8> level_dest = (num_frames == 1) ? data_ref.getSliceChecked(image->texture_data->level_offsets[lvl].offset, level_size) : MutableArrayRef<uint8>(multi_frame_level_buf);

			if(supercompressionScheme == 0) // If no compression:
			{
				file.readDataChecked(/*dest buf=*/level_dest, /*dest offset=*/0, level_size);
			}
			else if(supercompressionScheme == 2) // ZSTD compression:
			{
				if(!file.canReadNBytes(level_data[lvl].byteLength)) // Check compressed_size is valid, while taking care with wraparound
					throw glare::Exception("Compressed size is too large.");

				const uint64 decompressed_size = ZSTD_getFrameContentSize(file.currentReadPtr(), level_data[lvl].byteLength);
				if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
					throw glare::Exception("Failed to get decompressed_size");

				if(decompressed_size != level_size)
					throw glare::Exception("decompressed_size did not match uncompressedByteLength");

				const size_t res = ZSTD_decompress(/*dest=*/level_dest.data(), /*dest capacity=*/level_dest.size(), /*src=*/file.currentReadPtr(), /*compressed size=*/level_data[lvl].byteLength);
				if(ZSTD_isError(res))
					throw glare::Exception("Decompression of buffer failed: " + toString(res));
				if(res < decompressed_size)
					throw glare::Exception("Decompression of buffer failed: not enough bytes in result");
			}
			else
			{
				// supercompressionScheme is checked above also.
				throw glare::Exception("Unhandled supercompression scheme.");
			}

			// Copy each frame's data for this level to its place in mipmap_data.
			if(num_frames > 1)
			{
				const size_t frame_level_size = image->texture_data->level_offsets[lvl].level_size;
				for(size_t f=0; f<num_frames; ++f)
					std::memcpy(image->texture_data->mipmap_data.data() + f * frame_size + image->texture_data->level_offsets[lvl].offset, multi_frame_level_buf.data() + f * frame_level_size, frame_level_size);
			}
		}

		return image;
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


// Only handles VK_FORMAT_BC6H_UFLOAT_BLOCK format currently.
void KTXDecoder::supercompressKTX2File(const std::string& path_in, const std::string& path_out)
{
	throw glare::Exception("KTXDecoder::supercompressKTX2File disabled");
#if 0
	CompressedImageRef image;
	if(hasExtension(path_in, "ktx"))
		image = KTXDecoder::decode(path_in).downcast<CompressedImage>();
	else
		image = KTXDecoder::decodeKTX2(path_in).downcast<CompressedImage>();

	FileOutStream file(path_out);
	file.writeData(ktx2_file_id, 12);

	file.writeUInt32(VK_FORMAT_BC6H_UFLOAT_BLOCK); // vkFormat
	file.writeUInt32(1); // glTypeSize: For formats whose Vulkan names have the suffix _BLOCK it must equal 1
	file.writeUInt32((uint32)image->getMapWidth()); // pixelWidth: The size of the texture image for level 0, in pixels.
	file.writeUInt32((uint32)image->getMapHeight()); // pixelHeight
	file.writeUInt32(0); // pixelDepth: For 2D and cubemap textures, pixelDepth must be 0.
	file.writeUInt32(0); // layerCount: layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.
	file.writeUInt32(1); // faceCount specifies the number of cubemap faces: For non cubemaps this must be 1
	file.writeUInt32((uint32)image->/*mipmap_level_data*/mip_level_info.size()); // levelCount: levelCount specifies the number of levels in the Mip Level Array
	file.writeUInt32(2); // supercompressionScheme: Use 2 = zstd

	file.writeUInt32(0); // dfdByteOffset
	file.writeUInt32(0); // dfdByteLength
	file.writeUInt32(0); // kvdByteOffset
	file.writeUInt32(0); // kvdByteLength
	file.writeUInt64(0); // sgdByteOffset
	file.writeUInt64(0); // sgdByteLength

	// 80 bytes to here

	//  level index
	struct LevelData
	{
		uint64 byteOffset; // The offset from the start of the file of the first byte of image data for mip level p.
		uint64 byteLength; // The total size of the data for supercompressed mip level p.
		uint64 uncompressedByteLength; // levels[p].uncompressedByteLength is the number of bytes of pixel data in LOD levelp after reflation from supercompression.
	};

	std::vector<LevelData> level_data(image->/*mipmap_level_data*/mip_level_info.size());

	const size_t mip_level_byte_start = file.getWriteIndex() + level_data.size() * sizeof(LevelData);
	size_t level_byte_write_i = mip_level_byte_start;

	js::Vector<uint8, 16> compressed_data;

	for(int i=(int)image->/*mipmap_level_data*/mip_level_info.size() - 1; i>=0; --i)
	{
		const glare::AllocatorVector<uint8, 16>& level_i_data = image->mipmap_level_data[i];

		// Compress the buffer with zstandard
		const size_t compressed_bound = ZSTD_compressBound(level_i_data.size());

		const size_t compressed_data_write_i = compressed_data.size();
		compressed_data.resize(compressed_data.size() + compressed_bound); // Resize to be large enough to hold compressed_bound additional bytes.
		
		const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data() + compressed_data_write_i, /*dst capacity=*/compressed_bound, level_i_data.data(), level_i_data.size(),
			ZSTD_CLEVEL_DEFAULT // compression level
		);
		if(ZSTD_isError(compressed_size))
			throw glare::Exception("Compression failed: " + toString(compressed_size));

		// Trim compressed_data
		compressed_data.resize(compressed_data_write_i + compressed_size);

		level_data[i].byteOffset = level_byte_write_i;
		level_data[i].byteLength = compressed_size;
		level_data[i].uncompressedByteLength = level_i_data.size();

		level_byte_write_i += compressed_size;
	}

	// Write level index
	file.writeData(level_data.data(), level_data.size() * sizeof(LevelData));

	// Write mipmap level data
	file.writeData(compressed_data.data(), compressed_data.size());
#endif
}


void KTXDecoder::writeKTX2File(Format format, bool supercompression, int w, int h, int num_frames, double frame_duration_s, const std::vector<std::vector<uint8> >& level_image_data,
	const std::string& path_out, int zstd_compression_level)
{
	FileOutStream file(path_out);
	writeKTX2ToStream(format, supercompression, w, h, num_frames, frame_duration_s, level_image_data, file, zstd_compression_level);
}


void KTXDecoder::writeKTX2ToStream(Format format, bool supercompression, int w, int h, int num_frames, double frame_duration_s, const std::vector<std::vector<uint8> >& level_image_data,
	OutStream& file, int zstd_compression_level)
{
	if(num_frames < 1)
		throw glare::Exception("Invalid num_frames: " + toString(num_frames));
	for(size_t i=0; i<level_image_data.size(); ++i)
		if(level_image_data[i].size() % num_frames != 0)
			throw glare::Exception("Level data size is not a multiple of num_frames.");

	// Build the key/value data.  For an animated texture, this is the KTXanimData entry (https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_ktxanimdata)
	js::Vector<uint8, 16> kvd;
	if(num_frames > 1)
	{
		// Use the finest timescale (time units per second) for which the duration fits in the uint32 duration field: microseconds for durations up to ~4295 s,
		// otherwise milliseconds or seconds.
		const uint32 timescales[] = { 1000000, 1000, 1 };
		uint32 timescale = 0;
		double duration = 0;
		for(size_t i=0; i<staticArrayNumElems(timescales); ++i)
		{
			duration = myMax(1.0, std::round(frame_duration_s * timescales[i]));
			if(duration <= (double)std::numeric_limits<uint32>::max())
			{
				timescale = timescales[i];
				break;
			}
		}
		if(!(frame_duration_s > 0) || (timescale == 0)) // Also catches NaN and infinite durations.
			throw glare::Exception("Invalid frame duration: " + toString(frame_duration_s));

		const char key[] = "KTXanimData"; // sizeof() includes the terminating null, which is part of the key.
		const uint32 anim_data[3] = { /*duration=*/(uint32)duration, timescale, /*loopcount=*/0 }; // loopcount 0 = loop forever.
		const uint32 key_and_value_byte_length = (uint32)(sizeof(key) + sizeof(anim_data));

		kvd.resize(sizeof(uint32) + key_and_value_byte_length);
		std::memcpy(kvd.data(), &key_and_value_byte_length, sizeof(uint32));
		std::memcpy(kvd.data() + sizeof(uint32), key, sizeof(key));
		std::memcpy(kvd.data() + sizeof(uint32) + sizeof(key), anim_data, sizeof(anim_data));
		while(kvd.size() % 4 != 0) // valuePadding: pad to a multiple of 4 bytes.
			kvd.push_back(0);
	}

	file.writeData(ktx2_file_id, 12);

	uint32 vk_format;
	size_t bytes_per_block;
	switch(format)
	{
		// case Format_SRGB_Uint8: vk_format = VK_FORMAT_R8G8B8_SRGB;         break;
		case Format_BC1:        vk_format = VK_FORMAT_BC1_RGB_UNORM_BLOCK; bytes_per_block = 8;  break;
		case Format_BC3:        vk_format = VK_FORMAT_BC3_UNORM_BLOCK;     bytes_per_block = 16; break;
		case Format_BC6H:       vk_format = VK_FORMAT_BC6H_UFLOAT_BLOCK;   bytes_per_block = 16; break;
		default: throw glare::Exception("Invalid format");
	}


	file.writeUInt32(vk_format); // vkFormat
	file.writeUInt32(1); // typeSize: For formats whose Vulkan names have the suffix _BLOCK it must equal 1.  Also is 1 for any 8 bit format. (https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_typesize)
	file.writeUInt32((uint32)w); // pixelWidth: The size of the texture image for level 0, in pixels.
	file.writeUInt32((uint32)h); // pixelHeight
	file.writeUInt32(0); // pixelDepth: For 2D and cubemap textures, pixelDepth must be 0.
	file.writeUInt32((num_frames > 1) ? (uint32)num_frames : 0); // layerCount: layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.  An animated texture has one layer per frame.
	file.writeUInt32(1); // faceCount specifies the number of cubemap faces: For non cubemaps this must be 1
	file.writeUInt32((uint32)level_image_data.size()); // levelCount: levelCount specifies the number of levels in the Mip Level Array
	file.writeUInt32(supercompression ? 2 : 0); // supercompressionScheme (2 = zstd)

	struct LevelData
	{
		uint64 byteOffset; // The offset from the start of the file of the first byte of image data for mip level p.
		uint64 byteLength; // The total size of the data for supercompressed mip level p.
		uint64 uncompressedByteLength; // levels[p].uncompressedByteLength is the number of bytes of pixel data in LOD levelp after reflation from supercompression.
	};

	// The key/value data goes directly after the level index.
	const size_t kvd_byte_offset = 80 + level_image_data.size() * sizeof(LevelData);

	file.writeUInt32(0); // dfdByteOffset
	file.writeUInt32(0); // dfdByteLength
	file.writeUInt32(kvd.empty() ? 0 : (uint32)kvd_byte_offset); // kvdByteOffset
	file.writeUInt32((uint32)kvd.size()); // kvdByteLength
	file.writeUInt64(0); // sgdByteOffset
	file.writeUInt64(0); // sgdByteLength

	// 80 bytes to here

	//  level index
	std::vector<LevelData> level_data(level_image_data.size());

	// Without supercompression, the start of each mip level must be aligned to lcm(texel block size, 4) bytes, which is the block size for the formats we write.
	// With supercompression, no alignment is required.  See https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_mippadding
	const size_t mip_level_alignment = supercompression ? 1 : bytes_per_block;
	const size_t mip_level_byte_start = Maths::roundUpToMultipleOfPowerOf2(kvd_byte_offset + kvd.size(), mip_level_alignment);
	size_t level_byte_write_i = mip_level_byte_start;

	js::Vector<uint8, 16> compressed_data;

	for(int i=(int)level_image_data.size() - 1; i>=0; --i) // "Mip levels in the array are ordered from the level with the smallest size images, levelp to that with the largest size images, levelbase"
	{
		const std::vector<uint8>& level_i_data = level_image_data[i];

		if(supercompression)
		{
			// Compress the buffer with zstandard
			const size_t compressed_bound = ZSTD_compressBound(level_i_data.size());

			const size_t compressed_data_write_i = compressed_data.size();
			compressed_data.resize(compressed_data.size() + compressed_bound); // Resize to be large enough to hold compressed_bound additional bytes.
		
			const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data() + compressed_data_write_i, /*dst capacity=*/compressed_bound, level_i_data.data(), level_i_data.size(),
				zstd_compression_level // compression level
			);
			if(ZSTD_isError(compressed_size))
				throw glare::Exception("Compression failed: " + toString(compressed_size));

			// Trim compressed_data
			compressed_data.resize(compressed_data_write_i + compressed_size);

			level_data[i].byteOffset = level_byte_write_i;
			level_data[i].byteLength = compressed_size;
			level_data[i].uncompressedByteLength = level_i_data.size();

			level_byte_write_i += compressed_size;
		}
		else
		{
			const size_t compressed_data_write_i = compressed_data.size();
			compressed_data.resize(compressed_data.size() + level_i_data.size());
			std::memcpy(&compressed_data[compressed_data_write_i], level_i_data.data(), level_i_data.size());

			level_data[i].byteOffset = level_byte_write_i;
			level_data[i].byteLength = level_i_data.size();
			level_data[i].uncompressedByteLength = level_i_data.size();

			level_byte_write_i += level_i_data.size();
		}

		// Without supercompression, each level's size is a multiple of the block size, so the following levels stay aligned without padding between them.
	}

	// Write level index
	file.writeData(level_data.data(), level_data.size() * sizeof(LevelData));

	// Write key/value data (at kvd_byte_offset)
	file.writeData(kvd.data(), kvd.size());

	// Write padding to align the mip level data
	const uint8 zero_padding[16] = { 0 };
	const size_t padding_size = mip_level_byte_start - (kvd_byte_offset + kvd.size());
	assert(padding_size < 16);
	file.writeData(zero_padding, padding_size);

	// Write mipmap level data (at mip_level_byte_start)
	file.writeData(compressed_data.data(), compressed_data.size());
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/TaskManager.h"
#if !IS_INDIGO
#include "TextRenderer.h"
#endif
#include "DXTCompression.h"
//#include <encoder/basisu_comp.h>


#if 0
// Command line:
// C:\fuzz_corpus\ktx c:/code/glare-core/testfiles\ktx

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		KTXDecoder::decodeFromBuffer(data, size);
	}
	catch(glare::Exception&)
	{
	}

	try
	{
		KTXDecoder::decodeKTX2FromBuffer(data, size);
	}
	catch(glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


#define WRITE_FUZZ_SEEDS 0
#if WRITE_FUZZ_SEEDS


// Write out a set of valid animated KTX2 files (array textures with a KTXanimData key/value entry, as written by writeKTX2File() with num_frames > 1),
// for use as a libFuzzer seed corpus, in addition to the existing non-animated files in testfiles/ktx.
// The seeds cover: BC1 and BC3, with and without zstd supercompression, with and without MIP levels, non-square and non-multiple-of-4 dimensions,
// 2 frames up to many frames, and frames with identical and differing data.
static void writeAnimatedFuzzSeed(const std::string& dir, const std::string& name, KTXDecoder::Format format, bool supercompression, int w, int h, int num_frames, bool mipmaps,
	double frame_duration_s, bool identical_frames)
{
	const size_t bytes_per_block = (format == KTXDecoder::Format_BC1) ? 8 : 16;
	const size_t num_levels = mipmaps ? TextureData::computeNumMipLevels(w, h) : 1;

	// Use a simple deterministic pseudo-random sequence for the block data, so the zstd-compressed data has some structure, but isn't trivial.
	uint32 state = 1;
	auto nextByte = [&]() { state = state * 1664525u + 1013904223u; return (uint8)(state >> 24); };

	std::vector<std::vector<uint8> > level_image_data(num_levels);
	for(size_t k=0; k<num_levels; ++k)
	{
		const size_t frame_level_size = TextureData::computeNum4PixelBlocksForLevel(w, h, k) * bytes_per_block;
		std::vector<uint8> frame_0_data(frame_level_size);
		for(size_t i=0; i<frame_level_size; ++i)
			frame_0_data[i] = nextByte();

		for(int f=0; f<num_frames; ++f)
		{
			if(identical_frames || (f == 0))
				level_image_data[k].insert(level_image_data[k].end(), frame_0_data.begin(), frame_0_data.end());
			else
			{
				// Mostly the same as frame 0, with some changed bytes, like a typical animation.
				for(size_t i=0; i<frame_level_size; ++i)
					level_image_data[k].push_back(((i + f) % 7 == 0) ? nextByte() : frame_0_data[i]);
			}
		}
	}

	const std::string path = dir + "/" + name + ".ktx2";
	KTXDecoder::writeKTX2File(format, supercompression, w, h, num_frames, frame_duration_s, level_image_data, path, /*zstd_compression_level=*/3);

	// Check the seed decodes as expected.
	Reference<Map2D> im = KTXDecoder::decodeKTX2(path);
	testAssert(im->getMapWidth() == (size_t)w && im->getMapHeight() == (size_t)h);
	testAssert(im.isType<CompressedImage>());
	const TextureData* texture_data = im.downcastToPtr<CompressedImage>()->texture_data.ptr();
	testAssert(texture_data->numFrames() == (size_t)num_frames);
	testAssert(texture_data->numMipLevels() == num_levels);
}


static void writeFuzzSeeds(const std::string& dir)
{
	//                                    name                         format                    zstd    w    h    frames mipmaps frame duration identical frames
	writeAnimatedFuzzSeed(dir, "anim_bc1_zstd_mips",          KTXDecoder::Format_BC1, true,   16,  16,  3,     true,   0.1,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_raw_mips",           KTXDecoder::Format_BC1, false,  16,  16,  3,     true,   0.1,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc3_zstd_mips",          KTXDecoder::Format_BC3, true,   16,  16,  3,     true,   0.04,          false);
	writeAnimatedFuzzSeed(dir, "anim_bc3_raw_no_mips",        KTXDecoder::Format_BC3, false,  8,   8,   2,     false,  1.0,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_zstd_no_mips",       KTXDecoder::Format_BC1, true,   4,   4,   2,     false,  0.5,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_zstd_non_square",    KTXDecoder::Format_BC1, true,   24,  8,   4,     true,   0.1,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_raw_non_mult_4",     KTXDecoder::Format_BC1, false,  10,  6,   2,     true,   0.1,           false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_zstd_many_frames",   KTXDecoder::Format_BC1, true,   8,   8,   64,    true,   0.01,          false);
	writeAnimatedFuzzSeed(dir, "anim_bc1_zstd_identical",     KTXDecoder::Format_BC1, true,   32,  32,  8,     true,   0.1,           true);
	writeAnimatedFuzzSeed(dir, "anim_bc3_zstd_long_duration", KTXDecoder::Format_BC3, true,   8,   8,   2,     true,   3600.0,        false);

	conPrint("Wrote fuzz seeds to '" + dir + "'.");
}


#endif // WRITE_FUZZ_SEEDS


static void makeMipMapTestTexture()
{
#if !SERVER && !IS_INDIGO
	glare::TaskManager task_manager;

	TextRendererRef text_renderer = new TextRenderer();

	const int font_size_px = 14;
	TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, 
		TestUtils::getTestReposDir() + "/testfiles/fonts/TruenoLight-E2pg.otf", font_size_px);

	const int small_font_size_px = 8;
	TextRendererFontFaceRef small_font = new TextRendererFontFace(text_renderer, 
		TestUtils::getTestReposDir() + "/testfiles/fonts/TruenoLight-E2pg.otf", small_font_size_px);


	// A 'categorical' colour palette
	const float v = 0.3f;
	Colour3f cols[] = { 
		Colour3f(1, v, v),
		Colour3f(v, 1, v),
		Colour3f(v, 0.5f, 1),

		Colour3f(1, 1, 0),
		Colour3f(0, 1, 1),
		Colour3f(1, 0, 1),

		Colour3f(1, 0.6f, 0),
		Colour3f(0, 0.6f, 1),
		Colour3f(0.6f, 0, 1),

		Colour3f(0.4f, 0.2f, 0.7f),
		Colour3f(0.5, 0.5, 0.5),

		Colour3f(1.f),
		Colour3f(1.f),
		Colour3f(1.f),
		Colour3f(1.f),
		Colour3f(1.f),
		Colour3f(1.f)
	};

	const int W = 1024; // Texture width

	std::vector<std::vector<uint8>> level_image_data;
	std::vector<ImageMapUInt8Ref> level_images;
	int level_W = W;
	int level = 0;
	while(level_W != 0)
	{
		ImageMapUInt8Ref im = new ImageMapUInt8(level_W, level_W, 3);
		level_images.push_back(im);

		const Colour3f level_col = cols[level];

		for(size_t y=0; y<level_W; ++y)
		for(size_t x=0; x<level_W; ++x)
		{
			Colour3f background_col;
			background_col = level_col * 0.9f;

			im->getPixel(x, y)[0] = (uint8)(background_col.r * 255.01f);
			im->getPixel(x, y)[1] = (uint8)(background_col.g * 255.01f);
			im->getPixel(x, y)[2] = (uint8)(background_col.b * 255.01f);
		}

		if(level_W >= 8)
		{
			if(level <= 5)
			{
				const size_t line_w = (level_W < 4) ? 4 : 2;

				// Draw horizontal lines
				const size_t gap_w = (level <= 3) ? (level_W / 4) : level_W;
				for(size_t starty=0; starty<level_W; starty += gap_w)
				{
					for(size_t y=starty; y<starty + line_w; ++y)
					{
						Colour3f line_col = (starty == 0) ? (level_col * 0.5f) : (level_col * 0.82f);
						for(size_t x=0; x<level_W; ++x)
						{
							im->getPixel(x, y)[0] = (uint8)(line_col.r * 255.01f);
							im->getPixel(x, y)[1] = (uint8)(line_col.g * 255.01f);
							im->getPixel(x, y)[2] = (uint8)(line_col.b * 255.01f);
						}
					}
				}
				// Draw vertical lines
				for(size_t startx=0; startx<level_W; startx += gap_w)
				{
					for(size_t x=startx; x<startx + line_w; ++x)
					{
						Colour3f line_col = (startx == 0) ? (level_col * 0.5f) : (level_col * 0.75f);
						for(size_t y=0; y<level_W; ++y)
						{
							im->getPixel(x, y)[0] = (uint8)(line_col.r * 255.01f);
							im->getPixel(x, y)[1] = (uint8)(line_col.g * 255.01f);
							im->getPixel(x, y)[2] = (uint8)(line_col.b * 255.01f);
						}
					}
				}
			}

			// Draw mip level as repeating text
			Colour3f font_col = level_col * 0.6f;
			if(level < 4)
			{
				int cell_w_px = level_W / 4;
				for(size_t y=0; y<=level_W; y += cell_w_px)
				for(size_t x=0; x<=level_W; x += cell_w_px)
				{
					text_renderer->drawText(*im, toString(level), (int)x + cell_w_px/2 - 4, (int)y - cell_w_px/2 + font_size_px/2 + /*line w/2=*/2, font_col, false, font.ptr(), /*emoji_font=*/nullptr);
				}
			}
			else if(level < 8)
			{
				text_renderer->drawText(*im, toString(level), level_W/2 - 4, level_W/2 + small_font_size_px/2, font_col, false, font.ptr(), /*emoji_font=*/nullptr);
			}
				
			// Draw (x, y) coords for lower MIP levels.
			if(level < 2)
			{
				Colour3f xy_font_col = level_col * 0.3f;
				for(size_t y=0; y<=level_W; y += level_W / 2)
				for(size_t x=0; x<=level_W; x += level_W / 2)
				{
					float tex_x = (float)x / level_W;
					float tex_y = 1.f - (float)y / level_W;
						
					const int text_padding_px = 5;
					text_renderer->drawText(*im, "x=" + doubleToStringMaxNDecimalPlaces(tex_x, 2) + ", y=" + doubleToStringMaxNDecimalPlaces(tex_y, 2), (int)x + text_padding_px, (int)y - text_padding_px, xy_font_col, false, font.ptr(), /*emoji_font=*/nullptr);
				}
			}
		}

		const size_t compressed_size_B = DXTCompression::getCompressedSizeBytes(level_W, level_W, 3);

		// Compress data
		std::vector<uint8> level_data(compressed_size_B);
		DXTCompression::TempData temp_data;
		DXTCompression::compress(&task_manager, temp_data, level_W, level_W, 3,
			im->getData(), level_data.data(), level_data.size());

		level_image_data.push_back(level_data);

		level_W /= 2;
		level++;
	}

	KTXDecoder::writeKTX2File(KTXDecoder::Format::Format_BC1, /*supercompression=*/false, (int)W, (int)W, /*num_frames=*/1, /*frame_duration_s=*/0.0, level_image_data, "d:/tempfiles/miptest.ktx2", /*zstd_compression_level=*/3);


	// Save to basis file as well, as an array texture.
#if 0
	{
		basisu::basisu_encoder_init(); // Can be called multiple times harmlessly.
		basisu::basis_compressor_params params;

		params.m_source_mipmap_images.resize(1);

		for(size_t i=0; i<level_images.size(); ++i)
		{
			const ImageMapUInt8Ref level_im = level_images[i];
			
			basisu::image img(level_im->getData(), (uint32)level_im->getWidth(), (uint32)level_im->getHeight(), (uint32)3);

			if(i == 0)
				params.m_source_images.push_back(img);
			else
				params.m_source_mipmap_images[0].push_back(img);
		}


		params.m_tex_type = basist::cBASISTexType2DArray;
		
		params.m_perceptual = true;
	
		params.m_write_output_basis_files = true;
		params.m_out_filename = "d:/tempfiles/miptest_array_texture.basis";
		params.m_create_ktx2_file = false;

		params.m_mip_gen = false; // Generate mipmaps for each source image
		params.m_mip_srgb = false; // Convert image to linear before filtering, then back to sRGB

		params.m_quality_level = 255;

		// Need to be set if m_quality_level is not explicitly set.
		//params.m_max_endpoint_clusters = 16128;
		//params.m_max_selector_clusters = 16128;

		basisu::job_pool jpool(PlatformUtils::getNumLogicalProcessors());
		params.m_pJob_pool = &jpool;

		basisu::basis_compressor basisCompressor;
		basisu::enable_debug_printf(false);

		const bool res = basisCompressor.init(params);
		if(!res)
			throw glare::Exception("Failed to create basisCompressor");

		basisu::basis_compressor::error_code result = basisCompressor.process();

		if(result != basisu::basis_compressor::cECSuccess)
			throw glare::Exception("basisCompressor.process() failed.");
	}
#endif

#endif
}


void KTXDecoder::test()
{
	conPrint("KTXDecoder::test()");

	try
	{
#if WRITE_FUZZ_SEEDS
		writeFuzzSeeds(TestUtils::getTestReposDir() + "/testfiles/ktx");
#endif

		if(false)
			makeMipMapTestTexture();

		//{
		//	//----------------------------------- Test loading a KTX array texture -------------------------------------------
		//	Reference<Map2D> im = KTXDecoder::decodeKTX2("d:/tempfiles/chunk_basis_array_texture.ktx2");
		//	testAssert(im->getMapWidth() == 128);
		//	testAssert(im->getMapHeight() == 128);
		//	testAssert(im.isType<CompressedImage>());
		//	testAssert(im.downcastToPtr<CompressedImage>()->texture_data->level_offsets.size() == 8);
		//}
		{
			//----------------------------------- Test loading KTX files -------------------------------------------
			Reference<Map2D> im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/save.01.ktx2");
			testAssert(im->getMapWidth() == 512);
			testAssert(im->getMapHeight() == 512);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8);
			testAssert(com_im->texture_data->level_offsets.size() == 10);
			testAssert(com_im->texture_data->D == 1);
			testAssert(com_im->texture_data->num_array_images == 0);
		}
#if 1
		//----------------------------------- Test KTX files in ktxtest-master  -------------------------------------------
		{
			const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir() + "/testfiles/ktx/ktxtest-master", "ktx");
			for(size_t i=0; i<paths.size(); ++i)
			{
				try
				{
					conPrint("Loading '" + paths[i] + "'...");
					Reference<Map2D> im = KTXDecoder::decode(paths[i]);

					// Make some ktx2 files from these ktx files.
					// KTXDecoder::supercompressKTX2File(paths[i], TestUtils::getTestReposDir() + "/testfiles/ktx/ktx2_from_ktxtest/" + ::removeDotAndExtension(FileUtils::getFilename(paths[i])) + ".ktx2");
				}
				catch(glare::Exception& )
				{

				}
			}
		}


		Reference<Map2D> im;
		
		//----------------------------------- Test loading KTX files -------------------------------------------
		im = KTXDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->D == 1);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 1); // no mipmaps.
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numFrames() == 1);
		testAssert(!im.downcastToPtr<CompressedImage>()->texture_data->isMultiFrame());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->frame_size_B == im.downcastToPtr<CompressedImage>()->texture_data->mipmap_data.size());
		testAssert(!im.downcastToPtr<CompressedImage>()->texture_data->isArrayTexture());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->num_array_images == 0);


		im = KTXDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels

		//----------------------------------- Test loading KTX2 files -------------------------------------------
		im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX2");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 1); // no mipmaps.
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numFrames() == 1);
		testAssert(!im.downcastToPtr<CompressedImage>()->texture_data->isMultiFrame());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->frame_size_B == im.downcastToPtr<CompressedImage>()->texture_data->mipmap_data.size());
		testAssert(!im.downcastToPtr<CompressedImage>()->texture_data->isArrayTexture());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->num_array_images == 0);

		im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels


		//----------------------------------- Test writeKTX2File and decodeKTX2 round trip, with single and multiple (animated) frames -------------------------------------------
		for(int format_i=0; format_i<2; ++format_i)
		for(int use_mipmaps=0; use_mipmaps<2; ++use_mipmaps)
		for(int num_frames=1; num_frames<=3; num_frames += 2)
		for(int supercompression=0; supercompression<2; ++supercompression)
		for(int duration_i=0; duration_i<2; ++duration_i)
		{
			// 8 x 8 BC1 or BC3 texture, with MIP levels 8x8, 4x4, 2x2, 1x1 (which have 4, 1, 1, 1 blocks), or just the 8x8 level.
			// A single level gives an odd number of levels, so the level index ends at a position not aligned to 16 bytes.
			const KTXDecoder::Format format = (format_i == 0) ? KTXDecoder::Format_BC1 : KTXDecoder::Format_BC3;
			const size_t bytes_per_block = (format_i == 0) ? 8 : 16;
			const size_t num_levels = use_mipmaps ? 4 : 1;
			const size_t level_num_blocks[4] = { 4, 1, 1, 1 };
			const double frame_duration_s = (duration_i == 0) ? 0.07 : 10000.0; // 10000 s doesn't fit in the duration field in microseconds.
			auto testByte = [](size_t frame, size_t level, size_t i) { return (uint8)(frame * 97 + level * 31 + i); }; // Different data for each frame and level.

			std::vector<std::vector<uint8> > level_image_data(num_levels);
			for(size_t k=0; k<num_levels; ++k)
				for(int f=0; f<num_frames; ++f)
					for(size_t i=0; i<level_num_blocks[k] * bytes_per_block; ++i)
						level_image_data[k].push_back(testByte(f, k, i));

			const std::string path = PlatformUtils::getTempDirPath() + "/ktx2_round_trip_test.ktx2";
			KTXDecoder::writeKTX2File(format, /*supercompression=*/supercompression != 0, /*w=*/8, /*h=*/8, num_frames, frame_duration_s, level_image_data, path, /*zstd_compression_level=*/3);

			// Without supercompression, check the mip level data is aligned to the block size, as required by the spec.  (The decoder doesn't check this.)
			if(!supercompression)
			{
				std::vector<uint8> file_data;
				FileUtils::readEntireFile(path, file_data);
				for(size_t k=0; k<num_levels; ++k)
				{
					uint64 byte_offset;
					std::memcpy(&byte_offset, &file_data[80 + k * 24], sizeof(uint64)); // Level index starts at byte 80, 24 bytes per level.
					testAssert(byte_offset % bytes_per_block == 0);
				}
			}

			im = KTXDecoder::decodeKTX2(path);
			testAssert(im->getMapWidth() == 8 && im->getMapHeight() == 8);
			testAssert(im.isType<CompressedImage>());
			const TextureData* texture_data = im.downcastToPtr<CompressedImage>()->texture_data.ptr();
			testAssert(texture_data->format == ((format_i == 0) ? OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8 : OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8));
			testAssert(texture_data->numMipLevels() == num_levels);
			testAssert(texture_data->numFrames() == (size_t)num_frames);
			testAssert(texture_data->isMultiFrame() == (num_frames > 1));
			testAssert(!texture_data->isArrayTexture());
			size_t frame_size = 0;
			for(size_t k=0; k<num_levels; ++k)
				frame_size += level_num_blocks[k] * bytes_per_block;
			testAssert(texture_data->frame_size_B == frame_size);
			testAssert(texture_data->mipmap_data.size() == texture_data->frame_size_B * num_frames);
			if(num_frames > 1)
			{
				testAssert(texture_data->frame_durations_equal);
				testEpsEqual(texture_data->recip_frame_duration, 1.0 / frame_duration_s);
				testEpsEqual(texture_data->last_frame_end_time, frame_duration_s * num_frames);
			}

			// Check the data is in the frame-major layout (all levels of frame 0, then all levels of frame 1, etc.)
			for(int f=0; f<num_frames; ++f)
				for(size_t k=0; k<num_levels; ++k)
				{
					testAssert(texture_data->level_offsets[k].level_size == level_num_blocks[k] * bytes_per_block);
					for(size_t i=0; i<level_num_blocks[k] * bytes_per_block; ++i)
						testAssert(texture_data->mipmap_data[f * texture_data->frame_size_B + texture_data->level_offsets[k].offset + i] == testByte(f, k, i));
				}
		}

		//----------------------------------- Test writeKTX2File with invalid frame durations -------------------------------------------
		{
			const double invalid_durations[] = { 0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), 1.0e10 };
			for(size_t i=0; i<staticArrayNumElems(invalid_durations); ++i)
			{
				std::vector<std::vector<uint8> > level_image_data(1, std::vector<uint8>(2 * 8 * 4)); // 2 frames of 8x8 BC1
				try
				{
					KTXDecoder::writeKTX2File(KTXDecoder::Format_BC1, /*supercompression=*/true, /*w=*/8, /*h=*/8, /*num_frames=*/2, invalid_durations[i], level_image_data,
						PlatformUtils::getTempDirPath() + "/ktx2_invalid_duration_test.ktx2", /*zstd_compression_level=*/3);
					failTest("Expected exception for invalid frame duration " + toString(invalid_durations[i]));
				}
				catch(glare::Exception&)
				{}
			}
		}


		//---------------------------------- Test supercompressKTX2File -------------------------------------------
		if(0)
		{
			const std::string src_path  = TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2";
			const std::string dest_path = PlatformUtils::getTempDirPath() + "/lightmap_BC6H_with_mipmaps_supercompressed.KTX2";

			KTXDecoder::supercompressKTX2File(src_path, dest_path);

			// Check suprecompressed file.
			im = KTXDecoder::decodeKTX2(dest_path);
			testAssert(im->getMapWidth() == 512);
			testAssert(im->getMapHeight() == 512);
			testAssert(im.isType<CompressedImage>());
			testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels

			conPrint("raw KTX2 file size:             " + toString(FileUtils::getFileSize(src_path)));
			conPrint("supercompressed KTX2 file size: " + toString(FileUtils::getFileSize(dest_path)));
		}
#endif
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	conPrint("KTXDecoder::test() done.");
}


#endif // BUILD_TESTS
