/*=====================================================================
WMFVideoReader.cpp
-------------------
Copyright Glare Technologies Limited 2020 -
Generated at 2020-01-12 14:59:19 +1300
=====================================================================*/
#include "WMFVideoReader.h"


#ifdef _WIN32


#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>



// Some code adapted from https://docs.microsoft.com/en-us/windows/win32/medfound/processing-media-data-with-the-source-reader


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
		throw Indigo::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

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
		throw Indigo::Exception("MFGetAttributeSize failed: " + PlatformUtils::COMErrorString(hr));

	// Get the stride to find out if the bitmap is top-down or bottom-up.
	LONG lStride = 0;
	lStride = (LONG)MFGetAttributeUINT32(pType.ptr, MF_MT_DEFAULT_STRIDE, 1);

	format.top_down = lStride > 0;

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
	format.internal_width = width;

	return format;
}


static void ConfigureDecoder(IMFSourceReader* pReader, DWORD dwStreamIndex, FormatInfo& format_out)
{
	// Find the native format of the stream.
	ComObHandle<IMFMediaType> pNativeType;
	HRESULT hr = pReader->GetNativeMediaType(dwStreamIndex, 0, &pNativeType.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("GetNativeMediaType failed: " + PlatformUtils::COMErrorString(hr));

	GUID majorType, subtype;

	// Find the major type.
	hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
	if(FAILED(hr))
		throw Indigo::Exception("GetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Find the sub type.
	hr = pNativeType->GetGUID(MF_MT_SUBTYPE, &subtype);
	if(FAILED(hr))
		throw Indigo::Exception("GetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Define the output type.
	ComObHandle<IMFMediaType> pType;
	hr = MFCreateMediaType(&pType.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("MFCreateMediaType failed: " + PlatformUtils::COMErrorString(hr));

	hr = pType->SetGUID(MF_MT_MAJOR_TYPE, majorType);
	if(FAILED(hr))
		throw Indigo::Exception("SetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Select a subtype.
	if(majorType == MFMediaType_Video)
	{
		subtype = MFVideoFormat_RGB32;
	}
	else
	{
		throw Indigo::Exception("Unrecognized type");
	}

	//pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB8);
	pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE); // Specifies whether each sample is independent. Set to TRUE for uncompressed formats.
	//pType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	//pType->SetUINT32(MF_MT_FRAME_SIZE, 1920 * 1080 * 3);

	hr = pType->SetGUID(MF_MT_SUBTYPE, subtype);
	if(FAILED(hr))
		throw Indigo::Exception("SetGUID failed: " + PlatformUtils::COMErrorString(hr));

	// Set the uncompressed format.
	hr = pReader->SetCurrentMediaType(dwStreamIndex, NULL, pType.ptr);
	if(FAILED(hr))
		throw Indigo::Exception("SetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

	format_out = GetVideoFormat(pReader);
}


WMFVideoReader::WMFVideoReader(const std::string& URL)
:	com_inited(false)
{
	// Initialize the COM runtime.
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if(hr == RPC_E_CHANGED_MODE)
	{ 
		// COM has already been initialised in some other mode.
		// This is alright, but we don't want to call CoUninitialize in this case.
	}
	else
	{
		if(!SUCCEEDED(hr))
			throw Indigo::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

		com_inited = true;
	}

	// Initialize the Media Foundation platform.
	hr = MFStartup(MF_VERSION);
	if(!SUCCEEDED(hr))
		throw Indigo::Exception("MFStartup failed: " + PlatformUtils::COMErrorString(hr));

	// Configure the source reader to perform video processing.
	//
	// This includes:
	//   - YUV to RGB-32
	//   - Software deinterlace

	ComObHandle<IMFAttributes> pAttributes;
	hr = MFCreateAttributes(&pAttributes.ptr, 1);
	if(!SUCCEEDED(hr))
		throw Indigo::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

	hr = pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	if(!SUCCEEDED(hr))
		throw Indigo::Exception("COM init failure: " + PlatformUtils::COMErrorString(hr));

	// Create the source reader.
	hr = MFCreateSourceReaderFromURL(StringUtils::UTF8ToPlatformUnicodeEncoding(URL).c_str(), pAttributes.ptr, &this->reader.ptr);
	if(!SUCCEEDED(hr))
		throw Indigo::Exception("MFCreateSourceReaderFromURL failed for URL '" + URL + "': " + PlatformUtils::COMErrorString(hr));

	ConfigureDecoder(this->reader.ptr, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, this->format);
}


WMFVideoReader::~WMFVideoReader()
{
	reader.release();

	// Shut down Media Foundation.
	MFShutdown();

	if(com_inited)
		CoUninitialize();
}


FrameInfo WMFVideoReader::getAndLockNextFrame(BYTE*& frame_buffer_out, size_t& stride_B_out)
{
	FrameInfo frame_info;
	frame_info.frame_time = 0;

	frame_buffer_out = NULL;
	stride_B_out = (size_t)format.internal_width * 4;

	DWORD streamIndex, flags;
	LONGLONG llTimeStamp;
	ComObHandle<IMFSample> pSample;
	HRESULT hr = S_OK;

	hr = reader->ReadSample(
		(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,    // Stream index.
		0,                              // Flags.
		&streamIndex,                   // Receives the actual stream index. 
		&flags,                         // Receives status flags.
		&llTimeStamp,                   // Receives the time stamp. ( in 100-nanosecond units.)
		&pSample.ptr                    // Receives the sample or NULL.
	);
	if(FAILED(hr))
		throw Indigo::Exception("ReadSample failed: " + PlatformUtils::COMErrorString(hr));

	if(flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
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
			throw Indigo::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));

		// Get the width and height
		UINT32 width = 0, height = 0;
		hr = MFGetAttributeSize(media_type.ptr, MF_MT_FRAME_SIZE, &width, &height);
		if(FAILED(hr))
			throw Indigo::Exception("MFGetAttributeSize failed: " + PlatformUtils::COMErrorString(hr));

		format.internal_width = width;
		stride_B_out = (size_t)format.internal_width * 4; // NOTE: should use result of MF_MT_DEFAULT_STRIDE instead? 

		// Get the stride to find out if the bitmap is top-down or bottom-up.
		// LONG lStride = 0;
		// lStride = (LONG)MFGetAttributeUINT32(media_type.ptr, MF_MT_DEFAULT_STRIDE, 1);
	}

	if(pSample.ptr) // May be NULL at end of stream.
	{
		//DWORD total_len;
		//if(pSample->GetTotalLength(&total_len) != S_OK)
		//	throw Indigo::Exception("GetTotalLength failed.");
		// printVar((uint64)total_len);

		//IMFMediaBuffer* pBuffer;
		//if(MFCreateMemoryBuffer(total_len, &pBuffer) != S_OK)
		//	throw Indigo::Exception("MFCreateMemoryBuffer failed.");
		//
		//if(pSample->CopyToBuffer(pBuffer) != S_OK)
		//	throw Indigo::Exception("CopyToBuffer failed.");

		assert(this->buffer_ob.ptr == NULL);
		if(pSample->ConvertToContiguousBuffer(&this->buffer_ob.ptr) != S_OK)
			throw Indigo::Exception("ConvertToContiguousBuffer failed: " + PlatformUtils::COMErrorString(hr));

		//IMF2DBuffer* buffer2d;
		//if(this->buffer_ob->QueryInterface<IMF2DBuffer>(&buffer2d) == S_OK)
		//{}

		BYTE* buffer;
		DWORD cur_len;
		if(this->buffer_ob->Lock(&buffer, /*max-len=*/NULL, &cur_len) != S_OK)
			throw Indigo::Exception("Lock failed: " + PlatformUtils::COMErrorString(hr));

		frame_buffer_out = buffer;
	}

	//const double stream_time = llTimeStamp * 1.0e-7;
	//conPrint("Read sample from stream " + toString((uint64)streamIndex) + " with timestamp " + toString(stream_time) + " s");
	frame_info.frame_time = llTimeStamp * 1.0e-7;

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

	if(flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
	{
		// The format changed. Reconfigure the decoder.
		ConfigureDecoder(this->reader.ptr, streamIndex, format);
	}

	return frame_info;
}

void WMFVideoReader::unlockFrame()
{
	if(this->buffer_ob.ptr)
		this->buffer_ob.ptr->Unlock();

	this->buffer_ob.release();
}


void WMFVideoReader::seek(double time)
{
	PROPVARIANT var;
	PropVariantInit(&var);

	var.vt = VT_I8;
	var.hVal.QuadPart = (LONGLONG)(time * 1.0e7); // 100-nanosecond units.

	HRESULT hr = this->reader->SetCurrentPosition(GUID_NULL, var);
	if(FAILED(hr))
		throw Indigo::Exception("GetCurrentMediaType failed: " + PlatformUtils::COMErrorString(hr));
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void WMFVideoReader::test()
{
	try
	{
		//WMFVideoReader reader("D:\\video\\test5.avi");
		WMFVideoReader reader("D:\\downloads\\y2mate.com - ghost_in_the_shell_1995_official_trailer_hd_p2MEaROKjaE_360p.mp4");

		Timer timer;
		size_t frame_index = 0;
		while(1)
		{
			BYTE* frame_buffer;
			size_t stride_B;
			reader.getAndLockNextFrame(frame_buffer, stride_B);

			if(frame_buffer)
			{
				const FormatInfo& format = reader.getCurrentFormat();

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
			}
			else
			{
				conPrint("Reached EOF.");
				break;
			}

			reader.unlockFrame();
			frame_index++;
		}

		const double fps = frame_index / timer.elapsed();
		conPrint("FPS processed: " + toString(fps));
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif


#endif // _WIN32