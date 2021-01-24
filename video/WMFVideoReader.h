/*=====================================================================
WMFVideoReader.h
----------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:59:19 +1300
=====================================================================*/
#pragma once


#include "VideoReader.h"
#include "ComObHandle.h"
#include "IncludeWindows.h" // NOTE: Just for RECT, remove?
#include <string>


#ifdef _WIN32


struct IMFSourceReader;
struct IMFMediaBuffer;


struct FormatInfo
{
	uint32			im_width; // in pixels
	uint32			im_height;
	bool			top_down;
	RECT			rcPicture;    // Corrected for pixel aspect ratio
	uint32			internal_width; // in pixels

	FormatInfo() : im_width(0), im_height(0), internal_width(0), top_down(0)
	{
		SetRectEmpty(&rcPicture);
	}
};


/*=====================================================================
WMFVideoReader
--------------
Windows Media Foundation video reader.

The following libs are needed for this code:
mfplat.lib mfreadwrite.lib mfuuid.lib
=====================================================================*/
class WMFVideoReader : public VideoReader
{
public:
	WMFVideoReader(bool read_from_video_device, const std::string& URL); // Throws Indigo::Exception
	~WMFVideoReader();

	virtual FrameInfo getAndLockNextFrame(BYTE*& frame_buffer_out, size_t& stride_B_out); // frame_buffer_out will be set to NULL if we have reached EOF
	virtual void unlockFrame();

	virtual void seek(double time);

	static void test();

	const FormatInfo& getCurrentFormat() { return format; }

	bool isReadingFromVidDevice() const { return read_from_video_device; }
private:
	ComObHandle<IMFSourceReader> reader;
	ComObHandle<IMFMediaBuffer> buffer_ob;

	FormatInfo format;
	bool com_inited;
	bool read_from_video_device;
};


#endif // _WIN32
