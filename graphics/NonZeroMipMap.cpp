/*=====================================================================
NonZeroMipMap.cpp
-----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "NonZeroMipMap.h"


#include <utils/TestUtils.h>
#include <utils/ConPrint.h>


void NonZeroMipMap::test()
{
	conPrint("NonZeroMipMap::test()");

	{
		ImageMapUInt8 map(64, 64, 1);
		map.zero();

		map.getPixel(0, 0)[0] = 1;

		NonZeroMipMap non_zero_mipmap;
		non_zero_mipmap.build(map, /*channel=*/0);

		// layer widths:                    64, 32, 16, 8,  4,  2,  1
		// image pixels per mipmap pixel:    1,  2,  4, 8, 16, 32, 64
		for(int i=0; i<7; ++i)
		{
			const int W = 64 >> i;
			for(int y=0; y<W; ++y)
			for(int x=0; x<W; ++x)
			{
				const bool expected_non_zero = x == 0 && y == 0;
				testEqual(non_zero_mipmap.isSectionNonZero(i, x, y), expected_non_zero);
			}
		}

		testAssert(non_zero_mipmap.isRegionNonZero(/*norm start x=*/0, /*norm start y=*/0, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.25f, /*norm start y=*/0.25f, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.5f, /*norm start y=*/0.25f, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.75f, /*norm start y=*/0.25f, 0.25f, 0.25f));
	}

	// Test with ImageMapUInt1
	{
		ImageMapUInt1 map(64, 64);
		map.zero();

		map.setPixelValue(0, 0, 1);

		NonZeroMipMap non_zero_mipmap;
		non_zero_mipmap.build(map, /*channel=*/0);

		// layer widths:                    64, 32, 16, 8,  4,  2,  1
		// image pixels per mipmap pixel:    1,  2,  4, 8, 16, 32, 64
		for(int i=0; i<7; ++i)
		{
			const int W = 64 >> i;
			for(int y=0; y<W; ++y)
			for(int x=0; x<W; ++x)
			{
				const bool expected_non_zero = x == 0 && y == 0;
				testEqual(non_zero_mipmap.isSectionNonZero(i, x, y), expected_non_zero);
			}
		}

		testAssert(non_zero_mipmap.isRegionNonZero(/*norm start x=*/0, /*norm start y=*/0, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.25f, /*norm start y=*/0.25f, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.5f, /*norm start y=*/0.25f, 0.25f, 0.25f));
		testAssert(!non_zero_mipmap.isRegionNonZero(/*norm start x=*/0.75f, /*norm start y=*/0.25f, 0.25f, 0.25f));
	}


	{
		ImageMapUInt8 map(64, 64, 1);
		map.zero();

		map.getPixel(63, 63)[0] = 1;

		NonZeroMipMap non_zero_mipmap;
		non_zero_mipmap.build(map, /*channel=*/0);

		// layer widths: 64, 32, 16, 8, 4, 2, 1
		for(int i=0; i<7; ++i)
		{
			const int W = 64 >> i;
			for(int y=0; y<W; ++y)
			for(int x=0; x<W; ++x)
			{
				const bool expected_non_zero = x == W-1 && y == W-1;
				testEqual(non_zero_mipmap.isSectionNonZero(i, x, y), expected_non_zero);
			}
		}
	}

	// Test with ImageMapUInt1
	{
		ImageMapUInt1 map(64, 64);
		map.zero();

		map.setPixelValue(63, 63, 1);

		NonZeroMipMap non_zero_mipmap;
		non_zero_mipmap.build(map, /*channel=*/0);

		// layer widths: 64, 32, 16, 8, 4, 2, 1
		for(int i=0; i<7; ++i)
		{
			const int W = 64 >> i;
			for(int y=0; y<W; ++y)
			for(int x=0; x<W; ++x)
			{
				const bool expected_non_zero = x == W-1 && y == W-1;
				testEqual(non_zero_mipmap.isSectionNonZero(i, x, y), expected_non_zero);
			}
		}
	}

	conPrint("NonZeroMipMap::test() done.");
}
