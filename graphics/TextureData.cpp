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


size_t bytesPerPixel(OpenGLTextureFormat format)
{
	assert(!isCompressed(format));

	switch(format)
	{
		case Format_Greyscale_Uint8: return 1;
		case Format_Greyscale_Float: return 4;
		case Format_Greyscale_Half: return 2;
		case Format_SRGB_Uint8: return 3;
		case Format_SRGBA_Uint8: return 4;
		case Format_RGB_Linear_Uint8: return 3;
		case Format_RGB_Integer_Uint8: return 3;
		case Format_RGBA_Linear_Uint8: return 4;
		case Format_RGBA_Integer_Uint8: return 4;
		case Format_RGB_Linear_Float: return 12;
		case Format_RGB_Linear_Half: return 6;
		case Format_RGBA_Linear_Float: return 16;
		case Format_RGBA_Linear_Half: return 8;
		case Format_Depth_Float: return 4;
		case Format_Depth_Uint16: return 2;
		default:
			assert(0);
			return 1;
	}
}


size_t bytesPerBlock(OpenGLTextureFormat format)
{
	assert(isCompressed(format));

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
		case Format_RGBA_Linear_Float: return 4;
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
		case Format_RGBA_Linear_Float: return "Format_RGBA_Linear_Float";
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
		case Format_RGBA_Linear_Float: return 32;
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
	const size_t level_w = myMax<size_t>(1, base_W >> level);        assert(level_w == myMax<size_t>(1, base_W / ((size_t)1 << level)));
	const size_t level_h = myMax<size_t>(1, base_H >> level);        assert(level_h == myMax<size_t>(1, base_H / ((size_t)1 << level)));
	
	const size_t level_x_blocks = Maths::roundedUpDivide<size_t>(level_w, 4);
	const size_t level_y_blocks = Maths::roundedUpDivide<size_t>(level_h, 4);

	return level_x_blocks * level_y_blocks;
}


size_t TextureData::computeStorageSizeB(size_t W, size_t H, OpenGLTextureFormat format, bool include_mip_levels)
{
	if(::isCompressed(format))
	{
		const size_t block_B = bytesPerBlock(format);
		if(include_mip_levels)
		{
			size_t sum_num_blocks = 0;
			while(1)
			{
				const size_t x_blocks = Maths::roundedUpDivide<size_t>(W, 4);
				const size_t y_blocks = Maths::roundedUpDivide<size_t>(H, 4);
				sum_num_blocks += x_blocks * y_blocks;

				if(W == 1 && H == 1)
					break;

				W = myMax<size_t>(1, W / 2);
				H = myMax<size_t>(1, H / 2);
			}
			return sum_num_blocks * block_B;
		}
		else
			return block_B * computeNum4PixelBlocksForLevel(W, H, /*level=*/0);
	}
	else
	{
		const size_t pixel_B = bytesPerPixel(format);
		if(include_mip_levels)
		{
			size_t sum_num_pixels = 0;
			while(1)
			{
				sum_num_pixels += W * H;

				if(W == 1 && H == 1)
					break;

				W = myMax<size_t>(1, W / 2);
				H = myMax<size_t>(1, H / 2);
			}
			return sum_num_pixels * pixel_B;
		}
		else
			return pixel_B * W * H;
	}
}
