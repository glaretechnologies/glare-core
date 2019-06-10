/*=====================================================================
TextureLoadingTests.h
---------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


class TextureLoadingTests
{
public:
	static void test();

private:
	static void testDownSamplingGreyTexture(unsigned int W, unsigned int H, unsigned int N);
};
