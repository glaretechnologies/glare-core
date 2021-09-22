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
#include "../utils/PoolAllocator.h"
#ifdef _WIN32
#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#endif
#include <string>
#include <map>
#include <set>


class WMFVideoReaderCallback;
class WMFVideoReader;

#ifdef _WIN32


struct IMFSourceReader;
struct IMFMediaBuffer;
struct ID3D11Device;
struct IMFDXGIDeviceManager;
struct IMF2DBuffer;
struct IMFSample;
struct IMFMediaEvent;
struct ID3D11Texture2D;


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

	// Audio:
	uint32			num_channels;
	uint32			sample_rate_hz;
	uint32			bits_per_sample;
};


class TexturePool : public ThreadSafeRefCounted
{
public:
	Mutex mutex;
	// We'll just store raw pointers.  objects should be AddRef'd before breing added to the pool, and Released afterwards.
	std::set<ID3D11Texture2D*> used_textures; // Textures currently used by a sample
	std::vector<ID3D11Texture2D*> free_textures; // Textures that were used by a sample, but have been freed.
};


class WMFSampleInfo : public SampleInfo
{
public:
	WMFSampleInfo();
	~WMFSampleInfo();

	ComObHandle<IMFMediaBuffer> media_buffer;
	ComObHandle<IMF2DBuffer> buffer2d;
	ComObHandle<ID3D11Texture2D> d3d_tex;

	bool media_buffer_locked;

	Reference<TexturePool> texture_pool;
};


// Template specialisation of destroyAndFreeOb for WMFFrameInfo.
template <>
inline void destroyAndFreeOb<WMFSampleInfo>(WMFSampleInfo* ob)
{
	Reference<glare::Allocator> allocator = ob->allocator;

	// Destroy object
	ob->~WMFSampleInfo();

	// Free object mem
	if(allocator.nonNull())
		ob->allocator->free(ob);
	else
		delete ob;
}


/*=====================================================================
WMFVideoReader
--------------
Windows Media Foundation video reader.  Also can read audio files.

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
	WMFVideoReader(bool read_from_video_device, bool just_read_audio, const std::string& URL, VideoReaderCallback* reader_callback, IMFDXGIDeviceManager* dx_device_manager, bool decode_to_d3d_tex); // Throws Indigo::Exception
	~WMFVideoReader();

	virtual void startReadingNextSample() override;

	virtual Reference<SampleInfo> getAndLockNextSample(bool just_get_vid_sample) override; // FrameInfo.frame_buffer will be set to NULL if we have reached EOF

	virtual void seek(double time) override;

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
	WMFSampleInfo* allocWMFSampleInfo();

	ComObHandle<IMFSourceReader> reader;

	FormatInfo current_format;
	bool read_from_video_device;
	bool decode_to_d3d_tex;

	bool stream_is_video[10];

	VideoReaderCallback* reader_callback;

	WMFVideoReaderCallback* com_reader_callback;

	ThreadSafeQueue<int> from_com_reader_callback_queue; // For messages from com_reader_callback

public:
	Reference<TexturePool> texture_pool;

	Reference<glare::PoolAllocator<WMFSampleInfo>> frame_info_allocator;

	glare::AtomicInt num_pending_reads;
};


#endif // _WIN32
