/*=====================================================================
WMFVideoReader.h
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "VideoReader.h"
#include "ComObHandle.h"
#include "IncludeWindows.h" // NOTE: Just for RECT, remove?
#include "../utils/Mutex.h"
#include "../utils/ThreadSafeQueue.h"
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <string>


class WMFVideoReaderCallback;


#ifdef _WIN32


struct IMFSourceReader;
struct IMFMediaBuffer;
struct ID3D11Device;
struct IMFDXGIDeviceManager;
struct IMF2DBuffer;
struct IMFSample;
struct IMFMediaEvent;



struct FormatInfo
{
	uint32			im_width; // in pixels
	uint32			im_height;
	bool			top_down;
	RECT			rcPicture; // Corrected for pixel aspect ratio
	//uint32		internal_width; // in pixels
	uint32			stride_B;

	FormatInfo() : im_width(0), im_height(0), /*internal_width(0), */top_down(0), stride_B(0)
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
	static bool initialiseCOM(); // Returns true if COM inited, or false if we got RPC_E_CHANGED_MODE, in which case COM is fine to use but shutdownCOM shouldn't be called.
	static void shutdownCOM();
	static void initialiseWMF();
	static void shutdownWMF();

	// COM and WMF should be initialised before a WMFVideoReader is constructed.
	WMFVideoReader(bool read_from_video_device, const std::string& URL, VideoReaderCallback* reader_callback); // Throws Indigo::Exception
	~WMFVideoReader();

	virtual void startReadingNextFrame();

	virtual FrameInfo getAndLockNextFrame(); // FrameInfo.frame_buffer will be set to NULL if we have reached EOF

	virtual void unlockAndReleaseFrame(const FrameInfo& frameinfo);

	virtual void seek(double time);

	static void test();

	// NOTE: be careful using this with async callbacks, may not match current frame.  Better to use the values in FrameInfo instead.
	const FormatInfo& getCurrentFormat() { return current_format; }

	bool isReadingFromVidDevice() const { return read_from_video_device; }

	// Called from WMFVideoReaderCallback
	void OnReadSample(
		HRESULT hrStatus,
		DWORD dwStreamIndex,
		DWORD dwStreamFlags,
		LONGLONG llTimestamp,
		IMFSample* pSample
	);

private:
	ComObHandle<IMFSourceReader> reader;
	ComObHandle<ID3D11Device> d3d_device;
	ComObHandle<IMFDXGIDeviceManager> dev_manager;

	FormatInfo current_format;
	bool read_from_video_device;

	VideoReaderCallback* reader_callback;

	WMFVideoReaderCallback* com_reader_callback;

	ThreadSafeQueue<int> from_com_reader_callback_queue; // For messages from com_reader_callback
};


#endif // _WIN32
