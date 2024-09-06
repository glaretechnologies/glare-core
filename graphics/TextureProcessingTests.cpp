/*=====================================================================
TextureProcessingTests.cpp
--------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "TextureProcessingTests.h"


#if BUILD_TESTS


#include "PNGDecoder.h"
#include "jpegdecoder.h"
#include "GifDecoder.h"
#include "DXTCompression.h"
#include "TextureProcessing.h"
#include "ImageMap.h"
#include "../maths/mathstypes.h"
#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"
#include "../utils/GeneralMemAllocator.h"


// Generate mipmaps for grey texture, check mipmaps are still same grey value.
void TextureProcessingTests::testDownSamplingGreyTexture(unsigned int W, unsigned int H, unsigned int N)
{
	ImageMapUInt8Ref map = new ImageMapUInt8(W, H, N);
	map->set(128);

	Reference<const ImageMapUInt8> prev_mip_level_image = map;
	for(int k=1; ; ++k) // For each mipmap level:
	{
		const unsigned int level_W = (int)myMax(1u, W / (1 << k));
		const unsigned int level_H = (int)myMax(1u, H / (1 << k));

		Reference<ImageMapUInt8> mip_level_image = new ImageMapUInt8(level_W, level_H, N);
		float alpha_coverage;
		TextureProcessing::downSampleToNextMipMapLevel(prev_mip_level_image->getWidth(), prev_mip_level_image->getHeight(), prev_mip_level_image->getN(), prev_mip_level_image->getData(),
			/*alpha scale=*/1.f, level_W, level_H, mip_level_image->getData(), alpha_coverage);

		for(size_t i=0; i<mip_level_image->numPixels(); ++i)
			for(unsigned int c=0; c<N; ++c)
				testAssert(mip_level_image->getPixel(i)[c] == 128);

		prev_mip_level_image = mip_level_image;

		if(level_W == 1 && level_H == 1)
			break;
	}
}


void TextureProcessingTests::testBuildingTexDataForImage(glare::Allocator* allocator, unsigned int W, unsigned int H, unsigned int N)
{
	for(int i=0; i<2; ++i)
	{
		const bool allow_compression = i > 0;

		const bool result_should_be_compressed = allow_compression && (W > 1) && (H > 1);

		ImageMapUInt8Ref map = new ImageMapUInt8(W, H, N);
		map->set(128);
		Reference<TextureData> tex_data = TextureProcessing::buildUInt8MapTextureData(map.getPointer(), allocator, /*task manager=*/NULL, /*allow compression=*/allow_compression, /*build mipmaps=*/true);

		// Check MIP level offsets are valid
		for(size_t k=0; k<tex_data->level_offsets.size(); ++k)
		{
			const size_t offset = tex_data->level_offsets[k].offset;

			// Check offset is aligned.  Note: I'm not sure what this alignment needs to be, if anything
			testAssert(offset % 8 == 0);

			// Compute the size of the compressed data for this level
			const size_t level_W = myMax((size_t)1, tex_data->W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, tex_data->H / ((size_t)1 << k));
			const size_t level_size = result_should_be_compressed ? DXTCompression::getCompressedSizeBytes(level_W, level_H, N) : (level_W * level_H * N);

			testAssert(tex_data->level_offsets[k].level_size == level_size);

			// Make sure the next offset is sufficiently far away.
			size_t next_offset;
			if(k + 1 < tex_data->level_offsets.size())
				next_offset = tex_data->level_offsets[k + 1].offset;
			else
				next_offset = tex_data->frames[0].mipmap_data.size();

			testAssert(offset + level_size <= next_offset);
			testAssert(offset + level_size <= tex_data->frames[0].mipmap_data.size());
		}
	}
}

#if 0
static void testLoadingFile(const std::string& path, glare::TaskManager& task_manager)
{
	try
	{
		Reference<Map2D> mip_level_image = PNGDecoder::decode(path);

		testAssert(mip_level_image.isType<ImageMapUInt8>());

		const ImageMapUInt8* map_uint8 = mip_level_image.downcastToPtr<ImageMapUInt8>();


		Reference<TextureData> texdata = TextureLoading::buildUInt8MapTextureData(map_uint8, &task_manager, /*allow_compression=*/true);

		testAssert(texdata->W == map_uint8->getMapWidth());
		testAssert(texdata->H == map_uint8->getMapHeight());
		testAssert(texdata->bytes_pp == map_uint8->getBytesPerPixel());

		// Check MIP level offsets are valid
		for(size_t k=0; k<texdata->level_offsets.size(); ++k)
		{
			const size_t offset = texdata->level_offsets[k];

			// Check offset is aligned.  Note: I'm not sure what this alignment needs to be, if anything
			testAssert(offset % 8 == 0);

			const size_t level_W = myMax((size_t)1, texdata->W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, texdata->H / ((size_t)1 << k));
			const size_t level_compressed_size = DXTCompression::getCompressedSizeBytes(level_W, level_H, texdata->bytes_pp);

			testAssert(offset < texdata->frames[0].compressed_data.size());
			testAssert(offset + level_compressed_size <= texdata->frames[0].compressed_data.size());
		}


		/*testAssert(texdata->frames.size() == seq->images.size());
		for(size_t i=0; i<texdata->frames.size(); ++i)
		{
			testAssert(texdata->frames[i].compressed_data.size() > 0);
		}*/
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}
#endif


void TextureProcessingTests::testLoadingAnimatedFile(const std::string& path, glare::Allocator* allocator, glare::TaskManager& task_manager)
{
	for(int z=0; z<2; ++z)
	{
		const bool allow_compression = z > 0;
		try
		{
			Reference<Map2D> mip_level_image = GIFDecoder::decodeImageSequence(path);

			testAssert(mip_level_image.isType<ImageMapSequenceUInt8>());

			const ImageMapSequenceUInt8* seq = mip_level_image.downcastToPtr<ImageMapSequenceUInt8>();

			testAssert(seq->images.size() > 1);
			//for(size_t i=0; i<seq->images.size(); ++i)
			//{
			//	testAssert(seq->images[i]->getMapWidth() == 30);
			//	testAssert(seq->images[i]->getMapHeight() == 60);
			//	testAssert(seq->images[i]->getBytesPerPixel() == 3);
			//}

			Reference<TextureData> texdata = TextureProcessing::buildUInt8MapSequenceTextureData(seq, allocator, &task_manager, allow_compression, /*build_mipmaps=*/true);

			testAssert(texdata->W == seq->images[0]->getMapWidth());
			testAssert(texdata->H == seq->images[0]->getMapHeight());

			testAssert(texdata->frames.size() == seq->images.size());
			for(size_t i=0; i<texdata->frames.size(); ++i)
			{
				testAssert(texdata->frames[i].mipmap_data.size() > 0);
			}
		}
		catch(glare::Exception& e)
		{
			failTest(e.what());
		}
	}
}


void TextureProcessingTests::test()
{
	conPrint("TextureLoading::test()");

	glare::TaskManager task_manager;

	Reference<glare::GeneralMemAllocator> allocator = new glare::GeneralMemAllocator(/*arena_size_B=*/10000000);

	//testLoadingFile("resources/imposters/elm_imposters.png", task_manager);


	testBuildingTexDataForImage(allocator.ptr(), 256, 256, 3);
	testBuildingTexDataForImage(allocator.ptr(), 250, 250, 3);
	testBuildingTexDataForImage(allocator.ptr(), 250, 7, 3);
	testBuildingTexDataForImage(allocator.ptr(), 7, 250, 3);
	testBuildingTexDataForImage(allocator.ptr(), 2, 2, 3);
	testBuildingTexDataForImage(allocator.ptr(), 1, 1, 3);
	try
	{
		testBuildingTexDataForImage(allocator.ptr(), 0, 0, 3);
		failTest("Expected excep");
	}
	catch(glare::Exception&)
	{}

	testBuildingTexDataForImage(allocator.ptr(), 256, 256, 4);
	testBuildingTexDataForImage(allocator.ptr(), 250, 250, 4);
	testBuildingTexDataForImage(allocator.ptr(), 250, 7, 3);
	testBuildingTexDataForImage(allocator.ptr(), 7, 250, 4);
	testBuildingTexDataForImage(allocator.ptr(), 2, 2, 4);
	testBuildingTexDataForImage(allocator.ptr(), 1, 1, 4);


	// Test with 1 and 2 components per pixel
	testBuildingTexDataForImage(allocator.ptr(), 256, 256, /*num components=*/1);
	testBuildingTexDataForImage(allocator.ptr(), 256, 256, /*num components=*/2);




	testDownSamplingGreyTexture(256, 256, 3);
	testDownSamplingGreyTexture(250, 250, 3);
	testDownSamplingGreyTexture(250, 7, 3);
	testDownSamplingGreyTexture(7, 250, 3);
	testDownSamplingGreyTexture(2, 2, 3);

	testDownSamplingGreyTexture(256, 256, 4);
	testDownSamplingGreyTexture(250, 250, 4);
	testDownSamplingGreyTexture(250, 7, 3);
	testDownSamplingGreyTexture(7, 250, 4);
	testDownSamplingGreyTexture(2, 2, 4);

	
#if !defined(EMSCRIPTEN)
	// Test loading animated gifs
	testLoadingAnimatedFile(TestUtils::getTestReposDir() + "/testfiles/gifs/fire.gif", allocator.ptr(), task_manager);
	testLoadingAnimatedFile(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47ppTMXv7gqwCDm_47giphy.gif", allocator.ptr(), task_manager);
	testLoadingAnimatedFile(TestUtils::getTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47X93e1eC2J2hjy_47giphy.gif", allocator.ptr(), task_manager);

	// Perf test:
	{
		//Reference<Map2D> mip_level_image = JPEGDecoder::decode(".", "C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\GLB_image_7509840974915305048_jpg_7509840974915305048.jpg");
		Reference<Map2D> mip_level_image = JPEGDecoder::decode(".", TestUtils::getTestReposDir() + "/testfiles/textures/parquet-diffuse.jpg"); // 2000x2000 px image.

		testAssert(mip_level_image.isType<ImageMapUInt8>());
	
		const ImageMapUInt8* map_imagemapuint8 = mip_level_image.downcastToPtr<ImageMapUInt8>();

		Timer timer;
		Reference<TextureData> tex_data = TextureProcessing::buildUInt8MapTextureData(map_imagemapuint8, allocator.ptr(), /*task manager=*/NULL, /*allow compression=*/true, /*build mipmaps=*/true);

		conPrint("single-threaded buildUInt8MapTextureData() for 'parquet-diffuse.jpg' took " + timer.elapsedStringMSWIthNSigFigs(4));

		timer.reset();

		//for(int i=0; i<1000; ++i)
			tex_data = TextureProcessing::buildUInt8MapTextureData(map_imagemapuint8, allocator.ptr(), &task_manager, /*allow compression=*/true, /*build mipmaps=*/true);

		conPrint("multi-threaded buildUInt8MapTextureData() for 'parquet-diffuse.jpg' took " + timer.elapsedStringMSWIthNSigFigs(4));
	}

	if(false)
	{
		Map2DRef map = JPEGDecoder::decode(".", TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
		testAssert(dynamic_cast<ImageMapUInt8*>(map.ptr()));

		Reference<const ImageMapUInt8> prev_mip_level_image = map.downcast<const ImageMapUInt8>();
		const size_t W = map->getMapWidth();
		const size_t H = map->getMapHeight();
		for(size_t k=1; ; ++k) // For each mipmap level:
		{
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

			Reference<ImageMapUInt8> mip_level_image = new ImageMapUInt8(level_W, level_H, prev_mip_level_image->getN());
			float alpha_coverage;
			TextureProcessing::downSampleToNextMipMapLevel(prev_mip_level_image->getWidth(), prev_mip_level_image->getHeight(), prev_mip_level_image->getN(), prev_mip_level_image->getData(),
				/*alpha scale=*/1.f, level_W, level_H, mip_level_image->getData(), alpha_coverage);


			PNGDecoder::write(*mip_level_image, "mipmap_level_" + toString(k) + ".png");

			prev_mip_level_image = mip_level_image;

			if(level_W == 1 && level_H == 1)
				break;
		}
	}
#endif // end if !defined(EMSCRIPTEN)

	conPrint("TextureLoading::test() done.");
}


#endif // BUILD_TESTS
