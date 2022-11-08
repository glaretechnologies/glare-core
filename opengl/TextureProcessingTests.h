/*=====================================================================
TextureProcessingTests.h
------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <string>
namespace glare { class TaskManager; }
namespace glare { class GeneralMemAllocator; }

class TextureProcessingTests
{
public:
	static void test();

private:
	static void testDownSamplingGreyTexture(unsigned int W, unsigned int H, unsigned int N);
	static void testBuildingTexDataForImage(glare::GeneralMemAllocator* allocator, unsigned int W, unsigned int H, unsigned int N);
	static void testLoadingAnimatedFile(const std::string& path, glare::TaskManager& task_manager);
};
