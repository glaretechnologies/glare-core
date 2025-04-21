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
#include <memory.h>
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

		if(layerCount > 0)
			throw glare::Exception("layerCount > 0 not supported.");

		if(pixelDepth > 1)
			throw glare::Exception("pixelDepth > 1 not supported.");

		if(faceCount != 1)
			throw glare::Exception("faceCount != 1 not supported.");

		if(!(supercompressionScheme == 0 || supercompressionScheme == 2))
			throw glare::Exception("Only supercompressionScheme 0 and 2 supported (none and zstd).");

		if(layerCount > 100) // Fail on excessively large files.
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
		/*const uint32 kvdByteOffset =*/ file.readUInt32();
		/*const uint32 kvdByteLength =*/ file.readUInt32();
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

		// Build level_offsets
		size_t offset = 0;
		for(size_t i=0; i<use_num_mipmap_levels; ++i)
		{
			const uint64 use_level_size = level_data[i].uncompressedByteLength;

			const size_t expected_blocks = TextureData::computeNum4PixelBlocksForLevel(pixelWidth, pixelHeight, i);
			const size_t expected_image_size = expected_blocks * bytesPerBlock(image->texture_data->format);
			if(use_level_size != expected_image_size)
				throw glare::Exception("Unexpected mip image size.");

			TextureData::LevelOffsetData tex_level_data;
			tex_level_data.offset = offset; // Compute offset we will store at
			tex_level_data.level_size = use_level_size;
			image->texture_data->level_offsets.push_back(tex_level_data);

			offset += use_level_size;
		}

		const size_t total_size = offset;

		image->texture_data->mipmap_data.resizeNoCopy(total_size); // TODO: check against some reasonable max size first.
		image->texture_data->frame_size_B = total_size;
		MutableArrayRef<uint8> data_ref(image->texture_data->mipmap_data.data(), image->texture_data->mipmap_data.size());

		// Read mip levels in reverse order, since lowest level mipmap (smallest) should be first in file.
		for(int lvl = (int)use_num_mipmap_levels - 1; lvl >= 0; --lvl)
		{
			file.setReadIndex(level_data[lvl].byteOffset); // TODO: check this has a reasonable value - past header etc.

			if(level_data[lvl].uncompressedByteLength > 100000000) // Fail on excessively large files.
				throw glare::Exception("uncompressedByteLength is too large: " + toString(level_data[lvl].uncompressedByteLength));

			const size_t dest_offset = image->texture_data->level_offsets[lvl].offset;

			if(supercompressionScheme == 0) // If no compression:
			{
				file.readDataChecked(/*dest buf=*/data_ref, /*dest offset=*/dest_offset, level_data[lvl].uncompressedByteLength);
			}
			else if(supercompressionScheme == 2) // ZSTD compression:
			{
				if(!file.canReadNBytes(level_data[lvl].byteLength)) // Check compressed_size is valid, while taking care with wraparound
					throw glare::Exception("Compressed size is too large.");

				const uint64 decompressed_size = ZSTD_getFrameContentSize(file.currentReadPtr(), level_data[lvl].byteLength);
				if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
					throw glare::Exception("Failed to get decompressed_size");

				if(decompressed_size != level_data[lvl].uncompressedByteLength)
					throw glare::Exception("decompressed_size did not match uncompressedByteLength");

				// Decompress data into 'cur_level_data' buffer.
				MutableArrayRef<uint8> dest_array_ref = data_ref.getSliceChecked(dest_offset, decompressed_size);
				const size_t res = ZSTD_decompress(/*dest=*/dest_array_ref.data(), /*dest capacity=*/dest_array_ref.size(), /*src=*/file.currentReadPtr(), /*compressed size=*/level_data[lvl].byteLength);
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


void KTXDecoder::writeKTX2File(Format format, bool supercompression, int w, int h, const std::vector<std::vector<uint8> >& level_image_data, const std::string& path_out)
{
	FileOutStream file(path_out);
	file.writeData(ktx2_file_id, 12);

	uint32 vk_format;
	switch(format)
	{
		// case Format_SRGB_Uint8: vk_format = VK_FORMAT_R8G8B8_SRGB;         break;
		case Format_BC1:        vk_format = VK_FORMAT_BC1_RGB_UNORM_BLOCK; break;
		case Format_BC3:        vk_format = VK_FORMAT_BC3_UNORM_BLOCK;     break;
		case Format_BC6H:       vk_format = VK_FORMAT_BC6H_UFLOAT_BLOCK;   break;
		default: throw glare::Exception("Invalid format");
	}


	file.writeUInt32(vk_format); // vkFormat
	file.writeUInt32(1); // typeSize: For formats whose Vulkan names have the suffix _BLOCK it must equal 1.  Also is 1 for any 8 bit format. (https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_typesize)
	file.writeUInt32((uint32)w); // pixelWidth: The size of the texture image for level 0, in pixels.
	file.writeUInt32((uint32)h); // pixelHeight
	file.writeUInt32(0); // pixelDepth: For 2D and cubemap textures, pixelDepth must be 0.
	file.writeUInt32(0); // layerCount: layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.
	file.writeUInt32(1); // faceCount specifies the number of cubemap faces: For non cubemaps this must be 1
	file.writeUInt32((uint32)level_image_data.size()); // levelCount: levelCount specifies the number of levels in the Mip Level Array
	file.writeUInt32(supercompression ? 2 : 0); // supercompressionScheme (2 = zstd)

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

	std::vector<LevelData> level_data(level_image_data.size());

	const size_t mip_level_byte_start = file.getWriteIndex() + level_data.size() * sizeof(LevelData);
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

		// Because we only handle writing block formats with block sizes that are multiples of 4 bytes, we shouldn't need any padding between mip levels.
		// See https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html#_mippadding
	}

	// Write level index
	file.writeData(level_data.data(), level_data.size() * sizeof(LevelData));

	// Write mipmap level data
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
					font->drawText(*im, toString(level), (int)x + cell_w_px/2 - 4, (int)y - cell_w_px/2 + font_size_px/2 + /*line w/2=*/2, font_col, false);
				}
			}
			else if(level < 8)
			{
				small_font->drawText(*im, toString(level), level_W/2 - 4, level_W/2 + small_font_size_px/2, font_col, false);
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
					font->drawText(*im, "x=" + doubleToStringMaxNDecimalPlaces(tex_x, 2) + ", y=" + doubleToStringMaxNDecimalPlaces(tex_y, 2), (int)x + text_padding_px, (int)y - text_padding_px, xy_font_col, false);
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

	KTXDecoder::writeKTX2File(KTXDecoder::Format::Format_BC1, /*supercompression=*/false, (int)W, (int)W, level_image_data, "d:/tempfiles/miptest.ktx2");


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
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 1); // no mipmaps.

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

		im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->format == OpenGLTextureFormat::Format_Compressed_BC6H);
		testAssert(im.downcastToPtr<CompressedImage>()->texture_data->numMipLevels() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels
		
		

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
