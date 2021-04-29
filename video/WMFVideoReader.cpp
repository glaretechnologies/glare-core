/*=====================================================================
WMFVideoReader.cpp
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WMFVideoReader.h"


#ifdef _WIN32


#include "WMFVideoReaderCallback.h"
#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include "../utils/ComObHandle.h"
#include "../utils/Lock.h"
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <d3d11.h>
#include <d3d11_4.h>


static const bool GPU_DECODE = true;


// Some code adapted from https://docs.microsoft.com/en-us/windows/win32/medfound/processing-media-data-with-the-source-reader
// also
// https://stackoverflow.com/questions/40913196/how-to-properly-use-a-hardware-accelerated-media-foundation-source-reader-to-dec


// Returns true if COM inited, or false if we got RPC_E_CHANGED_MODE, in which case COM is fine to use but shutdownCOM shouldn't be called.
bool WMFVideoReader::initialiseCOM()
{
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if(hr == RPC_E_CHANGED_MODE)
	{ 
		// COM has already been initialised in some other mode.
		// This is alright, but we don't want to call CoUninitialize in this case.
		return false;
	}
	else
	{
		if(!SUCCEEDED(hr))
			throw glare::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

		return true;
	}
}


void WMFVideoReader::shutdownCOM()
{
	CoUninitialize();
}


void WMFVideoReader::initialiseWMF()
{
	// Initialize the Media Foundation platform.
	HRESULT hr = MFStartup(MF_VERSION);
	if(!SUCCEEDED(hr))
		throw glare::Exception("MFStartup failed: " + PlatformUtils::COMErrorString(hr));
}


void WMFVideoReader::shutdownWMF()
{
	MFShutdown();
}


//-----------------------------------------------------------------------------
// CorrectAspectRatio
//
// Converts a rectangle from the source's pixel aspect ratio (PAR) to 1:1 PAR.
// Returns the corrected rectangle.
//
// For example, a 720 x 486 rect with a PAR of 9:10, when converted to 1x1 PAR,  
// is stretched to 720 x 540. 
//-----------------------------------------------------------------------------
static RECT CorrectAspectRatio(const RECT& src, const MFRatio& srcPAR)
{
	// Start with a rectangle the same size as src, but offset to the origin (0,0).
	RECT rc ={ 0, 0, src.right - src.left, src.bottom - src.top };

	if((srcPAR.Numerator != 1) || (srcPAR.Denominator != 1))
	{
		// Correct for the source's PAR.

		if(srcPAR.Numerator > srcPAR.Denominator)
		{
			// The source has "wide" pixels, so stretch the width.
			rc.right = MulDiv(rc.right, srcPAR.Numerator, srcPAR.Denominator);
		}
		else if(srcPAR.Numerator < srcPAR.Denominator)
		{
			// The source has "tall" pixels, so stretch the height.
			rc.bottom = MulDiv(rc.bottom, srcPAR.Denominator, srcPAR.Numerator);
		}
		// else: PAR is 1:1, which is a no-op.
	}
	return rc;
}


//-------------------------------------------------------------------
// GetVideoFormat
// 
// Gets format information for the video stream.
//
// iStream: Stream index.
// pFormat: Receives the format information.

// NOTE: From https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/mediafoundation/VideoThumbnail/Thumbnail.cpp
//-------------------------------------------------------------------
static FormatInfo GetVideoFormat(IMFSourceReader* pReader)
{
	FormatInfo format;

	MFRatio par = { 0 , 0 }; // pixel aspect ratio
	GUID subtype = { 0 };

	ComObHandle<IMFMediaType> pType;

	// Get the media type from the stream.
	HRESULT hr = pReader->GetCurrentMediaType(
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		&pType.ptr
	);
	if(FAILED(hr))
		throw glare::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

	// Make sure it is a video format.
	/*hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
	if(subtype != MFVideoFormat_RGB32)
	{
		hr = E_UNEXPECTED;
		goto done;
	}*/

	// Get the width and height
	UINT32 width = 0, height = 0;
	hr = MFGetAttributeSize(pType.ptr, MF_MT_FRAME_SIZE, &width, &height);
	if(FAILED(hr))
		throw glare::Exception("MFGetAttributeSize failed: " + PlatformUtils::COMErrorString(hr));

	// Get the stride to find out if the bitmap is top-down or bottom-up.
	LONG lStride = 0;
	lStride = (LONG)MFGetAttributeUINT32(pType.ptr, MF_MT_DEFAULT_STRIDE, 1);

	format.top_down = lStride > 0;
	format.stride_B = abs(lStride);

	// Get the pixel aspect ratio. (This value might not be set.)
	hr = MFGetAttributeRatio(pType.ptr, MF_MT_PIXEL_ASPECT_RATIO, (UINT32*)&par.Numerator, (UINT32*)&par.Denominator);
	if(SUCCEEDED(hr) && (par.Denominator != par.Numerator))
	{
		RECT rcSrc = { 0, 0, (LONG)width, (LONG)height };
		format.rcPicture = CorrectAspectRatio(rcSrc, par);
	}
	else
	{
		// Either the PAR is not set (assume 1:1), or the PAR is set to 1:1.
		SetRect(&format.rcPicture, 0, 0, width, height);
	}

	format.im_width = width;
	format.im_height = height;
	//format.internal_width = width;

	return format;
}


static inline void throwOnError(HRESULT hres)
{
	if(FAILED(hres))
		throw glare::Exception("Error: " + PlatformUtils::COMErrorString(hres));
}


static void ConfigureDecoder(IMFSourceReader* pReader, DWORD dwStreamIndex, FormatInfo& format_out)
{
	// Find the native format of the stream.
	ComObHandle<IMFMediaType> pNativeType;
	HRESULT hr = pReader->GetNativeMediaType(dwStreamIndex, 0, &pNativeType.ptr);
	if(FAILED(hr))
		throw glare::Exception("GetNativeMediaType failed: " + PlatformUtils::COMErrorString(hr));

	GUID majorType, subtype;

	// Find the major type.
	hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
	if(FAILED(hr))
		throw glare::Exception("GetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Find the sub type.
	hr = pNativeType->GetGUID(MF_MT_SUBTYPE, &subtype);
	if(FAILED(hr))
		throw glare::Exception("GetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Define the output type.
	ComObHandle<IMFMediaType> pType;
	hr = MFCreateMediaType(&pType.ptr);
	if(FAILED(hr))
		throw glare::Exception("MFCreateMediaType failed: " + PlatformUtils::COMErrorString(hr));

	hr = pType->SetGUID(MF_MT_MAJOR_TYPE, majorType);
	if(FAILED(hr))
		throw glare::Exception("SetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Select a subtype.
	if(majorType == MFMediaType_Video)
	{
		subtype = MFVideoFormat_RGB32; // MFVideoFormat_RGB24 doesn't seem to work.
	}
	else
	{
		throw glare::Exception("Unrecognized type");
	}

	//pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB8);
	//handle_result(pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
//	handle_result(pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE)); // Specifies whether each sample is independent. Set to TRUE for uncompressed formats.
	//pType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	//pType->SetUINT32(MF_MT_FRAME_SIZE, 1920 * 1080 * 3);

	hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);
	if(FAILED(hr))
		throw glare::Exception("SetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Set the uncompressed format.
	hr = pReader->SetCurrentMediaType(dwStreamIndex, NULL, pType.ptr);
	if(FAILED(hr))
		throw glare::Exception("SetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

	format_out = GetVideoFormat(pReader);
}


WMFVideoReader::WMFVideoReader(bool read_from_video_device_, const std::string& URL, VideoReaderCallback* reader_callback_)
:	read_from_video_device(read_from_video_device_),
	reader_callback(reader_callback_),
	com_reader_callback(NULL)
{
	HRESULT hr;

	if(GPU_DECODE)
	{
		// Create a GPU device.  Needed to get hardware accelerated decoding.

		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

		hr = D3D11CreateDevice(
			nullptr, // pAdapter - use the default adaptor
			D3D_DRIVER_TYPE_HARDWARE, // DriverType
			nullptr, // Software
			/*D3D11_CREATE_DEVICE_SINGLETHREADED |*/ D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
			levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d_device.ptr, 
			nullptr, // pFeatureLevel
			nullptr // ppImmediateContext
		);
		if(!SUCCEEDED(hr))
			throw glare::Exception("D3D11CreateDevice failed: " + PlatformUtils::COMErrorString(hr));

		// Get ready for multi-threaded operation
		ComObHandle<ID3D11Multithread> multithreaded_device;
		if(!d3d_device.queryInterface(multithreaded_device))
			throw glare::Exception("failed to get ID3D11Multithread interace.");
	
		multithreaded_device->SetMultithreadProtected(TRUE);

		UINT reset_token;
		throwOnError(MFCreateDXGIDeviceManager(&reset_token, &dev_manager.ptr));
		throwOnError(dev_manager->ResetDevice(d3d_device.ptr, reset_token));
	}

	// Configure the source reader to perform video processing.
	//
	// This includes:
	//   - YUV to RGB-32
	//   - Software deinterlace

	ComObHandle<IMFAttributes> pAttributes;
	hr = MFCreateAttributes(&pAttributes.ptr, 1);
	if(!SUCCEEDED(hr))
		throw glare::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

	if(GPU_DECODE)
	{
		throwOnError(pAttributes.ptr->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, dev_manager.ptr));
		throwOnError(pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
		throwOnError(pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE));
	}
	else
	{
		throwOnError(pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE));
	}

	if(reader_callback != NULL)
	{
		this->com_reader_callback = new WMFVideoReaderCallback();
		this->com_reader_callback->AddRef();
		this->com_reader_callback->video_reader = this;
		this->com_reader_callback->to_vid_reader_queue = &from_com_reader_callback_queue;
		
		throwOnError(pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this->com_reader_callback));
	}

	if(read_from_video_device)
	{
		// Source type: video capture devices
		hr = pAttributes->SetGUID(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
		);
		if(!SUCCEEDED(hr))
			throw glare::Exception("failure: " + PlatformUtils::COMErrorString(hr));

		// Enumerate devices.
		UINT32 count;
		IMFActivate **ppDevices = NULL;
		hr = MFEnumDeviceSources(pAttributes.ptr, &ppDevices, &count);
		if(!SUCCEEDED(hr))
			throw glare::Exception("failure: " + PlatformUtils::COMErrorString(hr));

		if(count == 0)
			throw glare::Exception("No video devices, cannot read from video device");

		// Create the media source object.
		IMFMediaSource* media_source = NULL;
		hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&media_source));
		if(!SUCCEEDED(hr))
			throw glare::Exception("failure: " + PlatformUtils::COMErrorString(hr));

		hr = MFCreateSourceReaderFromMediaSource(
			media_source,
			pAttributes.ptr,
			&this->reader.ptr);

		if(!SUCCEEDED(hr))
			throw glare::Exception("MFCreateSourceReaderFromMediaSource failed: " + PlatformUtils::COMErrorString(hr));
	}
	else
	{
		// Create the source reader.
		hr = MFCreateSourceReaderFromURL(StringUtils::UTF8ToPlatformUnicodeEncoding(URL).c_str(), pAttributes.ptr, &this->reader.ptr);
		if(!SUCCEEDED(hr))
			throw glare::Exception("MFCreateSourceReaderFromURL failed for URL '" + URL + "': " + PlatformUtils::COMErrorString(hr));
	}
	
	ConfigureDecoder(this->reader.ptr, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, this->current_format);
}


WMFVideoReader::~WMFVideoReader()
{
	//conPrint("WMFVideoReader::~WMFVideoReader");
	Timer timer;
	// Flush all remaining sample reads in progress.  "The Flush method discards all queued samples and cancels all pending sample requests." - 
	// https://docs.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-flush
	reader->Flush((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM);
	if(com_reader_callback)
	{
		//conPrint("Waiting for flush");
		// Wait until the flush even has been received by the callback
		int x;
		from_com_reader_callback_queue.dequeue(x);

		//conPrint("Finished waiting for flush. elapsed: " + timer.elapsedString());

		com_reader_callback->video_reader = NULL; // null out pointer to this object.
		this->com_reader_callback->Release();
	}

	reader.release();

	//conPrint("Shutting down WMFVideoReader took " + timer.elapsedString());
}


void WMFVideoReader::startReadingNextFrame()
{
	if(reader.ptr)
	{
		// We are using asyncrhonous mode, so set all out parameters to NULL: https://docs.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-imfsourcereader-readsample
		HRESULT hr = reader->ReadSample(
			(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, // dwStreamIndex
			0,    // dwControlFlags
			NULL, // pdwActualStreamIndex
			NULL, // pdwStreamFlags
			NULL, // pllTimestamp
			NULL  // ppSample
		);
		if(!SUCCEEDED(hr))
			throw glare::Exception("ReadSample failed: " + PlatformUtils::COMErrorString(hr));
	}
}


FrameInfo WMFVideoReader::getAndLockNextFrame()
{
	ComObHandle<IMFSample> cur_sample;

	DWORD streamIndex, flags;
	LONGLONG llTimeStamp;
	HRESULT hr;
	hr = reader->ReadSample(
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,    // Stream index.
		0,                              // Flags.
		&streamIndex,                   // Receives the actual stream index. 
		&flags,                         // Receives status flags.
		&llTimeStamp,                   // Receives the time stamp. ( in 100-nanosecond units.)
		&cur_sample.ptr                 // Receives the sample or NULL.
	);
	if(FAILED(hr))
		throw glare::Exception("ReadSample failed: " + PlatformUtils::COMErrorString(hr));

	if(flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
	{
		// The current media has type changed for one or more streams.
		// This seems to happen e.g. when the video widths and heights are padded to a multiple of 16 for h264.

		// Get the media type from the stream.
		ComObHandle<IMFMediaType> media_type;
		hr = reader->GetCurrentMediaType(
			(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			&media_type.ptr
		);
		if(FAILED(hr))
			throw glare::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

		// Get the width and height
		UINT32 width = 0, height = 0;
		hr = MFGetAttributeSize(media_type.ptr, MF_MT_FRAME_SIZE, &width, &height);
		if(FAILED(hr))
			throw glare::Exception("MFGetAttributeSize failed: " + PlatformUtils::COMErrorString(hr));

		current_format.im_width = width;
		//stride_B_out = (size_t)format.internal_width * 4; // NOTE: should use result of MF_MT_DEFAULT_STRIDE instead? 

		// Get the stride to find out if the bitmap is top-down or bottom-up.
		LONG lStride = 0;
		lStride = (LONG)MFGetAttributeUINT32(media_type.ptr, MF_MT_DEFAULT_STRIDE, 1);

		current_format.top_down = lStride > 0; // lStride may be negative to indicate a bottom-up image.
		current_format.stride_B = abs(lStride);
	}

	if(flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) // "The native format has changed for one or more streams. The native format is the format delivered by the media source before any decoders are inserted."
	{
		// The format changed. Reconfigure the decoder.
		ConfigureDecoder(this->reader.ptr, streamIndex, current_format);
	}

	//conPrint("Read sample from stream " + toString((uint64)streamIndex) + " with timestamp " + toString(frame_info.frame_time) + " s");

	FrameInfo frame_info;
	frame_info.frame_time = llTimeStamp * 1.0e-7; // Timestamp is in 100 ns units, convert to s.
	frame_info.width  = current_format.im_width;
	frame_info.height = current_format.im_height;
	frame_info.top_down = current_format.top_down;

	if(cur_sample.ptr) // May be NULL at end of stream.
	{
		//IMFMediaBuffer* buffer_ob = NULL;
		ComObHandle<IMFMediaBuffer> buffer_ob;
		ComObHandle<IMF2DBuffer> buffer2d;

		//assert(this->buffer_ob.ptr == NULL);
		hr = cur_sample->ConvertToContiguousBuffer(&buffer_ob.ptr); // Receives a pointer to the IMFMediaBuffer interface. The caller must release the interface.
		if(hr != S_OK) 
			throw glare::Exception("ConvertToContiguousBuffer failed: " + PlatformUtils::COMErrorString(hr));

		if(true) // If use IMF2DBuffer interface:
		{
			if(!buffer_ob.queryInterface(buffer2d))
				throw glare::Exception("failed to get IMF2DBuffer interface");

			BYTE* scanline0;
			LONG pitch;
			hr = buffer2d->Lock2D(&scanline0, &pitch);
			if(FAILED(hr))
				throw glare::Exception("Error: " + PlatformUtils::COMErrorString(hr));

			frame_info.frame_buffer = scanline0;
			frame_info.top_down = pitch > 0;
			frame_info.stride_B = abs(pitch);
			frame_info.media_buffer = (void*)buffer_ob.ptr;
			frame_info.buffer2d = (void*)buffer2d.ptr;

			// Add some more ref counts to represent the reference in frame_info
			buffer_ob->AddRef();
			buffer2d->AddRef();
		}
		else
		{
			BYTE* buffer;
			DWORD cur_len;
			hr = buffer_ob->Lock(&buffer, /*max-len=*/NULL, &cur_len);
			if(FAILED(hr))
				throw glare::Exception("Lock failed: " + PlatformUtils::COMErrorString(hr));

			frame_info.frame_buffer = buffer;
			frame_info.stride_B = current_format.stride_B;
		}
	}

	/*if(flags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		wprintf(L"\tEnd of stream\n");
	}
	if(flags & MF_SOURCE_READERF_NEWSTREAM)
	{
		wprintf(L"\tNew stream\n");
	}
	if(flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
	{
		wprintf(L"\tNative type changed\n");
	}
	if(flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
	{
		wprintf(L"\tCurrent type changed\n");
	}
	if(flags & MF_SOURCE_READERF_STREAMTICK)
	{
		wprintf(L"\tStream tick\n");
	}*/

	return frame_info;
}


void WMFVideoReader::unlockAndReleaseFrame(const FrameInfo& frameinfo)
{
	IMFMediaBuffer* media_buffer = (IMFMediaBuffer*)frameinfo.media_buffer;
	IMF2DBuffer* buffer2d = (IMF2DBuffer*)frameinfo.buffer2d;

	// NOTE: am assuming we were using IMF2DBuffer interface.

	if(buffer2d)
	{
		throwOnError(buffer2d->Unlock2D());

		buffer2d->Release();
	}

	if(media_buffer)
		media_buffer->Release();
}


void WMFVideoReader::OnReadSample(
	HRESULT hrStatus,
	DWORD dwStreamIndex,
	DWORD dwStreamFlags,
	LONGLONG llTimestamp,
	IMFSample* pSample // A pointer to the IMFSample interface of a media sample. This parameter might be NULL.
)
{
	if(SUCCEEDED(hrStatus))
	{
		HRESULT hr;

		if(dwStreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
		{
			// The current media has type changed for one or more streams.
			// This seems to happen when the video widths and heights are e.g. padded to a multiple of 16 for h264.

			// Get the media type from the stream.
			ComObHandle<IMFMediaType> media_type;
			hr = reader->GetCurrentMediaType(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				&media_type.ptr
			);
			if(FAILED(hr))
				throw glare::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

			// Get the width and height
			UINT32 width = 0, height = 0;
			hr = MFGetAttributeSize(media_type.ptr, MF_MT_FRAME_SIZE, &width, &height);
			if(FAILED(hr))
				throw glare::Exception("MFGetAttributeSize failed: " + PlatformUtils::COMErrorString(hr));

			current_format.im_width = width;
			//stride_B = (size_t)current_format.im_width * 4; // NOTE: should use result of MF_MT_DEFAULT_STRIDE instead? 

			// Get the stride to find out if the bitmap is top-down or bottom-up.
			LONG lStride = 0;
			lStride = (LONG)MFGetAttributeUINT32(media_type.ptr, MF_MT_DEFAULT_STRIDE, 1);
			
			current_format.top_down = lStride > 0;
			current_format.stride_B = abs(lStride);
		}

		if(dwStreamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
		{
			// The format changed. Reconfigure the decoder.
			ConfigureDecoder(this->reader.ptr, dwStreamIndex, current_format);
		}

		ComObHandle<IMFMediaBuffer> media_buffer;
		ComObHandle<IMF2DBuffer> buffer2d;

		if(pSample) // May be NULL at end of stream.
		{
			hr = pSample->ConvertToContiguousBuffer(&media_buffer.ptr); // Receives a pointer to the IMFMediaBuffer interface. The caller must release the interface.
			if(hr != S_OK) 
				throw glare::Exception("ConvertToContiguousBuffer failed: " + PlatformUtils::COMErrorString(hr));

			FrameInfo frame_info;
			frame_info.frame_time = llTimestamp * 1.0e-7;
			frame_info.width  = current_format.im_width;
			frame_info.height = current_format.im_height;
			frame_info.top_down = current_format.top_down;

			if(true) // If use IMF2DBuffer interface:
			{
				if(!media_buffer.queryInterface(buffer2d))
					throw glare::Exception("failed to get IMF2DBuffer interface");

				BYTE* scanline0;
				LONG pitch;
				hr = buffer2d->Lock2D(&scanline0, &pitch);
				if(FAILED(hr))
					throw glare::Exception("Error: " + PlatformUtils::COMErrorString(hr));

				frame_info.frame_buffer = scanline0;
				frame_info.top_down = pitch > 0;
				frame_info.stride_B = abs(pitch);
			}
			else
			{
				BYTE* buffer;
				DWORD cur_len;
				hr = media_buffer->Lock(&buffer, /*max-len=*/NULL, &cur_len);
				if(FAILED(hr))
					throw glare::Exception("Lock failed: " + PlatformUtils::COMErrorString(hr));

				frame_info.frame_buffer = buffer;
				frame_info.stride_B = current_format.stride_B;
			}

			// Add some more ref counts to represent the reference in frame_info
			media_buffer->AddRef();
			buffer2d->AddRef();

			frame_info.media_buffer = (void*)media_buffer.ptr;
			frame_info.buffer2d = (void*)buffer2d.ptr;
			this->reader_callback->frameDecoded(this, frame_info);
		}
	
		if(dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			this->reader_callback->endOfStream(this);
		}
	}
	else
	{
		// There was an error while streaming.
		conPrint("OnReadSample: Error while streaming: " + PlatformUtils::COMErrorString(hrStatus));
	}
}


void WMFVideoReader::seek(double time)
{
	if(read_from_video_device)
		return; // Can't seek reading from a webcam etc.

	PROPVARIANT var;
	PropVariantInit(&var);

	var.vt = VT_I8;
	var.hVal.QuadPart = (LONGLONG)(time * 1.0e7); // 100-nanosecond units.

	HRESULT hr = this->reader->SetCurrentPosition(GUID_NULL, var);
	if(FAILED(hr))
		throw glare::Exception("SetCurrentPosition failed: " + PlatformUtils::COMErrorString(hr));
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/CircularBuffer.h"
#include "../utils/Mutex.h"
#include "../utils/Lock.h"
#include "../utils/ThreadSafeQueue.h"


class TestWMFVideoReaderCallback : public VideoReaderCallback
{
public:
	virtual void frameDecoded(VideoReader* vid_reader, const FrameInfo& frameinfo)
	{
		frame_queue->enqueue(frameinfo);
	}

	virtual void endOfStream(VideoReader* vid_reader)
	{
		frame_queue->enqueue(FrameInfo());
	}

	ThreadSafeQueue<FrameInfo>* frame_queue;
};


void WMFVideoReader::test()
{
	try
	{
		//const std::string URL = "E:\\video\\gopro\\GP010415.MP4";
		//const std::string URL = "E:\\video\\Tokyo night drive 4K 2016.mp4";
		//const std::string URL = "E:\\video\\highway_timelapse_jim_lamaison.mp4";
		//const std::string URL = "D:\\art\\chaotica\\anim_test2\\anim_HEVC.mp4";
		//const std::string URL = "E:\\video\\vj\\anim.mp4";
		//const std::string URL = "http://video.chaoticafractals.com/videos/tatasz_skulkey_starshine.mp4";
		//const std::string URL = "E:\\video\\tatasz_skulkey_starshine.mp4";
		//const std::string URL = "E:\\video\\Koyaanisqatsi.1982.1080p.BRrip.x264.AAC.MVGroup.org.mp4";
		//const std::string URL = "E:\\video\\line 1 historical context.mp4";
		//const std::string URL = "https://fnd.fleek.co/fnd-prod/QmYKYb5ASLqPeVGoRBL8pLHq9y8oRCZZFTgdVo4ckvQy5D/nft.mp4"; // pitch is != width*4
		//const std::string URL = "http://video.chaoticafractals.com/videos/lindelokse_tatasz_dewberries.mp4";
		//const std::string URL = "https://fnd.fleek.co/fnd-prod/QmZD6JqWswoDRjpbFmVXkQ8jDbFd6rEuit3wboFD4z8XEe/nft.mp4"; // 17 s
		const std::string URL = "E:\\video\\busted.mp4"; // 17 s

		ThreadSafeQueue<FrameInfo> frame_queue;
		TestWMFVideoReaderCallback callback;

		const bool TEST_ASYNC_CALLBACK = false;
		callback.frame_queue = &frame_queue;

		conPrint("Reading vid '" + URL + "'...");

		Timer timer;

		WMFVideoReader reader(/*read_from_video_device*/false, URL, TEST_ASYNC_CALLBACK ? &callback : NULL);

		conPrint("Reader construction took " + timer.elapsedString());
		

		
		size_t frame_index = 0;
		double last_frame_time = 0;
		Timer play_timer;

		if(TEST_ASYNC_CALLBACK) // If test async callback API:
		{
			for(int i=0; i<5; ++i)
				reader.startReadingNextFrame();

			
			while(1)
			{
				// Wait until the play time is past the last frame time
				while(play_timer.elapsed() < last_frame_time)
					PlatformUtils::Sleep(1);

				// Check if there is a new frame to consume
				FrameInfo front_frame;
				size_t queue_size;
				bool got_item = false;
				{
					Lock lock(frame_queue.getMutex());
					if(frame_queue.unlockedNonEmpty())
					{
						frame_queue.unlockedDequeue(front_frame);
						got_item = true;
					}
					queue_size = frame_queue.size();
				}
				if(got_item)
				{
					if(front_frame.frame_buffer)
					{
						frame_index++;
						last_frame_time = front_frame.frame_time;
						conPrint("Processing frame, time " + toString(front_frame.frame_time) + ", queue_size: " + toString(queue_size));
						// Simulate taking a long time with the frame:
						//PlatformUtils::Sleep(1000);
						reader.unlockAndReleaseFrame(front_frame);

						reader.startReadingNextFrame();
					}
					else
					{
						//conPrint("Received EOS frame");

						if(last_frame_time != 0)
						{
							conPrint("seeking to beginning...");
							const double fps = frame_index / play_timer.elapsed();
							conPrint("FPS processed: " + toString(fps));
							reader.seek(0.0);
						
							frame_index = 0;
							last_frame_time = 0.0;
							play_timer.reset();

							for(int i=0; i<5; ++i)
								reader.startReadingNextFrame();
						}
					}
				}
			}
		}
		else
		{
			while(1)
			{
				FrameInfo frame_info = reader.getAndLockNextFrame();

				if(frame_info.frame_buffer)
				{
					//const FormatInfo& format = reader.getCurrentFormat();

					/*ImageMapUInt8 map(format.im_width, format.im_height, 3);
					for(uint32 y=0; y<format.im_height; ++y)
					{
						BYTE* scanline = frame_buffer + (size_t)y * stride_B;
				
						for(uint32 x=0; x<format.im_width; ++x)
						{
							map.getPixel(x, y)[0] = scanline[x*4 + 2];
							map.getPixel(x, y)[1] = scanline[x*4 + 1];
							map.getPixel(x, y)[2] = scanline[x*4 + 0];
						}
					}
					PNGDecoder::write(map, "D:\\indigo_temp\\movie_output\\frame_" + toString(frame_index) + ".png");
					conPrint("Saved frame " + toString(frame_index));*/

					if(frame_index % 100 == 0)
					{
						const double fps = frame_index / timer.elapsed();
						conPrint("FPS: " + toString(fps));
					}
				}
				else
				{
					conPrint("Reached EOF.");
					reader.seek(0.0);
					//break;
				}

				reader.unlockAndReleaseFrame(frame_info);
				frame_index++;
			}
		}

		const double fps = frame_index / timer.elapsed();
		conPrint("FPS processed: " + toString(fps));
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS


#endif // _WIN32
