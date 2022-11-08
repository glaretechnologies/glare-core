/*=====================================================================
TextureLoadingTests.cpp
-----------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "TextureLoadingTests.h"


#if BUILD_TESTS


#include "TextureLoading.h"
#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
#include "../maths/mathstypes.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/jpegdecoder.h"
#include "../graphics/GifDecoder.h"
#include "../graphics/DXTCompression.h"
#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"



void TextureLoadingTests::test()
{
	conPrint("TextureLoading::test()");

	conPrint("TextureLoading::test() done.");
}


#endif // BUILD_TESTS
