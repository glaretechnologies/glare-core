/*=====================================================================
TextureLoadingTests.cpp
-----------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "TextureLoadingTests.h"


#if BUILD_TESTS


#include "TextureLoading.h"
#include "../graphics/ImageMap.h"
#include "../maths/mathstypes.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/jpegdecoder.h"
#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"


// Generate mipmaps for grey texture, check mipmaps are still same grey value.
void TextureLoadingTests::testDownSamplingGreyTexture(unsigned int W, unsigned int H, unsigned int N)
{
	ImageMapUInt8Ref map = new ImageMapUInt8(W, H, N);
	map->set(128);

	Reference<const ImageMapUInt8> prev_mip_level_image = map;
	for(int k=1; ; ++k) // For each mipmap level:
	{
		const unsigned int level_W = (int)myMax(1u, W / (1 << k));
		const unsigned int level_H = (int)myMax(1u, H / (1 << k));

		Reference<ImageMapUInt8> mip_level_image = new ImageMapUInt8(level_W, level_H, N);
		TextureLoading::downSampleToNextMipMapLevel(prev_mip_level_image->getWidth(), prev_mip_level_image->getHeight(), prev_mip_level_image->getN(), prev_mip_level_image->getData(),
			level_W, level_H, mip_level_image->getData());

		for(size_t i=0; i<mip_level_image->numPixels(); ++i)
			for(unsigned int c=0; c<N; ++c)
				testAssert(mip_level_image->getPixel(i)[c] == 128);

		prev_mip_level_image = mip_level_image;

		if(level_W == 1 && level_H == 1)
			break;
	}
}


void TextureLoadingTests::test()
{
	conPrint("TextureLoading::test()");

	TextureLoading::init();

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

	if(false)
	{
		Map2DRef map = JPEGDecoder::decode(".", TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
		testAssert(dynamic_cast<ImageMapUInt8*>(map.ptr()));

		Reference<const ImageMapUInt8> prev_mip_level_image = map.downcast<const ImageMapUInt8>();
		const size_t W = map->getMapWidth();
		const size_t H = map->getMapHeight();
		for(size_t k=1; ; ++k) // For each mipmap level:
		{
			const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
			const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));

			Reference<ImageMapUInt8> mip_level_image = new ImageMapUInt8(level_W, level_H, prev_mip_level_image->getN());
			TextureLoading::downSampleToNextMipMapLevel(prev_mip_level_image->getWidth(), prev_mip_level_image->getHeight(), prev_mip_level_image->getN(), prev_mip_level_image->getData(),
				level_W, level_H, mip_level_image->getData());


			PNGDecoder::write(*mip_level_image, "mipmap_level_" + toString(k) + ".png");

			prev_mip_level_image = mip_level_image;

			if(level_W == 1 && level_H == 1)
				break;
		}
	}

	conPrint("TextureLoading::test() done.");
}


#endif // BUILD_TESTS
