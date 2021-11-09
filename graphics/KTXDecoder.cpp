/*=====================================================================
KTXDecoder.cpp
--------------
Copyright Glare Technologies Limited 2020 -
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
#include <memory.h>
#include <zstd.h>


#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT					0x8E8F
// See https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
#define GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT						0x83F0
#define GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT					0x83F3


// Adapted from https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
// NOTE: untested
static uint32 swapByteOrder(const uint32 x)
{
	return (x >> 24) |
		((x << 8) & 0x00FF0000u) |
		((x >> 8) & 0x0000FF00u) |
		(x << 24);
}


static inline uint32 readUInt32(FileInStream& file, bool swap_endianness)
{
	const uint32 x = file.readUInt32();
	return swap_endianness ? swapByteOrder(x) : x;
}


static uint8 ktx_file_id [12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
static uint8 ktx2_file_id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };


// throws ImFormatExcep on failure
Reference<Map2D> KTXDecoder::decode(const std::string& path)
{
	try
	{
		FileInStream file(path);

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

		const uint32 glType = readUInt32(file, swap_endianness);
		const uint32 glTypeSize = readUInt32(file, swap_endianness);
		const uint32 glFormat = readUInt32(file, swap_endianness);
		const uint32 glInternalFormat = readUInt32(file, swap_endianness);
		const uint32 glBaseInternalFormat = readUInt32(file, swap_endianness);
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

		// Skip key-value data
		file.setReadIndex(file.getReadIndex() + bytesOfKeyValueData);

		const size_t N = 3;
		CompressedImageRef image = new CompressedImage(pixelWidth, pixelHeight, N);

		image->gl_type = glType;
		image->gl_type_size = glTypeSize;
		image->gl_internal_format = glInternalFormat;
		image->gl_format = glFormat;
		image->gl_base_internal_format = glBaseInternalFormat;

		const uint32 use_num_mipmap_levels = myMax(1u, numberOfMipmapLevels);
		image->mipmap_level_data.resize(use_num_mipmap_levels);

		// for each mipmap_level in numberOfMipmapLevels
		for(uint32 lvl = 0; lvl < use_num_mipmap_levels; ++lvl)
		{
			const uint32 image_size = readUInt32(file, swap_endianness);

			// Assume there are no array elements (this is not an array texture)
			glare::AllocatorVector<uint8, 16>& cur_level_data = image->mipmap_level_data[lvl];
			cur_level_data.resize(image_size);

			file.readData(cur_level_data.data(), image_size);

			//for(uint32 face = 0; face < myMax(1u, numberOfFaces); ++face)
			//{
			//	// Assume pixelDepth is 0 or 1 (e.g. not used)
			//}

			// Read mipPadding
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
Reference<Map2D> KTXDecoder::decodeKTX2(const std::string& path)
{
	try
	{
		FileInStream file(path);

		uint8 identifier[12];
		file.readData(identifier, 12);
		if(memcmp(identifier, ktx2_file_id, 12) != 0)
			throw glare::Exception("Invalid file id");

		const uint32 vkFormat    = file.readUInt32();
		const uint32 glTypeSize  = file.readUInt32();
		const uint32 pixelWidth  = file.readUInt32(); // The size of the texture image for level 0, in pixels.
		const uint32 pixelHeight = file.readUInt32();
		const uint32 pixelDepth  = file.readUInt32(); // For 2D and cubemap textures, pixelDepth must be 0.
		const uint32 layerCount  = file.readUInt32(); // layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.
		const uint32 faceCount   = file.readUInt32(); // faceCount specifies the number of cubemap faces
		const uint32 levelCount  = file.readUInt32(); // levelCount specifies the number of levels in the Mip Level Array
		const uint32 supercompressionScheme = file.readUInt32();

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


		const uint32 dfdByteOffset = file.readUInt32();
		const uint32 dfdByteLength = file.readUInt32();
		const uint32 kvdByteOffset = file.readUInt32();
		const uint32 kvdByteLength = file.readUInt32();
		const uint64 sgdByteOffset = file.readUInt64();
		const uint64 sgdByteLength = file.readUInt64();

		// Read level index
		struct LevelData
		{
			uint64 byteOffset; // The offset from the start of the file of the first byte of image data for mip level p.
			uint64 byteLength; // The total size of the data for supercompressed mip level p.
			uint64 uncompressedByteLength; // levels[p].uncompressedByteLength is the number of bytes of pixel data in LOD levelp after reflation from supercompression.
		};

		const size_t use_num_mipmap_levels = myMax<size_t>(1, levelCount);
		std::vector<LevelData> level_data(use_num_mipmap_levels);

		file.readData(level_data.data(), level_data.size() * sizeof(LevelData));

		const size_t N = 3;
		CompressedImageRef image = new CompressedImage(pixelWidth, pixelHeight, N);

		if(vkFormat == VK_FORMAT_BC6H_UFLOAT_BLOCK)
		{
			image->gl_type = GL_RGB; // correct?
			image->gl_type_size = 0; // not sure
			image->gl_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
			image->gl_format = GL_RGB;
			image->gl_base_internal_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT; // Correct?
		}
		else if(vkFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK) // Aka DXT1 (DXT without alpha)
		{
			image->gl_type = GL_UNSIGNED_BYTE; // correct?
			image->gl_type_size = 0; // not sure
			image->gl_internal_format = GL_RGB8;
			image->gl_format = GL_RGB;
			image->gl_base_internal_format = GL_EXT_COMPRESSED_RGB_S3TC_DXT1_EXT; // Correct?
		}
		else if(vkFormat == VK_FORMAT_BC3_UNORM_BLOCK) // Aka DXT5 (DXT with alpha)
		{
			image->gl_type = GL_UNSIGNED_BYTE; // correct?
			image->gl_type_size = 0; // not sure
			image->gl_internal_format = GL_RGBA8;
			image->gl_format = GL_RGBA;
			image->gl_base_internal_format = GL_EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT; // Correct?
		}
		else
			throw glare::Exception("Unhandled vkFormat " + toString(vkFormat) + ".");

		
		image->mipmap_level_data.resize(use_num_mipmap_levels);

		// Read levels in reverse order, since lowest level mipmap (smallest) should be first in file.
		for(int lvl = (int)use_num_mipmap_levels - 1; lvl >= 0; --lvl)
		{
			file.setReadIndex(level_data[lvl].byteOffset);
			
			// Assume there are no array elements (this is not an array texture)
			glare::AllocatorVector<uint8, 16>& cur_level_data = image->mipmap_level_data[lvl];

			if(level_data[lvl].uncompressedByteLength > 100000000) // Fail on excessively large files.
				throw glare::Exception("uncompressedByteLength is too large: " + toString(level_data[lvl].uncompressedByteLength));

			cur_level_data.resize(level_data[lvl].uncompressedByteLength);

			if(supercompressionScheme == 0) // If no compression:
			{
				file.readData(cur_level_data.data(), level_data[lvl].uncompressedByteLength);
			}
			else if(supercompressionScheme == 2) // ZSTD compression:
			{
				if((level_data[lvl].byteLength >= file.fileSize()) || (file.getReadIndex() + level_data[lvl].byteLength > file.fileSize())) // Check compressed_size is valid, while taking care with wraparound
					throw glare::Exception("Compressed size is too large.");

				const uint64 decompressed_size = ZSTD_getFrameContentSize(file.currentReadPtr(), level_data[lvl].byteLength);
				if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
					throw glare::Exception("Failed to get decompressed_size");

				if(decompressed_size != level_data[lvl].uncompressedByteLength)
					throw glare::Exception("decompressed_size did not match uncompressedByteLength");

				// Decompress data into 'cur_level_data' buffer.
				const size_t res = ZSTD_decompress(cur_level_data.data(), decompressed_size, file.currentReadPtr(), /*compressed size=*/level_data[lvl].byteLength);
				if(ZSTD_isError(res))
					throw glare::Exception("Decompression of buffer failed: " + toString(res));
				if(res < decompressed_size)
					throw glare::Exception("Decompression of buffer failed: not enough bytes in result");
			}
		}

		return image;
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


void KTXDecoder::supercompressKTX2File(const std::string& path_in, const std::string& path_out)
{
	CompressedImageRef image = KTXDecoder::decodeKTX2(path_in).downcast<CompressedImage>();

	FileOutStream file(path_out);
	file.writeData(ktx2_file_id, 12);

	file.writeUInt32(VK_FORMAT_BC6H_UFLOAT_BLOCK); // vkFormat
	file.writeUInt32(1); // glTypeSize: For formats whose Vulkan names have the suffix _BLOCK it must equal 1
	file.writeUInt32((uint32)image->getMapWidth()); // pixelWidth: The size of the texture image for level 0, in pixels.
	file.writeUInt32((uint32)image->getMapHeight()); // pixelHeight
	file.writeUInt32(0); // pixelDepth: For 2D and cubemap textures, pixelDepth must be 0.
	file.writeUInt32(0); // layerCount: layerCount specifies the number of array elements. If the texture is not an array texture, layerCount must equal 0.
	file.writeUInt32(1); // faceCount specifies the number of cubemap faces: For non cubemaps this must be 1
	file.writeUInt32((uint32)image->mipmap_level_data.size()); // levelCount: levelCount specifies the number of levels in the Mip Level Array
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

	std::vector<LevelData> level_data(image->mipmap_level_data.size());

	const size_t mip_level_byte_start = file.getWriteIndex() + level_data.size() * sizeof(LevelData);
	size_t level_byte_write_i = mip_level_byte_start;

	js::Vector<uint8, 16> compressed_data;

	for(int i=(int)image->mipmap_level_data.size() - 1; i>=0; --i)
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
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"


void KTXDecoder::test()
{
	conPrint("KTXDecoder::test()");

	try
	{
		Reference<Map2D> im;
		
		//----------------------------------- Test loading KTX files -------------------------------------------
		im = KTXDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 1); // no mipmaps.

		im = KTXDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels

		//----------------------------------- Test loading KTX2 files -------------------------------------------
		im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX2");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 1); // no mipmaps.

		im = KTXDecoder::decodeKTX2(TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels
		
		

		//---------------------------------- Test supercompressKTX2File -------------------------------------------
		{
			const std::string src_path  = TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2";
			const std::string dest_path = PlatformUtils::getTempDirPath() + "/lightmap_BC6H_with_mipmaps_supercompressed.KTX2";

			KTXDecoder::supercompressKTX2File(src_path, dest_path);

			// Check suprecompressed file.
			im = KTXDecoder::decodeKTX2(dest_path);
			testAssert(im->getMapWidth() == 512);
			testAssert(im->getMapHeight() == 512);
			testAssert(im.isType<CompressedImage>());
			testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels

			conPrint("raw KTX2 file size:             " + toString(FileUtils::getFileSize(src_path)));
			conPrint("supercompressed KTX2 file size: " + toString(FileUtils::getFileSize(dest_path)));
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	conPrint("KTXDecoder::test() done.");
}


#endif // BUILD_TESTS
