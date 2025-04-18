/*=====================================================================
TextureData.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TextureData.h"


bool isCompressed(OpenGLTextureFormat format)
{
	return 
		format == Format_Compressed_DXT_RGB_Uint8 ||
		format == Format_Compressed_DXT_RGBA_Uint8 ||
		format == Format_Compressed_DXT_SRGB_Uint8 ||
		format == Format_Compressed_DXT_SRGBA_Uint8 ||
		format == Format_Compressed_BC6H ||
		format == Format_Compressed_ETC2_RGB_Uint8 ||
		format == Format_Compressed_ETC2_RGBA_Uint8 ||
		format == Format_Compressed_ETC2_SRGB_Uint8 ||
		format == Format_Compressed_ETC2_SRGBA_Uint8;

};


size_t bytesPerBlock(OpenGLTextureFormat format)
{
	switch(format)
	{
		case Format_Compressed_DXT_RGB_Uint8: return 8;
		case Format_Compressed_DXT_RGBA_Uint8: return 16;
		case Format_Compressed_DXT_SRGB_Uint8: return 8;
		case Format_Compressed_DXT_SRGBA_Uint8: return 16;
		case Format_Compressed_BC6H: return 16;
		case Format_Compressed_ETC2_RGB_Uint8: return 8;
		case Format_Compressed_ETC2_RGBA_Uint8: return 16;
		case Format_Compressed_ETC2_SRGB_Uint8: return 8;
		case Format_Compressed_ETC2_SRGBA_Uint8: return 16;
		default:
			assert(0);
			return 1;
	}
}


size_t numChannels(OpenGLTextureFormat format)
{
	switch(format)
	{
		case Format_Greyscale_Uint8: return 1;
		case Format_Greyscale_Float: return 1;
		case Format_Greyscale_Half: return 1;
		case Format_SRGB_Uint8: return 3;
		case Format_SRGBA_Uint8: return 4;
		case Format_RGB_Linear_Uint8: return 3;
		case Format_RGB_Integer_Uint8: return 3;
		case Format_RGBA_Linear_Uint8: return 4;
		case Format_RGBA_Integer_Uint8: return 4;
		case Format_RGB_Linear_Float: return 3;
		case Format_RGB_Linear_Half: return 3;
		case Format_RGBA_Linear_Half: return 4;
		case Format_Depth_Float: return 1;
		case Format_Depth_Uint16: return 1;
		case Format_Compressed_DXT_RGB_Uint8: return 3;
		case Format_Compressed_DXT_RGBA_Uint8: return 4;
		case Format_Compressed_DXT_SRGB_Uint8: return 3;
		case Format_Compressed_DXT_SRGBA_Uint8: return 4;
		case Format_Compressed_BC6H: return 3;
		case Format_Compressed_ETC2_RGB_Uint8: return 3;
		case Format_Compressed_ETC2_RGBA_Uint8: return 4;
		case Format_Compressed_ETC2_SRGB_Uint8: return 3;
		case Format_Compressed_ETC2_SRGBA_Uint8: return 4;
		default:
			assert(0);
			return 1;
	}
}


const char* textureFormatString(OpenGLTextureFormat format)
{
	switch(format)
	{
		case Format_Greyscale_Uint8: return "Format_Greyscale_Uint8";
		case Format_Greyscale_Float: return "Format_Greyscale_Float";
		case Format_Greyscale_Half: return "Format_Greyscale_Half";
		case Format_SRGB_Uint8: return "Format_SRGB_Uint8";
		case Format_SRGBA_Uint8: return "Format_SRGBA_Uint8";
		case Format_RGB_Linear_Uint8: return "Format_RGB_Linear_Uint8";
		case Format_RGB_Integer_Uint8: return "Format_RGB_Integer_Uint8";
		case Format_RGBA_Linear_Uint8: return "Format_RGBA_Linear_Uint8";
		case Format_RGBA_Integer_Uint8: return "Format_RGBA_Integer_Uint8";
		case Format_RGB_Linear_Float: return "Format_RGB_Linear_Float";
		case Format_RGB_Linear_Half: return "Format_RGB_Linear_Half";
		case Format_RGBA_Linear_Half: return "Format_RGBA_Linear_Half";
		case Format_Depth_Float: return "Format_Depth_Float";
		case Format_Depth_Uint16: return "Format_Depth_Uint16";
		case Format_Compressed_DXT_RGB_Uint8: return "Format_Compressed_DXT_RGB_Uint8";
		case Format_Compressed_DXT_RGBA_Uint8: return "Format_Compressed_DXT_RGBA_Uint8";
		case Format_Compressed_DXT_SRGB_Uint8: return "Format_Compressed_DXT_SRGB_Uint8";
		case Format_Compressed_DXT_SRGBA_Uint8: return "Format_Compressed_DXT_SRGBA_Uint8";
		case Format_Compressed_BC6H: return "Format_Compressed_BC6H";
		case Format_Compressed_ETC2_RGB_Uint8: return "Format_Compressed_ETC2_RGB_Uint8";
		case Format_Compressed_ETC2_RGBA_Uint8: return "Format_Compressed_ETC2_RGBA_Uint8";
		case Format_Compressed_ETC2_SRGB_Uint8: return "Format_Compressed_ETC2_SRGB_Uint8";
		case Format_Compressed_ETC2_SRGBA_Uint8: return "Format_Compressed_ETC2_SRGBA_Uint8";
		default:
			assert(0);
			return "Unknown";
	}
}


size_t TextureData::numChannels() const
{
	return ::numChannels(this->format);
}


double TextureData::uncompressedBitsPerChannel() const
{
	switch(format)
	{
		case Format_Greyscale_Uint8: return 8;
		case Format_Greyscale_Float: return 32;
		case Format_Greyscale_Half: return 16;
		case Format_SRGB_Uint8: return 8;
		case Format_SRGBA_Uint8: return 8;
		case Format_RGB_Linear_Uint8: return 8;
		case Format_RGB_Integer_Uint8: return 8;
		case Format_RGBA_Linear_Uint8: return 8;
		case Format_RGBA_Integer_Uint8: return 8;
		case Format_RGB_Linear_Float: return 32;
		case Format_RGB_Linear_Half: return 16;
		case Format_RGBA_Linear_Half: return 16;
		case Format_Depth_Float: return 32; // ?
		case Format_Depth_Uint16: return 16;
		case Format_Compressed_DXT_RGB_Uint8: return 8;
		case Format_Compressed_DXT_RGBA_Uint8: return 8;
		case Format_Compressed_DXT_SRGB_Uint8: return 8;
		case Format_Compressed_DXT_SRGBA_Uint8: return 8;
		case Format_Compressed_BC6H: return 16;
		case Format_Compressed_ETC2_RGB_Uint8: return 8;
		case Format_Compressed_ETC2_RGBA_Uint8: return 8;
		case Format_Compressed_ETC2_SRGB_Uint8: return 8;
		case Format_Compressed_ETC2_SRGBA_Uint8: return 8;
		default:
			assert(0);
			return 8;
	}
}
	

size_t TextureData::totalCPUMemUsage() const
{
	size_t sum = mipmap_data.dataSizeBytes();

	if(converted_image)
		sum += converted_image->getByteSize();

	sum += frame_end_times.capacity() * sizeof(double);

	return sum;
}


size_t TextureData::computeNumMipLevels(size_t W, size_t H)
{
	size_t num = 0;
	while(1)
	{
		num++;

		if(W == 1 && H == 1)
			break;

		W = myMax<size_t>(1, W / 2);
		H = myMax<size_t>(1, H / 2);
	}
	return num;
}


size_t TextureData::computeNum4PixelBlocksForLevel(size_t base_W, size_t base_H, size_t level)
{
	const size_t expected_w = myMax<size_t>(1, base_W / ((size_t)1 << level));
	const size_t expected_h = myMax<size_t>(1, base_H / ((size_t)1 << level));

	const size_t expected_x_blocks = Maths::roundedUpDivide<size_t>(expected_w, 4);
	const size_t expected_y_blocks = Maths::roundedUpDivide<size_t>(expected_h, 4);

	return expected_x_blocks * expected_y_blocks;
}
