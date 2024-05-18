/*=====================================================================
PNGDecoder.cpp
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "PNGDecoder.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/MemMappedFile.h"
#include "../utils/BufferViewInStream.h"

#define LIBPNG_SUPPORT 1 // TEMP
#if LIBPNG_SUPPORT
#include <png.h>
#endif
#if !defined NO_LCMS_SUPPORT
#include <lcms2.h>
#endif


#if WUFFS_SUPPORT

#if defined(__x86_64__) || defined(_M_X64)
#define WUFFS_BASE__CPU_ARCH__X86_FAMILY // This needs to be defined for SSE instrinics use in Wuffs, which speeds up Wuffs quite a lot.
#endif

#define WUFFS_IMPLEMENTATION

#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB

#define WUFFS_CONFIG__MODULE__AUX__BASE
#define WUFFS_CONFIG__MODULE__AUX__IMAGE
#include "wuffs/wuffs-v0.3.c"
#endif


#ifndef PNG_ALLOW_BENIGN_ERRORS
#error PNG_ALLOW_BENIGN_ERRORS should be defined in preprocessor defs.
#endif
#ifndef PNG_INTEL_SSE
#error PNG_INTEL_SSE should be defined in preprocessor defs.
#endif
#ifndef PNG_NO_SETJMP
#error PNG_NO_SETJMP should be defined in preprocessor defs.
#endif


// See 'Precompute colour profile data' below.
static const size_t sRGB_profile_data_size = 592;
static const uint8 sRGB_profile_data[] = { 0, 0, 2, 80, 108, 99, 109, 115, 4, 48, 0, 0, 109, 110, 116, 114, 82, 71, 66, 32, 88, 89, 90, 32, 7, 227, 0, 5, 0, 28, 0, 6, 0, 20, 0, 39, 97, 99, 115, 112, 77, 83, 70, 84, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 246, 214, 0, 1, 0, 0, 0, 0, 211, 45, 108, 99, 109, 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 100, 101, 115, 99, 0, 0, 1, 8, 0, 0, 0, 56, 99, 112, 114, 116, 0, 0, 1, 64, 0, 0, 0, 78, 119, 116, 112, 116, 0, 0, 1, 144, 0, 0, 0, 20, 99, 104, 97, 100, 0, 0, 1, 164, 0, 0, 0, 44, 114,
	88, 89, 90, 0, 0, 1, 208, 0, 0, 0, 20, 98, 88, 89, 90, 0, 0, 1, 228, 0, 0, 0, 20, 103, 88, 89, 90, 0, 0, 1, 248, 0, 0, 0, 20, 114, 84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 103, 84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 98,
	84, 82, 67, 0, 0, 2, 12, 0, 0, 0, 32, 99, 104, 114, 109, 0, 0, 2, 44, 0, 0, 0, 36, 109, 108, 117, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 12, 101, 110, 85, 83, 0, 0, 0, 28, 0, 0, 0, 28, 0, 115, 0, 82, 0, 71, 0, 66, 0,
	32, 0, 98, 0, 117, 0, 105, 0, 108, 0, 116, 0, 45, 0, 105, 0, 110, 0, 0, 109, 108, 117, 99, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 12, 101, 110, 85, 83, 0, 0, 0, 50, 0, 0, 0, 28, 0, 78, 0, 111, 0, 32, 0, 99, 0, 111, 0, 112, 0,
	121, 0, 114, 0, 105, 0, 103, 0, 104, 0, 116, 0, 44, 0, 32, 0, 117, 0, 115, 0, 101, 0, 32, 0, 102, 0, 114, 0, 101, 0, 101, 0, 108, 0, 121, 0, 0, 0, 0, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 246, 214, 0, 1, 0, 0, 0, 0, 211, 45, 115,
	102, 51, 50, 0, 0, 0, 0, 0, 1, 12, 74, 0, 0, 5, 227, 255, 255, 243, 42, 0, 0, 7, 155, 0, 0, 253, 135, 255, 255, 251, 162, 255, 255, 253, 163, 0, 0, 3, 216, 0, 0, 192, 148, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 111, 148, 0, 0, 56, 238, 0,
	0, 3, 144, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 36, 157, 0, 0, 15, 131, 0, 0, 182, 190, 88, 89, 90, 32, 0, 0, 0, 0, 0, 0, 98, 165, 0, 0, 183, 144, 0, 0, 24, 222, 112, 97, 114, 97, 0, 0, 0, 0, 0, 3, 0, 0, 0, 2, 102, 102, 0,
	0, 242, 167, 0, 0, 13, 89, 0, 0, 19, 208, 0, 0, 10, 91, 99, 104, 114, 109, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 163, 215, 0, 0, 84, 123, 0, 0, 76, 205, 0, 0, 153, 154, 0, 0, 38, 102, 0, 0, 15, 92
};


// TRY:
// png_set_keep_unknown_chunks(png_ptr, 0, NULL, 0);
// or
// png_set_keep_unknown_chunks(png_ptr, 1, NULL, 0);

#if LIBPNG_SUPPORT
static void pngdecoder_error_func(png_structp /*png*/, const char* msg)
{
	throw ImFormatExcep("Error while processing PNG file: " + std::string(msg));
}


static void pngdecoder_warning_func(png_structp /*png*/, const char* /*msg*/)
{
	//const std::string* path = (const std::string*)png->error_ptr;
	//conPrint("PNG Warning while loading file '" + *path + "': " + std::string(msg));
}


static void pngdecoder_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	BufferViewInStream* stream = (BufferViewInStream*)png_get_io_ptr(png_ptr);

	try
	{
		stream->readData(data, length);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep("Error while processing PNG file: " + e.what());
	}
}
#endif // LIBPNG_SUPPORT


Reference<Map2D> PNGDecoder::decode(const std::string& path, glare::Allocator* mem_allocator)
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


#if WUFFS_SUPPORT

static inline void throwOnError(wuffs_base__status status)
{
	if(status.is_error())
		throw ImFormatExcep("Error: " + std::string(status.message()));
}


static inline uint32 decodeWuffsBitDepth(uint32 x)
{
	/*
	https://github.com/google/wuffs/blob/main/doc/note/pixel-formats.md:
	Bit depth is encoded in 4 bits:
	0 means the channel or index is unused.
	x means a bit depth of x, for x in the range 1 ..= 8.
	9 means a bit depth of 10.
	10 means a bit depth of 12.
	11 means a bit depth of 16.
	12 means a bit depth of 24.
	13 means a bit depth of 32.
	14 means a bit depth of 48.
	15 means a bit depth of 64.
	*/
	if(x <= 8)
		return x;
	switch(x)
	{
	case 9: return 10;
	case 10: return 12;
	case 11: return 16;
	case 12: return 24;
	case 13: return 32;
	case 14: return 48;
	case 15: return 64;
	}
	return 0;
}

/*
Wuffs (Wrangling Untrusted File Formats Safely) provides faster and hopefully safer decoding of PNG files.

It however still has some limitations on the formats it handles.
For example it will load a 3x16 bit rgb color PNG file as a 4x16 bit format.

As such we won't make it the default way to load PNGs for now, but will continue to use LibPNG as the default.  
Define WUFFS_SUPPORT to make Wuffs the default way to load PNGs.
*/
static GLARE_NO_INLINE Reference<Map2D> doDecodeFromBufferWithWuffs(BufferViewInStream& buffer_view_in_stream, glare::Allocator* mem_allocator)
{
	try
	{
		wuffs_png__decoder png_decoder;
		wuffs_base__status status = wuffs_png__decoder__initialize(&png_decoder, sizeof(png_decoder), WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
		throwOnError(status);

		// We will ignore PNG checksums.
		wuffs_png__decoder__set_quirk_enabled(&png_decoder, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, true);

		const size_t effective_buf_size = buffer_view_in_stream.size() - buffer_view_in_stream.getReadIndex();
		wuffs_base__io_buffer src_io_buffer = wuffs_base__make_io_buffer(
			wuffs_base__make_slice_u8((uint8*)buffer_view_in_stream.currentReadPtr(), effective_buf_size),
			wuffs_base__make_io_buffer_meta(/*write index=*/effective_buf_size, /*read index=*/0, /*pos=*/effective_buf_size, /*closed=*/true)
		);


		wuffs_base__image_config image_config;
		status = wuffs_png__decoder__decode_image_config(&png_decoder, &image_config, &src_io_buffer);
		throwOnError(status);

		const wuffs_base__pixel_format pixel_format = wuffs_base__pixel_config__pixel_format(&image_config.pixcfg);

		const uint32_t w = wuffs_base__pixel_config__width(&image_config.pixcfg);
		const uint32_t h = wuffs_base__pixel_config__height(&image_config.pixcfg);
		//const uint32_t num_planes = wuffs_base__pixel_config__pixel_format(&image_config.pixcfg).num_planes();
		const uint32_t bpp = wuffs_base__pixel_config__pixel_format(&image_config.pixcfg).bits_per_pixel();

		if(w >= 1000000)
			throw ImFormatExcep("invalid width: " + toString(w));
		if(h >= 1000000)
			throw ImFormatExcep("invalid height: " + toString(h));

		const size_t max_num_pixels = 1 << 28;
		if((size_t)w * (size_t)h > max_num_pixels)
			throw ImFormatExcep("invalid width and height (too many pixels): " + toString(w) + ", " + toString(h));

		// See https://github.com/google/wuffs/blob/main/doc/note/pixel-formats.md
		const uint32 channel_0_bits = decodeWuffsBitDepth((pixel_format.repr >>  0) & 15); // Bits 0 ..= 3 encodes the number of bits (depth) in the 0th channel.
		//const uint32 channel_1_bits = decodeWuffsBitDepth((pixel_format.repr >>  4) & 15); // Bits 4 ..=  7 encodes the number of bits (depth) in the 1st channel.
		//const uint32 channel_2_bits = decodeWuffsBitDepth((pixel_format.repr >>  8) & 15); // Bits 8 ..= 11 encodes the number of bits (depth) in the 2nd channel.
		//const uint32 channel_3_bits = decodeWuffsBitDepth((pixel_format.repr >> 12) & 15); // Bits 12 ..= 15 encodes the number of bits (depth) in the 3rd channel.

		//const uint32 colour_type = (pixel_format.repr >> 28) & 15;
		//const uint32 transparency = (pixel_format.repr >> 24) & 3;
		const uint32 palette_indexed = (pixel_format.repr >> 18) & 1;

		//printVar(channel_0_bits);
		//printVar(channel_1_bits);
		//printVar(channel_2_bits);
		//printVar(channel_3_bits);
		//printVar(colour_type);
		//printVar(transparency);
		//printVar(palette_indexed);

		if(channel_0_bits == 0)
			throw ImFormatExcep("channel 0 had 0 bit depth");

		// Configure the work buffer.
		const uint64 workbuf_len = wuffs_png__decoder__workbuf_len(&png_decoder).max_incl;
		const uint64 MAX_WORKBUF_ARRAY_SIZE = 256 * 1024 * 1024;
		if(workbuf_len > MAX_WORKBUF_ARRAY_SIZE)
			throw ImFormatExcep("main: image is too large (to configure work buffer)");


		glare::AllocatorVector<uint8, 16> workbuf_array;
		if(mem_allocator)
			workbuf_array.setAllocator(mem_allocator);
		workbuf_array.resizeNoCopy(workbuf_len); // NOTE: does not zero memory
		const wuffs_base__slice_u8 workbuf_slice = wuffs_base__make_slice_u8(workbuf_array.data(), workbuf_len);


		wuffs_base__decode_frame_options options;
		memset(&options, 0, sizeof(wuffs_base__decode_frame_options));

		const wuffs_base__pixel_blend blend = WUFFS_BASE__PIXEL_BLEND__SRC;

		int N; // num channels
		if(palette_indexed)
		{
			N = 3;
		}
		else
		{
			N = bpp / channel_0_bits;
		}

		uint32 use_pixelformat;
		if(N == 1)
		{
			if(channel_0_bits == 8)
				use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__Y;
			else if(channel_0_bits == 16)
				use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__Y_16LE;
			else
				throw ImFormatExcep("Unhandled channel_0_bits: " + toString(channel_0_bits));
		}
		else if(N == 3)
		{
			if(channel_0_bits == 8)
				use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__BGR;
			//else if(channel_0_bits == 16)
			//	use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE;
			else
				throw ImFormatExcep("Unhandled channel_0_bits: " + toString(channel_0_bits));
		}
		else if(N == 4)
		{
			if(channel_0_bits == 8)
				use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL;
			else if(channel_0_bits == 16)
				use_pixelformat = WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE;
			else
				throw ImFormatExcep("Unhandled channel_0_bits: " + toString(channel_0_bits));
		}
		else
			throw ImFormatExcep("Unhandled num components: " + toString(N));


		wuffs_base__pixel_config destination_pixcfg;
		wuffs_base__pixel_config__set(&destination_pixcfg,
			use_pixelformat,
			WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, 
			w, h
		);


		if(channel_0_bits == 8)
		{
			ImageMapUInt8Ref imagemap = new ImageMapUInt8(w, h, N, mem_allocator);

			wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8(imagemap->getData(), imagemap->getDataSize());

			// Configure the wuffs_base__pixel_buffer struct.
			wuffs_base__pixel_buffer dest_pixel_buffer;
			status = wuffs_base__pixel_buffer__set_from_slice(&dest_pixel_buffer, &destination_pixcfg, pixbuf_slice);
			throwOnError(status);

			status = wuffs_png__decoder__decode_frame(&png_decoder, &dest_pixel_buffer, &src_io_buffer, blend, workbuf_slice, &options);
			throwOnError(status);

			
			const size_t num_pixels = (size_t)w * (size_t)h;
			uint8* const dest = imagemap->getData();
			if(N == 3)
			{
				// Convert from BGR to RGB
				for(size_t i=0; i<num_pixels; ++i)
				{
					const uint8 r = dest[i * 3 + 2];
					const uint8 g = dest[i * 3 + 1];
					const uint8 b = dest[i * 3 + 0];
					dest[i * 3 + 0] = r;
					dest[i * 3 + 1] = g;
					dest[i * 3 + 2] = b;
				}
			}
			else if(N == 4)
			{
				// Convert from BGRA to RGBA
				for(size_t i=0; i<num_pixels; ++i)
				{
					const uint8 r = dest[i * 4 + 2];
					const uint8 g = dest[i * 4 + 1];
					const uint8 b = dest[i * 4 + 0];
					const uint8 a = dest[i * 4 + 3];
					dest[i * 4 + 0] = r;
					dest[i * 4 + 1] = g;
					dest[i * 4 + 2] = b;
					dest[i * 4 + 3] = a;
				}
			}

			return imagemap;
		}
		else if(channel_0_bits == 16)
		{
			Reference<ImageMap<uint16_t, UInt16ComponentValueTraits> > imagemap = new ImageMap<uint16_t, UInt16ComponentValueTraits>(w, h, N, mem_allocator);

			wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8((uint8*)imagemap->getData(), imagemap->getDataSize() * sizeof(uint16));

			// Configure the wuffs_base__pixel_buffer struct.
			wuffs_base__pixel_buffer dest_pixel_buffer;
			status = wuffs_base__pixel_buffer__set_from_slice(&dest_pixel_buffer, &destination_pixcfg, pixbuf_slice);
			throwOnError(status);

			status = wuffs_png__decoder__decode_frame(&png_decoder, &dest_pixel_buffer, &src_io_buffer, blend, workbuf_slice, &options);
			throwOnError(status);

			const size_t num_pixels = (size_t)w * (size_t)h;
			uint16* const dest = imagemap->getData();
			if(N == 3)
			{
				// Convert from BGR to RGB
				for(size_t i=0; i<num_pixels; ++i)
				{
					const uint16 r = dest[i * 3 + 2];
					const uint16 g = dest[i * 3 + 1];
					const uint16 b = dest[i * 3 + 0];
					dest[i * 3 + 0] = r;
					dest[i * 3 + 1] = g;
					dest[i * 3 + 2] = b;
				}
			}
			else if(N == 4)
			{
				// Convert from BGRA to RGBA
				for(size_t i=0; i<num_pixels; ++i)
				{
					const uint16 r = dest[i * 4 + 2];
					const uint16 g = dest[i * 4 + 1];
					const uint16 b = dest[i * 4 + 0];
					const uint16 a = dest[i * 4 + 3];
					dest[i * 4 + 0] = r;
					dest[i * 4 + 1] = g;
					dest[i * 4 + 2] = b;
					dest[i * 4 + 3] = a;
				}
			}

			return imagemap;
		}
		else
			throw ImFormatExcep("Unhandled channel_0_bits: " + toString(channel_0_bits));
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}
#endif // WUFFS_SUPPORT


#if LIBPNG_SUPPORT
static GLARE_NO_INLINE Reference<Map2D> doDecodeFromBufferLibPNG(BufferViewInStream& buffer_view_in_stream)
{
	std::vector<png_bytep> row_pointers;

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	try
	{
		png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, // error ptr &error_out,
			pngdecoder_error_func,
			pngdecoder_warning_func
		);
		if(!png_ptr)
			throw ImFormatExcep("Failed to create PNG read struct.");


		png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE); // Set quiet-use, so checksums are not evaluated, allowing faster overall decoding.

		info_ptr = png_create_info_struct(png_ptr);
		if(!info_ptr)
			throw ImFormatExcep("Failed to create PNG info struct.");

		// Set the read callback function, to read from our data buffer.
		png_set_read_fn(png_ptr, /*io ptr=*/&buffer_view_in_stream, /*read data function=*/pngdecoder_read_data);


		png_read_info(png_ptr, info_ptr);


		// Work out gamma
		double use_gamma = 2.2;

		int intent = 0;
		if(png_get_sRGB(png_ptr, info_ptr, &intent))
		{
			// There is a sRGB chunk in this file, so it uses the sRGB colour space,
			// which in turn implies a specific gamma value.
			use_gamma = 2.2;
		}
		else
		{
			// Read gamma
			double file_gamma = 0;
			if(png_get_gAMA(png_ptr, info_ptr, &file_gamma))
			{
				// There was gamma info in the file.
				// File gamma is < 1, e.g. 0.45
				use_gamma = 1.0 / file_gamma;
			}
		}


		// Set up the transformations that we want to run.
		png_set_palette_to_rgb(png_ptr);
		png_set_expand_gray_1_2_4_to_8(png_ptr);

		// Re-read info, which will be changed based on our transformations.
		png_read_update_info(png_ptr, info_ptr);

		const uint32 width = png_get_image_width(png_ptr, info_ptr);
		const uint32 height = png_get_image_height(png_ptr, info_ptr);
		const uint32 bit_depth = png_get_bit_depth(png_ptr, info_ptr);
		const uint32 num_channels = png_get_channels(png_ptr, info_ptr);

		if(width >= 1000000)
			throw ImFormatExcep("invalid width: " + toString(width));
		if(height >= 1000000)
			throw ImFormatExcep("invalid height: " + toString(height));

		const size_t max_num_pixels = 1 << 28;
		if((size_t)width * (size_t)height > max_num_pixels)
			throw ImFormatExcep("invalid width and height (too many pixels): " + toString(width) + ", " + toString(height));

		if(num_channels > 4)
			throw ImFormatExcep("Invalid num channels: " + toString(num_channels));

		if(bit_depth != 8 && bit_depth != 16)
			throw ImFormatExcep("Invalid bit depth found: " + toString(bit_depth));

		row_pointers.resize(height);

		Map2DRef map;
		if(bit_depth == 8)
		{
			Reference<ImageMapUInt8> image_map = new ImageMapUInt8(width, height, num_channels);
			map = image_map;
			image_map->setGamma((float)use_gamma);

			for(uint32 y=0; y<height; ++y)
				row_pointers[y] = (png_bytep)image_map->getPixel(0, y);

			png_read_image(png_ptr, row_pointers.data());
		}
		else // if(bit_depth == 16)
		{
			assert(bit_depth == 16);

			// Swap to little-endian (Intel) byte order, from network byte order, which is what PNG uses.
			png_set_swap(png_ptr);

			Reference<ImageMap<uint16_t, UInt16ComponentValueTraits>> image_map = new ImageMap<uint16_t, UInt16ComponentValueTraits>(width, height, num_channels);
			map = image_map;
			image_map->setGamma((float)use_gamma);

			for(uint32 y=0; y<height; ++y)
				row_pointers[y] = (png_bytep)image_map->getPixel(0, y);

			png_read_image(png_ptr, row_pointers.data());
		}

		// Free structures
		png_destroy_read_struct(&png_ptr, &info_ptr, /*end_info_ptr_ptr=*/NULL);

		return map;
	}
	catch(ImFormatExcep& e)
	{
		// Destroy the png read struct, and the info struct, if they have been created.
		// LibPNG seems to be good about checking for NULL pointers and setting them to NULL after destruction in png_destroy_read_struct to avoid double-destroys.
		png_destroy_read_struct(&png_ptr, &info_ptr, /*end_info_ptr_ptr=*/NULL);
		throw e; // rethrow
	}
}
#endif // LIBPNG_SUPPORT


Reference<Map2D> PNGDecoder::decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator)
{
	BufferViewInStream buffer_view_in_stream(ArrayRef<uint8>((const uint8*)data, size));

#if WUFFS_SUPPORT
	return doDecodeFromBufferWithWuffs(buffer_view_in_stream, mem_allocator);
#elif LIBPNG_SUPPORT
	return doDecodeFromBufferLibPNG(buffer_view_in_stream);
#else
#error Either WUFFS_SUPPORT or LIBPNG_SUPPORT must be defined.
#endif
}


#if LIBPNG_SUPPORT
const std::map<std::string, std::string> PNGDecoder::getMetaData(const std::string& image_path)
{
	try
	{
		png_structp png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING, 
			(png_voidp)NULL,
			pngdecoder_error_func, 
			pngdecoder_warning_func
			);

		if (!png_ptr)
			throw ImFormatExcep("Failed to create PNG struct.");

		png_infop info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		{
			png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);

			throw ImFormatExcep("Failed to create PNG info struct.");
		}

		png_infop end_info = png_create_info_struct(png_ptr);
		if (!end_info)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

			throw ImFormatExcep("Failed to create PNG info struct.");
		}


		FileHandle fp(image_path, "rb");

		png_init_io(png_ptr, fp.getFile());

		/// Set read function ///
		//png_set_read_fn(png_ptr, (void*)&encoded_img[0], user_read_data_proc);

		//png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

		//png_byte* row_pointers = png_get_rows(png_ptr, info_ptr);

		png_read_info(png_ptr, info_ptr);


		png_textp text_ptr;
		const png_uint_32 num_comments = png_get_text(png_ptr, info_ptr, &text_ptr, NULL);


		std::map<std::string, std::string> metadata;
		for(png_uint_32 i=0; i<num_comments; ++i)
			metadata[std::string(text_ptr[i].key)] = std::string(text_ptr[i].text);



		// Read the info at the end of the PNG file
		//png_read_end(png_ptr, end_info);

		// Free structures
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

		return metadata;
	}
	catch(glare::Exception& )
	{
		throw ImFormatExcep("Failed to open file '" + image_path + "' for reading.");
	}
}
#endif


void PNGDecoder::write(const Bitmap& bitmap, const std::map<std::string, std::string>& metadata, const std::string& pathname)
{
	write(bitmap.getPixel(0, 0), (unsigned int)bitmap.getWidth(), (unsigned int)bitmap.getHeight(), (unsigned int)bitmap.getBytesPP(), /*bits per channel=*/8, metadata, pathname);
}


void PNGDecoder::write(const Bitmap& bitmap, const std::string& pathname)
{
	std::map<std::string, std::string> metadata;
	write(bitmap, metadata, pathname);
}


void PNGDecoder::write(const ImageMap<uint8, UInt8ComponentValueTraits>& imagemap, const std::string& pathname) // Write with no metadata
{
	write(imagemap.getData(), (unsigned int)imagemap.getWidth(), (unsigned int)imagemap.getHeight(), (unsigned int)imagemap.getN(), /*bits per channel=*/8, pathname);
}


void PNGDecoder::write(const ImageMap<uint16, UInt16ComponentValueTraits>& imagemap, const std::string& pathname) // Write with no metadata
{
	write(imagemap.getData(), (unsigned int)imagemap.getWidth(), (unsigned int)imagemap.getHeight(), (unsigned int)imagemap.getN(), /*bits per channel=*/16, pathname);
}


void PNGDecoder::write(const void* data, unsigned int W, unsigned int H, unsigned int N, unsigned int bits_per_channel, const std::string& pathname) // Write with no metadata
{
	std::map<std::string, std::string> metadata;
	write(data, W, H, N, bits_per_channel, metadata, pathname);
}


// Write with metadata
void PNGDecoder::write(const void* data, unsigned int W, unsigned int H, unsigned int N, unsigned int bits_per_channel, const std::map<std::string, std::string>& metadata, const std::string& pathname)
{
#if LIBPNG_SUPPORT
	png_struct* png = NULL;
	png_info* info = NULL;

	try
	{
		int colour_type;
		if(N == 1)
			colour_type = PNG_COLOR_TYPE_GRAY;
		else if(N == 2)
			colour_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		else if(N == 3)
			colour_type = PNG_COLOR_TYPE_RGB;
		else if(N == 4)
			colour_type = PNG_COLOR_TYPE_RGB_ALPHA;
		else
			throw ImFormatExcep("Invalid bytes pp");

		if(!(bits_per_channel == 8 || bits_per_channel == 16))
			throw ImFormatExcep("Invalid bits_per_channel");
			

		// Open the file
		FileHandle fp(pathname, "wb");
		
		// Create and initialize the png_struct with the desired error handler functions.
		png = png_create_write_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, // error user pointer
			pngdecoder_error_func, // error func
			pngdecoder_warning_func // warning func
		);
		if(!png)
			throw ImFormatExcep("Failed to create PNG object.");

		info = png_create_info_struct(png);
		if(!info)
			throw ImFormatExcep("Failed to create PNG info object.");
		
		// set up the output control if you are using standard C stream
		png_init_io(png, fp.getFile());

		//------------------------------------------------------------------------
		// Set some image info
		//------------------------------------------------------------------------
		png_set_IHDR(png, info, (png_uint_32)W, (png_uint_32)H,
			bits_per_channel, // bit depth of each channel
			colour_type, // colour type
			PNG_INTERLACE_NONE, // interlace type
			PNG_COMPRESSION_TYPE_DEFAULT, // compression type
			PNG_FILTER_TYPE_DEFAULT // filter method
		);

		// Write an ICC sRGB colour profile.
		// NOTE: We could write an sRGB Chunk instead, see section '11.3.3.5 sRGB Standard RGB colour space' (http://www.libpng.org/pub/png/spec/iso/index-object.html#11iCCP)
		//
		// Checking embedded ICC profiles can be done with this online tool: 'Jeffrey's Image Metadata Viewer': http://exif.regex.info/exif.cgi
		if(N > 1) // It's not allowed to have a colour profile in a greyscale image.
		{
#if 1 // if SAVE_PRECOMPUTED_SRGB_PROFILE:
	
	#if PNG_LIBPNG_VER >= 10634
			png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_const_bytep)sRGB_profile_data, sRGB_profile_data_size);
	#else
			png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_charp)sRGB_profile_data, sRGB_profile_data_size);
	#endif

#else // else if !SAVE_PRECOMPUTED_SRGB_PROFILE:

	#if !defined NO_LCMS_SUPPORT
			// Else get Little CMS to spit out a fresh profile.
			cmsHPROFILE profile = cmsCreate_sRGBProfile();
			if(profile == NULL)
				throw ImFormatExcep("Failed to create colour profile.");

			// Get num bytes needed to store the encoded profile.
			cmsUInt32Number profile_size = 0;
			if(cmsSaveProfileToMem(profile, NULL, &profile_size) == FALSE)
				throw ImFormatExcep("Failed to save colour profile.");

			std::vector<uint8> buf(profile_size);

			// Now write the actual profile.
			if(cmsSaveProfileToMem(profile, &buf[0], &profile_size) == FALSE)
				throw ImFormatExcep("Failed to save colour profile.");

			cmsCloseProfile(profile);

		#if PNG_LIBPNG_VER >= 10634
			png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_const_bytep)&buf[0], profile_size);
		#else
			png_set_iCCP(png, info, (png_charp)"Embedded Profile", 0, (png_charp)&buf[0], profile_size);
		#endif

	#endif // end if!!defined NO_LCMS_SUPPORT

#endif // end if !SAVE_PRECOMPUTED_SRGB_PROFILE
		}

		//------------------------------------------------------------------------
		// Write metadata pairs if present
		//------------------------------------------------------------------------
		if(metadata.size() > 0)
		{
			png_text* txt = new png_text[metadata.size()];
			memset(txt, 0, sizeof(png_text)*metadata.size());
			int c = 0;
			for(std::map<std::string, std::string>::const_iterator i = metadata.begin(); i != metadata.end(); ++i)
			{
				txt[c].compression = PNG_TEXT_COMPRESSION_NONE;
				txt[c].key = (png_charp)(*i).first.c_str();
				txt[c].text = (png_charp)(*i).second.c_str();
				txt[c].text_length = (*i).second.length();
				c++;
			}
			png_set_text(png, info, txt, (int)metadata.size());
			delete[] txt;
		}


		// Write info
		png_write_info(png, info);


		// PNG files store 16-bit pixels in network byte order (big-endian).  Use png_set_swap to convert little-endian data to big-endian.
#if defined(__x86_64__) || defined(_M_X64) // If we are targetting x64 (little endian):
		if(bits_per_channel > 8)
			png_set_swap(png);
#endif

		const size_t bytes_per_channel = (bits_per_channel == 8) ? 1 : 2;

		for(unsigned int y=0; y<H; ++y)
			png_write_row(
				png,
				(png_bytep)((const uint8*)data + y * W * N * bytes_per_channel) // Pointer to row data
			);

		//------------------------------------------------------------------------
		//finish writing file
		//------------------------------------------------------------------------
		png_write_end(png, info);

		//------------------------------------------------------------------------
		//free structs
		//------------------------------------------------------------------------
		png_destroy_write_struct(&png, &info);
	}
	catch(ImFormatExcep& e)
	{
		// Free any allocated libPNG structures, then re-throw the exception.
		if(png && info)
			png_destroy_write_struct(&png, &info);
		throw e;
	}
	catch(glare::Exception& )
	{
		if(png && info)
			png_destroy_write_struct(&png, &info);
		throw ImFormatExcep("Failed to open '" + pathname + "' for writing.");
	}

#else // else if !LIBPNG_SUPPORT:
	throw ImFormatExcep("LIBPNG_SUPPORT needs to be enabled for PNGDecoder::write()");
#endif
}


#if BUILD_TESTS


#if 0
// Command line:
// C:\fuzz_corpus\png N:\glare-core\trunk\testfiles\pngs

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		PNGDecoder::decodeFromBuffer(data, size);
	}
	catch (glare::Exception&)
	{
	}

	return 0; // Non-zero return values are reserved for future use.
}
#endif


#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "bitmap.h"


template <class T, class TTraits>
void testSavingAndLoadingRoundtrip(int W, int H, int N)
{
	try
	{
		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test3.png";

		ImageMap<T, TTraits> imagemap(W, H, N);
		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		for(int c=0; c<N; ++c)
			imagemap.getPixel(x, y)[c] = (T)((x + y + c) % TTraits::maxValue());

		// Write the map to disk
		PNGDecoder::write(imagemap, path);

		// Read it and compare with the original
		Reference<Map2D> read_image = PNGDecoder::decode(path);
		testAssert((int)read_image->getMapWidth() == W);
		testAssert((int)read_image->getMapHeight() == H);
		testAssert((int)read_image->numChannels() == N);

		if(read_image.isType<ImageMapUInt8>())
		{
			for(int y=0; y<H; ++y)
			for(int x=0; x<W; ++x)
			for(int c=0; c<N; ++c)
			{
				const T target = (T)((x + y + c) % TTraits::maxValue());
				const T read_val = read_image.downcastToPtr<ImageMapUInt8>()->getPixel(x, y)[c];
				testEqual(target, read_val);
			}
		}
		else if(read_image.isType<ImageMap<uint16, UInt16ComponentValueTraits>>())
		{
			for(int y=0; y<H; ++y)
			for(int x=0; x<W; ++x)
			for(int c=0; c<N; ++c)
			{
				const T target = (T)((x + y + c) % TTraits::maxValue());
				const T read_val = (T)read_image.downcastToPtr<ImageMap<uint16, UInt16ComponentValueTraits>>()->getPixel(x, y)[c];
				testEqual(target, read_val);
			}
		}
		else
			failTest("unexpected format");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
}


void PNGDecoder::test(const std::string& base_dir_path)
{
	conPrint("PNGDecoder::test()");

#if EMSCRIPTEN

	// Do Performance test
	if(false)
	{
		const int num_iters = 10;
		double min_time = 1.0e10;
		for(int z=0; z<num_iters; ++z)
		{
			Timer timer;
			const std::string path = base_dir_path + "/data/resources/elm_leaf_backface.png";
			const int num_repititions = 10;
			for(int q=0; q<num_repititions; ++q)
			{
				Reference<Map2D> im = decode(path);
				testAssert(im->getMapWidth() == 512);
			}
			const double time_per_load = timer.elapsed() / num_repititions;
			min_time = myMin(time_per_load, timer.elapsed());
		}
		
		conPrint("Time to load elm_leaf_backface.png: " + doubleToStringNSigFigs(min_time * 1.0e3, 4) + " ms.");
	}

#else // else if !EMSCRIPTEN:

	testSavingAndLoadingRoundtrip<uint8, UInt8ComponentValueTraits>(20, 10, /*N=*/1);
	testSavingAndLoadingRoundtrip<uint8, UInt8ComponentValueTraits>(20, 10, /*N=*/3);
	testSavingAndLoadingRoundtrip<uint8, UInt8ComponentValueTraits>(20, 10, /*N=*/4);

	testSavingAndLoadingRoundtrip<uint16, UInt16ComponentValueTraits>(20, 10, /*N=*/1);
	//testSavingAndLoadingRoundtrip<uint16, UInt16ComponentValueTraits>(20, 10, /*N=*/3); // Wuffs converts 3x16-bit format to 4x16-bit, so this test is failing.
	testSavingAndLoadingRoundtrip<uint16, UInt16ComponentValueTraits>(20, 10, /*N=*/4);


	// Leak test with a malformed file
	if(false)
	{
		while(1)
		{
			const std::string path = "C:\\fuzz_corpus\\png\\01369c6b90d84b8a7dfcb59bfb039415ad132cd6";
			try
			{
				Reference<Map2D> map = PNGDecoder::decode(path);
			}
			catch(ImFormatExcep& e)
			{
				conPrint(e.what());
			}
		}
	}


	// Test all fuzz corpus PNG files
	if(false)
	{
		while(1)
		{
			const std::vector<std::string> paths = FileUtils::getFilesInDirFullPaths("C:\\fuzz_corpus\\png");

			for(size_t i=0; i<paths.size(); ++i)
			{
				const std::string path = paths[i];

				conPrint("path: " + path);
				try
				{
					Reference<Map2D> map = PNGDecoder::decode(path);
					//printVar(map->getRefCount());
				}
				catch(ImFormatExcep& e)
				{
					conPrint(e.what());
				}
			}
		}
	}




	// Precompute colour profile data
#if 0
	if(true)
	{
		cmsHPROFILE profile = cmsCreate_sRGBProfile();
		if(profile == NULL)
			throw ImFormatExcep("Failed to create colour profile.");

		// Get num bytes needed to store the encoded profile.
		cmsUInt32Number profile_size = 0;
		if(cmsSaveProfileToMem(profile, NULL, &profile_size) == 0)
			throw ImFormatExcep("Failed to save colour profile.");

		std::vector<uint8> buf(profile_size);

		// Now write the actual profile.
		if(cmsSaveProfileToMem(profile, &buf[0], &profile_size) == 0)
			throw ImFormatExcep("Failed to save colour profile.");

		cmsCloseProfile(profile);

		std::string s = "static const size_t sRGB_profile_data_size = " + toString(profile_size) + ";\n";
		s += "static const uint8 sRGB_profile_data[] = { ";
		for(size_t i=0; i<buf.size(); ++i)
		{
			s += toString(buf[i]) + ((i + 1 < buf.size()) ? ", " : "");
			if(i % 60 == 0)
				s += "\n";
		}
		s += " }; \n";

		conPrint(s);

		assert(profile_size == sRGB_profile_data_size);

		std::vector<uint8> precomp_data(sRGB_profile_data, sRGB_profile_data + sRGB_profile_data_size);

		// NOTE: colour profile seems to have the current date in it, so bytes will not be exactly the same, see _cmsEncodeDateTimeNumber.

		//assert(precomp_data == buf);

		/*for(size_t i=0; i<buf.size(); ++i)
		{
			conPrint(toString(i) + ": " + toString(buf[i]) + " " + toString(precomp_data[i]));
			assert(buf[i] == precomp_data[i]);
		}*/
	}
#endif

	/*{
		Map2DRef map = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.png");

		Bitmap bitmap;
		bitmap.setFromImageMap(*map.downcast<ImageMapUInt8>());

		PNGDecoder::write(bitmap, "saved.png");
	}*/


	try
	{
		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/basn0g08_badcrc.png");

		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/pino.png");
		PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/Fencing_Iron.png");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Test PNG files in PngSuite-2013jan13 (From http://www.schaik.com/pngsuite/)
	try
	{
		const std::string png_suite_dir = TestUtils::getTestReposDir() + "/testfiles/pngs/PngSuite-2013jan13";
		const std::vector<std::string> files = FileUtils::getFilesInDir(png_suite_dir);

		for(size_t i=0; i<files.size(); ++i)
		{
			const std::string path = png_suite_dir + "/" + files[i];
			if(::hasExtension(path, "png") && !::hasPrefix(files[i], "x"))
			{
				// conPrint("Processing '" + path + "'...");

				PNGDecoder::decode(path);
			}
		}

		// Test some particular files, see http://www.schaik.com/pngsuite/pngsuite_bas_png.html
		
		//==================== Test basic formats =====================
		// black & white
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g01.png");
			testAssert(map->numChannels() == 1);
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 2 bit (4 level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g02.png");
			testAssert(map->numChannels() == 1);
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 8 bit (256 level) grayscale 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g08.png");
			testAssert(map->numChannels() == 1);
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 16 bit (64k level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn0g16.png");
			testAssert(map->numChannels() == 1);
			testAssert(map->getBytesPerPixel() == 2);
		}

		// 3x8 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn2c08.png");
			testAssert(map->numChannels() == 3);
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 3x16 bits rgb color
		// NOTE: decodes to 4*16 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn2c16.png");
			testAssert(map->numChannels() == 3 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 6 || map->getBytesPerPixel() == 8);
		}

		// 1 bit (2 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn3p01.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit (256 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn3p08.png");
			testAssert(map->numChannels() == 3);
			testAssert(map->getBytesPerPixel() == 3);

			PNGDecoder::write(*map.downcast<ImageMapUInt8>(), "test.png");
		}

		// 8 bit grayscale + 8 bit alpha-channel
		// NOTE: decodes to 4*8 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn4a08.png");
			testAssert(map->numChannels() == 2 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 2 || map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 16 bit grayscale + 16 bit alpha-channel
		// NOTE: decodes to 4*16 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn4a16.png");
			testAssert(map->numChannels() == 2 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 4 || map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}

		// 3x8 bits rgb color + 8 bit alpha-channel 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn6a08.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x16 bits rgb color + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basn6a16.png");
			testAssert(map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}

		//==================== Test some interlaced files =====================
		// black & white
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g01.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 2 bit (4 level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g02.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 8 bit (256 level) grayscale 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g08.png");
			testAssert(map->getBytesPerPixel() == 1);
		}

		// 16 bit (64k level) grayscale
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi0g16.png");
		//WUFFS	testAssert(map->numChannels() == 1);
		//WUFFS	testAssert(map->getBytesPerPixel() == 2);
		}

		// 3x8 bits rgb color
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi2c08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 3x16 bits rgb color
		// NOTE: decodes to 4*16 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi2c16.png");
			testAssert(map->numChannels() == 3 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 6 || map->getBytesPerPixel() == 8);
}

		// 1 bit (2 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi3p01.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit (256 color) paletted - will get converted to RGB
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi3p08.png");
			testAssert(map->getBytesPerPixel() == 3);
		}

		// 8 bit grayscale + 8 bit alpha-channel
		// NOTE: decodes to 4*8 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi4a08.png");
			testAssert(map->numChannels() == 2 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 2 || map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 16 bit grayscale + 16 bit alpha-channel
		// NOTE: decodes to 4*16 bit channels with Wuffs
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi4a16.png");
			testAssert(map->numChannels() == 2 || map->numChannels() == 4);
			testAssert(map->getBytesPerPixel() == 4 || map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}

		// 3x8 bits rgb color + 8 bit alpha-channel 
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi6a08.png");
			testAssert(map->getBytesPerPixel() == 4);
			testAssert(map->hasAlphaChannel());
		}

		// 3x16 bits rgb color + 16 bit alpha-channel
		{
			Map2DRef map = PNGDecoder::decode(png_suite_dir + "/basi6a16.png");
			testAssert(map->getBytesPerPixel() == 8);
			testAssert(map->hasAlphaChannel());
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		failTest(e.what());
	}



	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/NO_SUCH_FILE.png");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a truncated file
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/pngs/truncated.png");

		failTest("Shouldn't get here.");
	}
	catch (ImFormatExcep& )
	{
		//testAssert(StringUtils::containsString(e.what(), "Read")); // "Read past end of buffer"
	}

	// Try with a malformed file
	try
	{
		decode(TestUtils::getTestReposDir() + "/testfiles/pngs/malformed.png");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep& )
	{
		//testAssert(StringUtils::containsString(e.what(), "IDAT")); // "IDAT: invalid distance too far back"
	}


	// Try writing a bitmap object to a PNG file
	try
	{
		Bitmap bitmap(20, 10, 3, NULL);
		bitmap.zero();

		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test.png";
		write(bitmap, path);

		// Now read it to make sure it's a valid PNG
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == 20);
		testAssert(im->getMapHeight() == 10);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try writing an ImageMap object to a PNG file
	try
	{
		ImageMapUInt8 imagemap(20, 10, 3);
		imagemap.set(125);

		const std::string path = PlatformUtils::getTempDirPath() + "/indigo_png_write_test2.png";
		write(imagemap, path);

		// Now read it to make sure it's a valid PNG
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == 20);
		testAssert(im->getMapHeight() == 10);
		testAssert(im->getBytesPerPixel() == 3);
		testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer()) != NULL);
		testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer())->getPixel(5, 5)[0] == 125);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}



	// Try testing write failure - write to an invalid location
	try
	{
		ImageMapUInt8 imagemap(20, 10, 3);
		imagemap.set(125);

#if defined(_WIN32)
		const std::string path = "abc:/def.png";
#else
		const std::string path = "/abc/def.png";
#endif
		write(imagemap, path);

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}


	// Performance test
	if(false)
	{
		const int num_iters = 10;
		double min_time = 1.0e10;
		for(int z=0; z<num_iters; ++z)
		{
			Timer timer;
			const std::string path = TestUtils::getTestReposDir() + "/testscenes/zomb_dark_sphere_bug_scene/PACKED_0_PACKED_0_leather_white3.png";
			Reference<Map2D> im = decode(path);
			testAssert(im->getMapWidth() == 2189);
			min_time = myMin(min_time, timer.elapsed());
		}
		
		conPrint("Time to load PACKED_0_PACKED_0_leather_white3.png: " + doubleToStringNSigFigs(min_time, 4) + " s.");
	}


	//----------- Perf test, Wuffs vs LibPNG ------------
	if(true)
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/antialias_test3.png";
		//const std::string path = TestUtils::getTestReposDir() + "/testfiles/pngs/Excited.png";
		MemMappedFile file(path);

#if LIBPNG_SUPPORT
		{
			double min_time = 1.0e10;
			for(int i=0; i<16; ++i)
			{
				Timer timer;
				BufferViewInStream buffer_view_in_stream(ArrayRef<uint8>((const uint8*)file.fileData(), file.fileSize()));
				Reference<Map2D> map = doDecodeFromBufferLibPNG(buffer_view_in_stream);
				//testAssert(map->getMapWidth() == 1000);
				min_time = myMin(min_time, timer.elapsed());

			}
			conPrint("doDecodeFromBufferLibPNG took    " + doubleToStringNSigFigs(min_time * 1000, 5) + " ms");
		}
#endif
#if WUFFS_SUPPORT
		{
			double min_time = 1.0e10;
			for(int i=0; i<16; ++i)
			{
				Timer timer;
				BufferViewInStream buffer_view_in_stream(ArrayRef<uint8>((const uint8*)file.fileData(), file.fileSize()));
				Reference<Map2D> map = doDecodeFromBufferWithWuffs(buffer_view_in_stream, NULL);
				//testAssert(map->getMapWidth() == 1000);
				min_time = myMin(min_time, timer.elapsed());
			}
			conPrint("doDecodeFromBufferWithWuffs took " + doubleToStringNSigFigs(min_time * 1000, 5) + " ms");
		}
#endif
	}

#endif // end if !EMSCRIPTEN


	conPrint("PNGDecoder::test() done.");
}


#endif // BUILD_TESTS
