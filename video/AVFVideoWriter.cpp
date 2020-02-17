/*=====================================================================
AVFVideoWriter.cpp
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "AVFVideoWriter.h"


#ifdef OSX


#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"



AVFVideoWriter::AVFVideoWriter(const std::string& URL, const VidParams& vid_params_)
:	frame_index(0)
{
}


AVFVideoWriter::~AVFVideoWriter()
{
}


void AVFVideoWriter::writeFrame(const uint8* source_data, size_t source_row_stride_B)
{
	frame_index++;
}


void AVFVideoWriter::finalise()
{
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"


void WMFVideoWriter::test()
{
	
}


#endif // BUILD_TESTS


#endif // OSX
