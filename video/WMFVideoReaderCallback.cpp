/*=====================================================================
WMFVideoReaderCallback.cpp
--------------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WMFVideoReaderCallback.h"


#ifdef _WIN32


#include "WMFVideoReader.h"
#include "../utils/ConPrint.h"


// IUnknown methods
STDMETHODIMP WMFVideoReaderCallback::QueryInterface(REFIID iid, void** ppv)
{
	// Always set out parameter to NULL, validating it first.
	if (!ppv)
		return E_INVALIDARG;
	*ppv = NULL;
	if(iid == IID_IUnknown || iid == IID_IMFSourceReaderCallback)
	{
		// Increment the reference count and return the pointer.
		*ppv = (LPVOID)this;
		AddRef();
		return NOERROR;
	}
	return E_NOINTERFACE;
}


STDMETHODIMP_(ULONG) WMFVideoReaderCallback::AddRef()
{
	// See https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/mediafoundation/MFCaptureD3D/preview.cpp
	return InterlockedIncrement(&m_nRefCount);
}


STDMETHODIMP_(ULONG) WMFVideoReaderCallback::Release()
{
	const LONG uCount = InterlockedDecrement(&m_nRefCount);
	if(uCount == 0)
		delete this;
	return uCount;
}


// IMFSourceReaderCallback methods
STDMETHODIMP WMFVideoReaderCallback::OnReadSample(
	HRESULT hrStatus,
	DWORD dwStreamIndex,
	DWORD dwStreamFlags,
	LONGLONG llTimestamp,
	IMFSample* pSample
)
{
	if(video_reader)
	{
		video_reader->OnReadSample(
			hrStatus,
			dwStreamIndex,
			dwStreamFlags,
			llTimestamp,
			pSample
		);
	}
	
	return S_OK;
}


STDMETHODIMP WMFVideoReaderCallback::OnEvent(DWORD, IMFMediaEvent *)
{
	return S_OK;
}


STDMETHODIMP WMFVideoReaderCallback::OnFlush(DWORD)
{
	//conPrint("WMFVideoReaderCallback::OnFlush");
	to_vid_reader_queue->enqueue(1);
	return S_OK;
}


// Constructor is private. Use static CreateInstance method to create.
WMFVideoReaderCallback::WMFVideoReaderCallback()
:	m_nRefCount(0),
	video_reader(NULL)
{
}

// Destructor is private. Caller should call Release.
WMFVideoReaderCallback::~WMFVideoReaderCallback()
{
}


#endif // _WIN32
