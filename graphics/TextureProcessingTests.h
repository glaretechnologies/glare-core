/*=====================================================================
TextureProcessingTests.h
------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <string>
namespace glare { class TaskManager; }
namespace glare { class Allocator; }

class TextureProcessingTests
{
public:
	static void test();

private:
	static void testDownSamplingGreyTexture(unsigned int W, unsigned int H, unsigned int N);
	static void testBuildingTexDataForImage(glare::Allocator* allocator, unsigned int W, unsigned int H, unsigned int N);
	static void testLoadingAnimatedFile(const std::string& path, glare::Allocator* allocator, glare::TaskManager& task_manager);
};
