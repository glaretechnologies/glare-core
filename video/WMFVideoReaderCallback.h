/*=====================================================================
WMFVideoReaderCallback.h
------------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "VideoReader.h"
#include "ComObHandle.h"
#include "IncludeWindows.h" // NOTE: Just for RECT, remove?
#include "ThreadSafeQueue.h"
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <string>


#ifdef _WIN32


struct IMFSourceReader;
struct IMFMediaBuffer;
struct ID3D11Device;
struct IMFDXGIDeviceManager;
struct IMF2DBuffer;
struct IMFSample;
class WMFVideoReader;


/*=====================================================================
WMFVideoReaderCallback
----------------------
Windows Media Foundation video reader callback.

See https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/mediafoundation/MFCaptureD3D/preview.h

=====================================================================*/
class WMFVideoReaderCallback : public IMFSourceReaderCallback
{
public:
	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFSourceReaderCallback methods
	STDMETHODIMP OnReadSample(
		HRESULT hrStatus,
		DWORD dwStreamIndex,
		DWORD dwStreamFlags,
		LONGLONG llTimestamp,
		IMFSample* pSample
	);

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*);

	STDMETHODIMP OnFlush(DWORD);


	WMFVideoReaderCallback();
	virtual ~WMFVideoReaderCallback();

	long m_nRefCount; // Reference count.

	WMFVideoReader* video_reader;
	
	ThreadSafeQueue<int>* to_vid_reader_queue; // Writes 1 to this queue if flush is called.
};


#endif // _WIN32
