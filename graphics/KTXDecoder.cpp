/*=====================================================================
KTXDecoder.cpp
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "KTXDecoder.h"


#include "imformatdecoder.h"
#include "CompressedImage.h"
#include "../utils/FileInStream.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include <memory.h>


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


static uint8 ktx_file_id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };


// throws ImFormatExcep on failure
Reference<Map2D> KTXDecoder::decode(const std::string& path)
{
	try
	{
		FileInStream file(path);

		uint8 identifier[12];
		file.readData(identifier, 12);
		if(memcmp(identifier, ktx_file_id, 12) != 0)
			throw Indigo::Exception("Invalid file id");

		const uint32 endianness = file.readUInt32();
		bool swap_endianness;
		if(endianness == 0x04030201u)
			swap_endianness = false;
		else if(endianness == 0x01020304u)
			swap_endianness = true;
		else
			throw Indigo::Exception("invalid endianness value.");

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
			throw Indigo::Exception("numberOfArrayElements > 0 not supported.");

		if(pixelDepth > 1)
			throw Indigo::Exception("pixelDepth > 1 not supported.");

		if(numberOfFaces != 1)
			throw Indigo::Exception("numberOfFaces != 1 not supported.");

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
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"


void KTXDecoder::test()
{
	conPrint("KTXDecoder::test()");

	try
	{
		Reference<Map2D> im;

		im = KTXDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 1); // no mipmaps.

		im = KTXDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX");
		testAssert(im->getMapWidth() == 512);
		testAssert(im->getMapHeight() == 512);
		testAssert(im.isType<CompressedImage>());
		testAssert(im.downcastToPtr<CompressedImage>()->mipmap_level_data.size() == 10); // 512, 256, 128, 64, 32, 16, 8, 4, 2, 1 = 10 levels
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	conPrint("KTXDecoder::test() done.");
}


#endif // BUILD_TESTS
