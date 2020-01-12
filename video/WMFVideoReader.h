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

	FormatInfo() : im_width(0), im_height(0), top_down(0)
	{
		SetRectEmpty(&rcPicture);
	}
};


/*=====================================================================
WMFVideoReader
--------------
Windows Media Foundation video reader
=====================================================================*/
class WMFVideoReader : public VideoReader
{
public:
	WMFVideoReader(const std::string& URL); // Throws Indigo::Exception
	~WMFVideoReader();

	virtual void getAndLockNextFrame(BYTE*& frame_buffer_out); // frame_buffer_out will be set to NULL if we have reached EOF
	virtual void unlockFrame();

	static void test();

	const FormatInfo& getCurrentFormat() { return format; }
private:
	ComObHandle<IMFSourceReader> reader;
	ComObHandle<IMFMediaBuffer> buffer_ob;

	FormatInfo format;
};


#endif // _WIN32
