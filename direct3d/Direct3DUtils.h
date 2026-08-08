/*=====================================================================
Direct3DUtils.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/ComObHandle.h"
#include "../utils/Platform.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <map>
struct ID3D11Device;
struct ID3D11Texture2D;
struct IMFDXGIDeviceManager;
struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoProcessor;
struct ID3D11VideoProcessorEnumerator;
struct ID3D11VideoProcessorInputView;
struct ID3D11VideoProcessorOutputView;


/*=====================================================================
Direct3DUtils
-------------
Requires linking with
d3d11.lib
DXGI.lib
=====================================================================*/
class Direct3DUtils
{
public:
#if defined(_WIN32)
	static void createGPUDeviceAndMFDeviceManager(ComObHandle<ID3D11Device>& device_out, ComObHandle<IMFDXGIDeviceManager>& device_manager_out);


	// For debugging: dump a texture to disk
	static void saveTextureToBmp(const std::string& filename, ID3D11Texture2D* texture);

	static ComObHandle<ID3D11Texture2D> copyTextureToNewShareableTexture(const ComObHandle<ID3D11Device>& d3d_device, const ComObHandle<ID3D11Texture2D>& tex);
	
	static void copyTextureToExistingShareableTexture(const ComObHandle<ID3D11Device>& d3d_device, const ComObHandle<ID3D11Texture2D>& src_tex, ComObHandle<ID3D11Texture2D>& dest_tex);

	// returns HANDLE
	static void* getSharedHandleForTexture(ComObHandle<ID3D11Texture2D>& tex);
#endif
};


#if defined(_WIN32)
/*=====================================================================
MFScopedDeviceLock
------------------
Takes the lock on a D3D11 device that is shared with Media Foundation, without blocking.

A device shared with Media Foundation has to be multithread-protected, so any D3D11 call on it waits on the device's critical section.  Media
Foundation holds that section for hundreds of milliseconds while it builds a decoder (in IMFSourceReader::SetCurrentMediaType), which means a
thread that just wants to copy a texture can be stalled for most of a second.  Going through the device manager instead lets us find out that the
device is busy rather than waiting for it, so the render thread can skip its work and try again next frame.

Check isLocked() before using getDevice().
=====================================================================*/
class MFScopedDeviceLock
{
public:
	MFScopedDeviceLock(IMFDXGIDeviceManager* device_manager);
	~MFScopedDeviceLock();

	bool isLocked() const { return device_handle != nullptr; }

	const ComObHandle<ID3D11Device>& getDevice() const { return device; } // Only valid if isLocked().

private:
	GLARE_DISABLE_COPY(MFScopedDeviceLock)

	IMFDXGIDeviceManager* device_manager;
	void* device_handle; // HANDLE, null if we didn't get the lock.
	ComObHandle<ID3D11Device> device;
};


/*=====================================================================
D3DVideoProcessor
-----------------
Converts video frames from the format the decoder produces them in (NV12: 8-bit luma plus half-resolution interleaved chroma) to the BGRA that
OpenGL can sample, using the GPU's video processor.

The video processor is fixed-function hardware that sits next to the decoder and exists to do exactly this, so the conversion costs no shader
code of ours, no render target of ours, and no round trip through system memory.  The alternative - having Media Foundation output RGB32 - makes
it allocate a 4-bytes-per-pixel surface for every sample in flight rather than the 1.5 of NV12, and it is that burst of allocation at playback
start that stalls the renderer.

Make one per video being played: it is built for a particular pair of input and output sizes, and it caches the views it creates for the textures
it is handed.

Requires the D3D11 device to have been created with D3D11_CREATE_DEVICE_VIDEO_SUPPORT.
=====================================================================*/
class D3DVideoProcessor : public RefCounted
{
public:
	// input_width/height: the size of the textures coming out of the decoder.  For h264 these are padded up to a multiple of 16.
	// output_width/height: the size of the picture inside them, which is what we want out.  The top-left output_width * output_height region of the
	//   input is taken, at 1:1, so the padding is trimmed off rather than scaled into the output.
	// bt_709: use the BT.709 YUV matrix (HD) rather than BT.601 (SD).
	// full_range_input: the input uses the full 0-255 range rather than the usual 16-235.
	// Throws glare::Exception on failure.
	D3DVideoProcessor(ComObHandle<ID3D11Device> device, uint32 input_width, uint32 input_height, uint32 output_width, uint32 output_height,
		bool bt_709, bool full_range_input);
	~D3DVideoProcessor();

	// Create a BGRA texture of the output size, to be used as the destination of convert().  It is shareable, so it can be imported into OpenGL.
	// Throws glare::Exception on failure.
	ComObHandle<ID3D11Texture2D> createOutputTexture();

	// Convert src_tex (e.g. an NV12 texture from the video decoder) into dest_tex, which should have come from createOutputTexture().
	// Throws glare::Exception on failure.
	void convert(const ComObHandle<ID3D11Texture2D>& src_tex, const ComObHandle<ID3D11Texture2D>& dest_tex);

private:
	GLARE_DISABLE_COPY(D3DVideoProcessor)

	ComObHandle<ID3D11VideoProcessorInputView>  getInputView (ID3D11Texture2D* src_tex);
	ComObHandle<ID3D11VideoProcessorOutputView> getOutputView(ID3D11Texture2D* dest_tex);

	ComObHandle<ID3D11Device> d3d_device;
	ComObHandle<ID3D11VideoDevice> video_device;
	ComObHandle<ID3D11VideoContext> video_context;
	ComObHandle<ID3D11VideoProcessorEnumerator> processor_enumerator;
	ComObHandle<ID3D11VideoProcessor> processor;

	// Making a view is a driver allocation, and the same few textures come back around frame after frame, so keep the views we make.  A view holds a
	// reference to its texture, which is fine: the textures come from the video reader's pool, which outlives this object.
	std::map<ID3D11Texture2D*, ComObHandle<ID3D11VideoProcessorInputView>>  input_views;
	std::map<ID3D11Texture2D*, ComObHandle<ID3D11VideoProcessorOutputView>> output_views;

	uint32 input_width, input_height;
	uint32 output_width, output_height;
};
#endif
