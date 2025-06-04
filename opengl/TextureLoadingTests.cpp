/*=====================================================================
TextureLoadingTests.cpp
-----------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TextureLoadingTests.h"


#if BUILD_TESTS


#include "TextureLoading.h"
#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
#include "../graphics/TextureProcessing.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/jpegdecoder.h"
#include "../graphics/GifDecoder.h"
#include "../graphics/DXTCompression.h"
#include "../maths/mathstypes.h"
#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"



void TextureLoadingTests::test(const Reference<OpenGLEngine>& opengl_engine)
{
	conPrint("TextureLoadingTests::test()");

	{
		ImageMapUInt8Ref map = new ImageMapUInt8(16, 16, 3);
		map->set(240);

		Reference<TextureData> texture_data = TextureProcessing::buildTextureData(map.ptr(), /*mem allocator=*/NULL, /*task manager=*/NULL, /*allow compression=*/false, /*build mipmaps=*/false, /*convert_float_to_half=*/true);

		TextureParams texture_params;
		OpenGLTextureRef tex = TextureLoading::createUninitialisedOpenGLTexture(*texture_data, opengl_engine, texture_params);
	}

	conPrint("TextureLoadingTests::test() done.");
}


#endif // BUILD_TESTS
