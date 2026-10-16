/*=====================================================================
VideoWriter.cpp
---------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:58:43 +1300
=====================================================================*/
#include "VideoWriter.h"


#include "../utils/Exception.h"
#if defined(_WIN32)
#include "WMFVideoWriter.h"
#endif


VideoWriter::VideoWriter()
{

}


VideoWriter::~VideoWriter()
{

}


Reference<VideoWriter> VideoWriter::create(const std::string& path, const VidParams& vid_params)
{
#if defined(_WIN32)
	return new WMFVideoWriter(path, vid_params);
#else
	// TODO: use AVFVideoWriter on Mac (AVFVideoWriter.mm needs to be added to the build).
	throw glare::Exception("Video writing is not supported on this platform.");
#endif
}
