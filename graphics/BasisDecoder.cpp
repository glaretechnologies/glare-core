/*=====================================================================
BasisDecoder.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "BasisDecoder.h"


#include "imformatdecoder.h"
#include "CompressedImage.h"
#include "IncludeOpenGL.h"
#include "../utils/FileInStream.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Vector.h"
#include "../utils/PlatformUtils.h"
#include "../utils/BufferViewInStream.h"
#include "../utils/ArrayRef.h"
#include "../maths/CheckedMaths.h"
#include <transcoder/basisu_transcoder.h>


void BasisDecoder::init()
{
	basist::basisu_transcoder_init();
}


// throws ImFormatExcep on failure
Reference<Map2D> BasisDecoder::decode(const std::string& path, glare::Allocator* mem_allocator, const BasisDecoderOptions& options)
{
	try
	{
		MemMappedFile file(path);
		return decodeFromBuffer(file.fileData(), file.fileSize(), mem_allocator, options);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> BasisDecoder::decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator, const BasisDecoderOptions& options)
{
	try
	{
		BufferViewInStream file(ArrayRef<uint8>((const uint8*)data, size));

		basist::basisu_transcoder transcoder;

		if(!transcoder.validate_header(data, (uint32)size))
			throw glare::Exception("validate_header failed");

		basist::basisu_file_info file_info;
		transcoder.get_file_info(data, (uint32)size, file_info);

		const uint32 num_images = transcoder.get_total_images(data, (uint32)size);
		checkProperty(num_images > 0, "require num_images > 0");

		const basist::basis_texture_type texture_type = transcoder.get_texture_type(data, (uint32)size);
		if(!(texture_type == basist::cBASISTexType2D || texture_type == basist::cBASISTexType2DArray || texture_type == basist::cBASISTexTypeVideoFrames))
			throw glare::Exception("invalid/unsupported texture type");

		checkProperty(transcoder.get_basis_tex_format(data, (uint32)size) == basist::basis_tex_format::cETC1S, "tex format must be cETC1S");

		basist::basisu_image_info image_0_info;
		transcoder.get_image_info(data, (uint32)size, image_0_info, /*image index=*/0);

		const uint32 num_mip_levels = transcoder.get_total_image_levels(data, (uint32)size, /*image index=*/0);
		const size_t expected_num_mip_levels = TextureData::computeNumMipLevels(image_0_info.m_orig_width, image_0_info.m_orig_height);
		if(num_mip_levels > expected_num_mip_levels)
			throw glare::Exception("Invalid number of mip levels: " + toString(num_mip_levels) + ", max num: " + toString(expected_num_mip_levels));

		if(image_0_info.m_orig_width > 1000000)
			throw ImFormatExcep("Invalid width: " + toString(image_0_info.m_orig_width));
		if(image_0_info.m_orig_height > 1000000)
			throw ImFormatExcep("Invalid height: " + toString(image_0_info.m_orig_height));

		const size_t max_num_pixels = 1 << 27;
		if(((size_t)image_0_info.m_orig_width * (size_t)image_0_info.m_orig_height * (size_t)num_images) > max_num_pixels)
			throw ImFormatExcep("invalid width, height, num_images: (too many pixels): " + toString(image_0_info.m_orig_width) + ", " + toString(image_0_info.m_orig_height));

		OpenGLTextureFormat format;
		basist::transcoder_texture_format dest_basis_format;
		if(!image_0_info.m_alpha_flag) // If no alpha:
		{
			if(options.ETC_support)
			{
				format = OpenGLTextureFormat::Format_Compressed_ETC2_SRGB_Uint8;
				dest_basis_format = basist::transcoder_texture_format::cTFETC1_RGB;
			}
			else
			{
				format = OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8;
				dest_basis_format = basist::transcoder_texture_format::cTFBC1_RGB; // BC1 a.k.a. DXT1.
			}
		}
		else // else if alpha:
		{
			if(options.ETC_support)
			{
				format = OpenGLTextureFormat::Format_Compressed_ETC2_SRGBA_Uint8;
				dest_basis_format = basist::transcoder_texture_format::cTFETC2_RGBA;
			}
			else
			{
				format = OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8;
				dest_basis_format = basist::transcoder_texture_format::cTFBC3_RGBA; // BC3 a.k.a  DXT5
			}
		}

		const bool multi_frame = texture_type == basist::cBASISTexTypeVideoFrames; // Is this transcoded from an animated GIF?

		CompressedImageRef image = new CompressedImage(image_0_info.m_orig_width, image_0_info.m_orig_height, format);
		image->texture_data->frames.resize(multi_frame ? num_images : 1);
		image->texture_data->W = image_0_info.m_orig_width;
		image->texture_data->H = image_0_info.m_orig_height;
		image->texture_data->D = multi_frame ? 1 : num_images;
		image->texture_data->num_array_images = (texture_type == basist::cBASISTexType2DArray) ? num_images : 0;

		if(multi_frame)
		{
			const double frame_time_s = file_info.m_us_per_frame * 1.0e-6;
			image->texture_data->frame_durations_equal = true;
			image->texture_data->recip_frame_duration = 1.0 / frame_time_s;
			image->texture_data->last_frame_end_time = frame_time_s * num_images;
			image->texture_data->num_frames = num_images;
		}

		const size_t bytes_per_block = bytesPerBlock(image->texture_data->format);

		// Build level_offsets (byte offset of each MIP level in mipmap_data)
		image->texture_data->level_offsets.resize(num_mip_levels);
		size_t offset = 0;
		for(uint32 lvl=0; lvl<num_mip_levels; ++lvl)
		{
			basist::basisu_image_level_info level_info;
			transcoder.get_image_level_info(data, (uint32)size, level_info, /*image index=*/0, lvl);

			const size_t expected_num_blocks = TextureData::computeNum4PixelBlocksForLevel(image_0_info.m_orig_width, image_0_info.m_orig_height, lvl);
			if(level_info.m_total_blocks != expected_num_blocks)
				throw glare::Exception("Unexpected level_info.m_total_blocks.");

			const size_t single_im_level_size_B = level_info.m_total_blocks * bytes_per_block; // For a single image
			const size_t total_level_size_B = single_im_level_size_B * (multi_frame ? 1 : num_images);

			image->texture_data->level_offsets[lvl].offset = offset;
			image->texture_data->level_offsets[lvl].level_size = total_level_size_B;

			offset += total_level_size_B;
		}

		const size_t frame_total_size_B = offset; // Sum over all Mip levels.

		// Allocate image data
		for(size_t i=0; i<image->texture_data->frames.size(); ++i)
			image->texture_data->frames[i].mipmap_data.resize(frame_total_size_B);

		for(uint32 im = 0; im < num_images; ++im)
		{
			basist::basisu_image_info image_info;
			transcoder.get_image_info(data, (uint32)size, image_info, /*image index=*/im);

			checkProperty(image_info.m_width  == image_0_info.m_width,  "all images must have same dimensions");
			checkProperty(image_info.m_height == image_0_info.m_height, "all images must have same dimensions");
		}


		transcoder.start_transcoding(data, (uint32)size);


		/*
		mipmap_data layout, for an array image:

		Mip level 0                                               Mip level 1                              Mip level 2
		----------------------------------------------------------------------------------------------------
		|im 0         |  im 1        |  im 2        |   im 3      |im 0    |  im 1   |  im 2  |   im 3  |  ...
		----------------------------------------------------------------------------------------------------
		^<------------------------------------------------------->^
		|                     level_size[0]                       |
		offset[0]                                             offset[1]



		mipmap_data layout, for a multi-frame image (e.g. animated gif)

		-----------------------------------------------------------------------------------------    -----------------------------------------------------------------------------------------
		|im 0 mip 0                         |     im 0 mip 1          | im 0 mip 2 |  ...      |    |im 1 mip 0                         |      im 1 mip 1           | im 1 mip 2 |  ...      |   ...
		-----------------------------------------------------------------------------------------    -----------------------------------------------------------------------------------------
		^<--------------------------------->^
		|       level_size[0]               |       level_size[1]     |
		offset[0]                        offset[1]                offset[2]
		*/
		for(uint32 lvl=0; lvl<num_mip_levels; ++lvl)
		{
			for(uint32 im = 0; im < num_images; ++im)
			{
				const size_t per_image_level_size_B = image->texture_data->level_offsets[lvl].level_size / (multi_frame ? 1 : num_images);
				const size_t dest_lvl_offset_B = image->texture_data->level_offsets[lvl].offset + (multi_frame ? 0 : (per_image_level_size_B * im));

				const size_t frame_index = multi_frame ? im : 0;
				runtimeCheck(frame_index < image->texture_data->frames.size());
				runtimeCheck(dest_lvl_offset_B <= image->texture_data->frames[frame_index].mipmap_data.size());
				const uint8* dest_output_blocks = image->texture_data->frames[frame_index].mipmap_data.data() + dest_lvl_offset_B; // Get destination/write pointer

				runtimeCheck((CheckedMaths::addUnsignedInts(dest_lvl_offset_B, per_image_level_size_B)) <= image->texture_data->frames[frame_index].mipmap_data.size());
				const uint32 output_num_blocks = (uint32)(per_image_level_size_B / bytes_per_block);

				if(!transcoder.transcode_image_level(data, (uint32)size,
					/*image_index=*/im, /*level index=*/lvl, (void*)dest_output_blocks, output_num_blocks,
					dest_basis_format // Target texture format.
				))
					throw glare::Exception("transcode_image_level failed.");
			}
		}

		return image;
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "jpegdecoder.h"
#include "PNGDecoder.h"
#include "TextureProcessing.h"
#include "DXTCompression.h"
#include <encoder/basisu_comp.h>

#if 0
// Command line:
// C:\fuzz_corpus\basis c:\code\glare-core\testfiles\basis

#ifndef BASISU_NO_HEADER_OR_DATA_CRC16_CHECKS
#error should define BASISU_NO_HEADER_OR_DATA_CRC16_CHECKS
#endif

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	BasisDecoder::init();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		BasisDecoder::BasisDecoderOptions options;
		options.ETC_support = false;
		BasisDecoder::decodeFromBuffer(data, size, /*mem_allocator=*/NULL, options);

		options.ETC_support = true;
		BasisDecoder::decodeFromBuffer(data, size, /*mem_allocator=*/NULL, options);
	}
	catch(glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif

#if !GUI_CLIENT
void writeBasisFile(const ImageMapUInt8& map, const std::string& dest_path)
{
	basisu::basisu_encoder_init(); // Can be called multiple times harmlessly.
	basisu::basis_compressor_params params;


	basisu::image img(map.getData(), (uint32)map.getWidth(), (uint32)map.getHeight(), (uint32)map.getN());

	params.m_source_images.push_back(img);

	Timer timer;

	params.m_write_output_basis_or_ktx2_files = true;
	params.m_out_filename = dest_path;

	params.m_mip_gen = true; // Generate mipmaps for each source image
	params.m_mip_srgb = true; // Convert image to linear before filtering, then back to sRGB

	params.m_etc1s_quality_level = 255;

	params.m_debug = false;

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

	conPrint("Basisu compression and writing of file took " + timer.elapsedStringNSigFigs(3));
}
#endif

void BasisDecoder::test()
{
	conPrint("BasisDecoder::test()");

	BasisDecoder::init();

	{
		Timer timer;
		basist::basisu_transcoder_init();
		conPrint("basisu_transcoder_init took " + timer.elapsedStringMSWIthNSigFigs(4));
	}

	try
	{
		// Write a basis image:
		/*{
			Timer timer;

			Reference<Map2D> im = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/Fencing_Iron.png");
			writeBasisFile(*im.downcastToPtr<ImageMapUInt8>(), "d:/tempfiles/Fencing_Iron.basis");

			conPrint("Writing Fencing_Iron.basis took " + timer.elapsedStringMSWIthNSigFigs(4));
		}*/
#if !GUI_CLIENT
		{
			Timer timer;

			Reference<Map2D> im = JPEGDecoder::decode(".", TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
			writeBasisFile(*im.downcastToPtr<ImageMapUInt8>(), "d:/files/italy_bolsena_flag_flowers_stairs_01.basis");

			conPrint("Writing italy_bolsena_flag_flowers_stairs_01.basis took " + timer.elapsedStringMSWIthNSigFigs(4));
		}
#endif

		//----------------------------------- Test time to load a JPG and do DXT compression for comparison -------------------------------------------
		/*{
			conPrint("----------------------");
			Timer timer;
			Reference<Map2D> im = JPEGDecoder::decode(".", TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
			conPrint("Reading italy_bolsena_flag_flowers_stairs_01.jpg took " + timer.elapsedStringMSWIthNSigFigs(4));

			// Measure TextureProcessing::buildTextureData for reference.
			timer.reset();
			TextureProcessing::buildTextureData(im.ptr(), NULL, NULL, true, true);
			conPrint("TextureProcessing::buildTextureData of italy_bolsena_flag_flowers_stairs_01.jpg took " + timer.elapsedStringMSWIthNSigFigs(4));
		}*/

		//----------------------------------- Test loading a texture without alpha -------------------------------------------
		{
			conPrint("----------------------");
			Timer timer;
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/italy_bolsena_flag_flowers_stairs_01.basis");
			conPrint("Reading italy_bolsena_flag_flowers_stairs_01.basis took " + timer.elapsedStringMSWIthNSigFigs(4));

			testAssert(im->getMapWidth() == 750);
			testAssert(im->getMapHeight() == 1152);
			testAssert(im->numChannels() == 3);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 11);
			testAssert(com_im->texture_data->D == 1);
			testAssert(com_im->texture_data->num_array_images == 0);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8);
		}
		// Load and transcode to ETC
		{
			conPrint("----------------------");
			Timer timer;
			BasisDecoder::BasisDecoderOptions options;
			options.ETC_support = true;
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/italy_bolsena_flag_flowers_stairs_01.basis", /*allocator=*/nullptr, options);
			conPrint("Reading italy_bolsena_flag_flowers_stairs_01.basis took " + timer.elapsedStringMSWIthNSigFigs(4));

			testAssert(im->getMapWidth() == 750);
			testAssert(im->getMapHeight() == 1152);
			testAssert(im->numChannels() == 3);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 11);
			testAssert(com_im->texture_data->D == 1);
			testAssert(com_im->texture_data->num_array_images == 0);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_ETC2_SRGB_Uint8);
		}
			
		//----------------------------------- Test loading a texture with alpha -------------------------------------------
		{
			conPrint("----------------------");
			Timer timer;
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/Fencing_Iron.basis");
			conPrint("Reading Fencing_Iron.basis took " + timer.elapsedStringMSWIthNSigFigs(4));

			testAssert(im->getMapWidth() == 256);
			testAssert(im->getMapHeight() == 286);
			testAssert(im->numChannels() == 4);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 9);
			testAssert(com_im->texture_data->D == 1);
			testAssert(com_im->texture_data->num_array_images == 0);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_DXT_SRGBA_Uint8);
		}
		// Load and transcode to ETC
		{
			conPrint("----------------------");
			Timer timer;
			BasisDecoder::BasisDecoderOptions options;
			options.ETC_support = true;
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/Fencing_Iron.basis", /*allocator=*/nullptr, options);
			conPrint("Reading Fencing_Iron.basis took " + timer.elapsedStringMSWIthNSigFigs(4));

			testAssert(im->getMapWidth() == 256);
			testAssert(im->getMapHeight() == 286);
			testAssert(im->numChannels() == 4);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 9);
			testAssert(com_im->texture_data->D == 1);
			testAssert(com_im->texture_data->num_array_images == 0);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_ETC2_SRGBA_Uint8);
		}

		//----------------------------------- Test loading an array texture -------------------------------------------
		{
			conPrint("----------------------");
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/chunk_basis_array_texture.basis");
			testAssert(im->getMapWidth() == 128);
			testAssert(im->getMapHeight() == 128);
			testAssert(im->numChannels() == 3);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 8);
			testAssert(com_im->texture_data->D == 6);
			testAssert(com_im->texture_data->num_array_images == 6);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_DXT_SRGB_Uint8);
		}
		// Load and transcode to ETC
		{
			conPrint("----------------------");
			BasisDecoder::BasisDecoderOptions options;
			options.ETC_support = true;
			Reference<Map2D> im = BasisDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/basis/chunk_basis_array_texture.basis", /*allocator=*/nullptr, options);
			testAssert(im->getMapWidth() == 128);
			testAssert(im->getMapHeight() == 128);
			testAssert(im->numChannels() == 3);
			testAssert(im.isType<CompressedImage>());
			CompressedImage* com_im = im.downcastToPtr<CompressedImage>();
			testAssert(com_im->texture_data->level_offsets.size() == 8);
			testAssert(com_im->texture_data->D == 6);
			testAssert(com_im->texture_data->num_array_images == 6);

			testAssert(com_im->texture_data->format == OpenGLTextureFormat::Format_Compressed_ETC2_SRGB_Uint8);
		}
		
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	conPrint("BasisDecoder::test() done.");
}


#endif // BUILD_TESTS
