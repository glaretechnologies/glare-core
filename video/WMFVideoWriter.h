/*=====================================================================
WMFVideoWriter.h
----------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:59:19 +1300
=====================================================================*/
#pragma once


#include "VideoWriter.h"
#include "ComObHandle.h"
#include "IncludeWindows.h"
#include <string>


#ifdef _WIN32


struct IMFSinkWriter;
struct IMFMediaBuffer;


/*=====================================================================
WMFVideoWriter
--------------
Windows Media Foundation video writer.

The following libs are needed for this code:
mfplat.lib mfreadwrite.lib mfuuid.lib

See https://docs.microsoft.com/en-us/windows/win32/medfound/tutorial--using-the-sink-writer-to-encode-video
=====================================================================*/
class WMFVideoWriter : public VideoWriter
{
public:
	WMFVideoWriter(const std::string& URL, const VidParams& vid_params); // Throws glare::Exception
	~WMFVideoWriter();

	static void test();

	// RGB data
	virtual void writeFrame(const uint8* data, size_t source_row_stride_B);

	virtual void finalise();

	const VidParams& getVidParams() const { return vid_params; }
private:
	ComObHandle<IMFSinkWriter> writer;

	DWORD stream_index;
	uint64 frame_index;
	VidParams vid_params;
	bool com_inited;
};


#endif // _WIN32
