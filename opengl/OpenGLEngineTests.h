/*=====================================================================
OpenGLEngineTests.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <string>


class OpenGLEngine;


namespace OpenGLEngineTests
{
	void test(const std::string& indigo_base_dir);

	void doTextureLoadingTests(OpenGLEngine& engine);

	void buildData();
}
