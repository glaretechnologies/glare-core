/*=====================================================================
Direct3DUtils.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Direct3DUtils.h"


#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include <tracy/Tracy.hpp>
#include <cassert>
#ifdef _WIN32
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d11_4.h>
#include <wincodec.h>
#endif


#ifdef _WIN32


static inline void throwOnError(HRESULT hres)
{
	if(FAILED(hres))
		throw glare::Exception("Error: " + PlatformUtils::COMErrorString(hres));
}


void Direct3DUtils::createGPUDeviceAndMFDeviceManager(ComObHandle<ID3D11Device>& d3d_device_out, ComObHandle<IMFDXGIDeviceManager>& device_manager_out)
{
	ZoneScoped; // Tracy profiler

	// Try and pick the discrete Nvidia or AMD GPU if it exists.
	// Adapted from https://github.com/ValveSoftware/openvr/issues/539#issuecomment-306371490
	ComObHandle<IDXGIAdapter1> recommended_adapter;
	ComObHandle<IDXGIFactory1> factory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&factory.ptr));
	if(SUCCEEDED(hr))
	{
		ComObHandle<IDXGIAdapter1> adapter;
		UINT index = 0;
		while(factory->EnumAdapters1(index, &adapter.ptr) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			// conPrint("Adapter: " + StringUtils::PlatformToUTF8UnicodeEncoding(desc.Description) + ", LUID: {" + toString((uint32)desc.AdapterLuid.LowPart) + ", " + toString(desc.AdapterLuid.HighPart) + "}");

			if(desc.VendorId == 0x10de || desc.VendorId == 0x1002) // if vendor is Nvidia or AMD:
				recommended_adapter = adapter;

			adapter = ComObHandle<IDXGIAdapter1>(); // Make sure to close our adapter handle before we overrite it with EnumAdapters1.
			index++;
		}
	}

	const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

#ifndef NDEBUG // If in debug config:
	const UINT flags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#else
	const UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#endif

	hr = D3D11CreateDevice(
		recommended_adapter.ptr, // pAdapter
		recommended_adapter.ptr ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, // DriverType
		NULL, // Software rasteriser
		flags,
		levels, ARRAYSIZE(levels), 
		D3D11_SDK_VERSION, 
		&d3d_device_out.ptr, 
		NULL, // pFeatureLevel
		NULL // ppImmediateContext
	);
	if(!SUCCEEDED(hr))
		throw glare::Exception("D3D11CreateDevice failed: " + PlatformUtils::COMErrorString(hr));

	// Get ready for multi-threaded operation
	ComObHandle<ID3D11Multithread> multithreaded_device;
	if(!d3d_device_out.queryInterface(multithreaded_device))
		throw glare::Exception("failed to get ID3D11Multithread interace.");

	multithreaded_device->SetMultithreadProtected(TRUE);

	UINT reset_token;
	throwOnError(MFCreateDXGIDeviceManager(&reset_token, &device_manager_out.ptr));
	throwOnError(device_manager_out->ResetDevice(d3d_device_out.ptr, reset_token));
}


// For debugging: dump a texture to disk
// From https://github.com/Microsoft/graphics-driver-samples/blob/master/render-only-sample/rostest/util.cpp#L244
void Direct3DUtils::saveTextureToBmp(const std::string& filename, ID3D11Texture2D* texture)
{
	HRESULT hr;

	// First verify that we can map the texture
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);

	// translate texture format to WIC format. We support only BGRA and ARGB.
	GUID wicFormatGuid;
	switch (desc.Format) {
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		wicFormatGuid = GUID_WICPixelFormat32bppRGBA;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		wicFormatGuid = GUID_WICPixelFormat32bppBGRA;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		wicFormatGuid = GUID_WICPixelFormat32bppBGRA;
		break;
	default:
		throw glare::Exception("Unsupported DXGI_FORMAT: %d. Only RGBA and BGRA are supported.");
	}

	// Get the device context
	ComObHandle<ID3D11Device> d3dDevice;
	texture->GetDevice(&d3dDevice.ptr);
	ComObHandle<ID3D11DeviceContext> d3dContext;
	d3dDevice->GetImmediateContext(&d3dContext.ptr);

	// map the texture
	ComObHandle<ID3D11Texture2D> mappedTexture;
	D3D11_MAPPED_SUBRESOURCE mapInfo;
	//mapInfo.RowPitch;
	hr = d3dContext->Map(
		texture,
		0,  // Subresource
		D3D11_MAP_READ,
		0,  // MapFlags
		&mapInfo);

	if (FAILED(hr)) {
		// If we failed to map the texture, copy it to a staging resource
		if (hr == E_INVALIDARG) {
			D3D11_TEXTURE2D_DESC desc2;
			desc2.Width = desc.Width;
			desc2.Height = desc.Height;
			desc2.MipLevels = desc.MipLevels;
			desc2.ArraySize = desc.ArraySize;
			desc2.Format = desc.Format;
			desc2.SampleDesc = desc.SampleDesc;
			desc2.Usage = D3D11_USAGE_STAGING;
			desc2.BindFlags = 0;
			desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			desc2.MiscFlags = 0;

			ComObHandle<ID3D11Texture2D> stagingTexture;
			hr = d3dDevice->CreateTexture2D(&desc2, nullptr, &stagingTexture.ptr);
			if (FAILED(hr)) {
				throw glare::Exception("Failed to create staging texture");
			}

			// copy the texture to a staging resource
			d3dContext->CopyResource(stagingTexture.ptr, texture);

			// now, map the staging resource
			hr = d3dContext->Map(
				stagingTexture.ptr,
				0,
				D3D11_MAP_READ,
				0,
				&mapInfo);
			if (FAILED(hr)) {
				throw glare::Exception("Failed to map staging texture");
			}

			mappedTexture.ptr = std::move(stagingTexture.ptr); // NOTE: dodgy?
			stagingTexture.ptr = NULL;
		} else {
			throw glare::Exception("Failed to map texture.");
		}
	} else {
		mappedTexture.ptr = texture;
	}
	//auto unmapResource = Finally([&] {
	//	d3dContext->Unmap(mappedTexture.ptr, 0);
	//});

	ComObHandle<IWICImagingFactory> wicFactory;
	hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(wicFactory),
		reinterpret_cast<void**>(&wicFactory.ptr));
	if (FAILED(hr)) {
		throw glare::Exception("Failed to create instance of WICImagingFactory");
	}

	ComObHandle<IWICBitmapEncoder> wicEncoder;
	hr = wicFactory->CreateEncoder(
		GUID_ContainerFormatBmp,
		nullptr,
		&wicEncoder.ptr);
	if (FAILED(hr)) {
		throw glare::Exception("Failed to create BMP encoder");
	}

	ComObHandle<IWICStream> wicStream;
	hr = wicFactory->CreateStream(&wicStream.ptr);
	if (FAILED(hr)) {
		throw glare::Exception("Failed to create IWICStream");
	}

	hr = wicStream->InitializeFromFilename(StringUtils::UTF8ToPlatformUnicodeEncoding(filename).c_str(), GENERIC_WRITE);
	if (FAILED(hr)) {
		throw glare::Exception("Failed to initialize stream from file name");
	}

	hr = wicEncoder->Initialize(wicStream.ptr, WICBitmapEncoderNoCache);
	if (FAILED(hr)) {
		throw glare::Exception("Failed to initialize bitmap encoder");
	}

	// Encode and commit the frame
	{
		ComObHandle<IWICBitmapFrameEncode> frameEncode;
		wicEncoder->CreateNewFrame(&frameEncode.ptr, nullptr);
		if (FAILED(hr)) {
			throw glare::Exception("Failed to create IWICBitmapFrameEncode");
		}

		hr = frameEncode->Initialize(nullptr);
		if (FAILED(hr)) {
			throw glare::Exception("Failed to initialize IWICBitmapFrameEncode");
		}


		hr = frameEncode->SetPixelFormat(&wicFormatGuid);
		if (FAILED(hr)) {
			throw glare::Exception("SetPixelFormat(%s) failed.");
		}

		hr = frameEncode->SetSize(desc.Width, desc.Height);
		if (FAILED(hr)) {
			throw glare::Exception("SetSize(...) failed.");
		}

		hr = frameEncode->WritePixels(
			desc.Height,
			mapInfo.RowPitch,
			desc.Height * mapInfo.RowPitch,
			reinterpret_cast<BYTE*>(mapInfo.pData));
		if (FAILED(hr)) {
			throw glare::Exception("frameEncode->WritePixels(...) failed.");
		}

		hr = frameEncode->Commit();
		if (FAILED(hr)) {
			throw glare::Exception("Failed to commit frameEncode");
		}
	}

	hr = wicEncoder->Commit();
	if (FAILED(hr)) {
		throw glare::Exception("Failed to commit encoder");
	}

	d3dContext->Unmap(mappedTexture.ptr, 0);
}


ComObHandle<ID3D11Texture2D> Direct3DUtils::copyTextureToNewShareableTexture(const ComObHandle<ID3D11Device>& d3d_device, const ComObHandle<ID3D11Texture2D>& src_tex)
{
	ComObHandle<ID3D11DeviceContext> d3d_context;
	d3d_device->GetImmediateContext(&d3d_context.ptr);

	D3D11_TEXTURE2D_DESC desc;
	src_tex->GetDesc(&desc);

	//----------------- Create texture copy ---------------
	D3D11_TEXTURE2D_DESC your_desc = desc;
	//your_desc.MipLevels = 1;
	your_desc.Usage = D3D11_USAGE_DEFAULT /*D3D11_USAGE_STAGING*/;  // TEMP D3D11_USAGE_STAGING for map
	your_desc.CPUAccessFlags = 0; // D3D11_CPU_ACCESS_READ; // TEMP
	your_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;// | D3D11_BIND_RENDER_TARGET;

	// Share with a legacy (KMT) handle rather than an NT handle.  D3D11_RESOURCE_MISC_SHARED_NTHANDLE has to be combined with
	// D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, and we can't use a keyed mutex: the only other user of this texture is OpenGL, and AMD's drivers don't
	// expose GL_EXT_win32_keyed_mutex, so GL can't take it.  A keyed-mutex resource that only one side ever locks gives no synchronisation while
	// still being treated as shared-and-synchronised by the driver.  Ordering between the copy below and GL sampling the texture wants a shared
	// fence (ID3D11Fence imported into GL as a semaphore) instead.
	your_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	ComObHandle<ID3D11Texture2D> texture_copy;
	{
		ZoneScopedN("CreateTexture2D"); // Tracy profiler
		HRESULT hr = d3d_device->CreateTexture2D(&your_desc, nullptr, &texture_copy.ptr);
		if(!(SUCCEEDED(hr) && texture_copy))
			throw glare::Exception("Failed to copy texture: " + PlatformUtils::COMErrorString(hr));
	}

	{
		ZoneScopedN("CopyResource"); // Tracy profiler
		d3d_context->CopyResource(/*dest=*/texture_copy.ptr, /*source=*/src_tex.ptr); // Copy the texture

		// The flush is required, not an optimisation: a shared surface only shows the results of commands that have been submitted, and without a
		// keyed mutex (whose ReleaseSync submitted them for us) nothing else here does that.  A consumer that draws rarely - a browser page that
		// paints once - otherwise samples a texture that was never written, and sees black.
		d3d_context->Flush();
	}

	return texture_copy;
}


void Direct3DUtils::copyTextureToExistingShareableTexture(const ComObHandle<ID3D11Device>& d3d_device, const ComObHandle<ID3D11Texture2D>& src_tex, ComObHandle<ID3D11Texture2D>& dest_tex)
{
	ComObHandle<ID3D11DeviceContext> d3d_context;
	d3d_device->GetImmediateContext(&d3d_context.ptr);

	// NOTE: no keyed mutex is taken here, see the comment in copyTextureToNewShareableTexture().
	{
		ZoneScopedN("CopyResource"); // Tracy profiler
		d3d_context->CopyResource(/*dest=*/dest_tex.ptr, /*source=*/src_tex.ptr); // Copy the texture

		// Required so the other side of the shared texture sees the copy, see the comment in copyTextureToNewShareableTexture().
		d3d_context->Flush();
	}
}


HANDLE Direct3DUtils::getSharedHandleForTexture(ComObHandle<ID3D11Texture2D>& tex)
{
	ZoneScoped; // Tracy profiler

#ifndef NDEBUG
	D3D11_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);

	assert((desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED) != 0);
#endif

	ComObHandle<IDXGIResource> dxgi_resource = tex.getInterface<IDXGIResource>();

	// NOTE: this is a legacy (KMT) handle, not an NT handle.  It is owned by the resource, so it must not be closed, and it is only valid in this
	// process.  Import it into OpenGL with GL_HANDLE_TYPE_D3D11_IMAGE_KMT_EXT.
	HANDLE shared_handle = nullptr;
	HRESULT hr = dxgi_resource->GetSharedHandle(&shared_handle);
	if(!(SUCCEEDED(hr) && shared_handle))
		throw glare::Exception("Failed to get shared handle from texture: " + PlatformUtils::COMErrorString(hr));

	return shared_handle;
}


D3DVideoProcessor::D3DVideoProcessor(ComObHandle<ID3D11Device> device, uint32 input_width_, uint32 input_height_, uint32 output_width_, uint32 output_height_,
	bool bt_709, bool full_range_input)
:	d3d_device(device),
	input_width(input_width_), input_height(input_height_),
	output_width(output_width_), output_height(output_height_)
{
	ZoneScoped; // Tracy profiler

	if(!d3d_device.queryInterface(video_device))
		throw glare::Exception("Failed to get ID3D11VideoDevice interface.  (Was the device created with D3D11_CREATE_DEVICE_VIDEO_SUPPORT?)");

	ComObHandle<ID3D11DeviceContext> d3d_context;
	d3d_device->GetImmediateContext(&d3d_context.ptr);
	if(!d3d_context.queryInterface(video_context))
		throw glare::Exception("Failed to get ID3D11VideoContext interface.");

	D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
	content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	content_desc.InputWidth   = input_width;
	content_desc.InputHeight  = input_height;
	content_desc.OutputWidth  = output_width;
	content_desc.OutputHeight = output_height;
	// The frame rates just tell the driver how much time it has per frame, they don't pace anything: we do one blt per frame we present.
	content_desc.InputFrameRate.Numerator   = 60;  content_desc.InputFrameRate.Denominator   = 1;
	content_desc.OutputFrameRate.Numerator  = 60;  content_desc.OutputFrameRate.Denominator  = 1;
	content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

	HRESULT hr = video_device->CreateVideoProcessorEnumerator(&content_desc, &processor_enumerator.ptr);
	if(FAILED(hr))
		throw glare::Exception("CreateVideoProcessorEnumerator failed: " + PlatformUtils::COMErrorString(hr));

	// Check the hardware can write the format we want out of it, so a driver that can't says so here rather than at the first blt.
	UINT format_support = 0;
	hr = processor_enumerator->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM, &format_support);
	if(FAILED(hr) || ((format_support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) == 0))
		throw glare::Exception("The video processor can't output DXGI_FORMAT_B8G8R8A8_UNORM.");

	hr = video_device->CreateVideoProcessor(processor_enumerator.ptr, /*RateConversionIndex=*/0, &processor.ptr);
	if(FAILED(hr))
		throw glare::Exception("CreateVideoProcessor failed: " + PlatformUtils::COMErrorString(hr));

	// All of the state below is per-processor, not per-blt, so set it once here.

	// Turn off any denoising, sharpening, or frame-rate conversion the driver would otherwise apply by default.  We want the frame the decoder
	// produced, just in a different format, and those filters cost time and change the image.
	video_context->VideoProcessorSetStreamAutoProcessingMode(processor.ptr, /*stream index=*/0, /*Enable=*/FALSE);
	video_context->VideoProcessorSetStreamFrameFormat(processor.ptr, /*stream index=*/0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
	video_context->VideoProcessorSetStreamOutputRate(processor.ptr, /*stream index=*/0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, /*RepeatFrame=*/FALSE, /*pCustomRate=*/nullptr);

	// Take the top-left output_width * output_height of the input and put it in the same-sized region of the output: source and destination are the
	// same size, so this is a straight conversion with no scaling, and the coded padding h264 adds is left behind.
	const RECT src_rect  = { 0, 0, (LONG)output_width, (LONG)output_height };
	const RECT dest_rect = { 0, 0, (LONG)output_width, (LONG)output_height };
	video_context->VideoProcessorSetStreamSourceRect(processor.ptr, /*stream index=*/0, /*Enable=*/TRUE, &src_rect);
	video_context->VideoProcessorSetStreamDestRect  (processor.ptr, /*stream index=*/0, /*Enable=*/TRUE, &dest_rect);
	video_context->VideoProcessorSetOutputTargetRect(processor.ptr, /*Enable=*/FALSE, /*pRect=*/nullptr); // Write the whole target.

	// Tell the processor how to interpret the YUV coming in, and what we want the RGB going out to be.  This is the actual colour conversion, and
	// getting the matrix or the range wrong shows up as slightly washed out or over-contrasty video rather than as an error.
	D3D11_VIDEO_PROCESSOR_COLOR_SPACE input_colour_space = {};
	input_colour_space.Usage        = 0u; // 0 = playback (favour quality), 1 = video processing.
	input_colour_space.RGB_Range    = 0u; // 0 = full range (0-255).  Applies to the RGB side of the conversion.
	input_colour_space.YCbCr_Matrix = bt_709 ? 1u : 0u; // 0 = BT.601, 1 = BT.709.
	input_colour_space.YCbCr_xvYCC  = 0u;
	input_colour_space.Nominal_Range = full_range_input ? (UINT)D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255 : (UINT)D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
	video_context->VideoProcessorSetStreamColorSpace(processor.ptr, /*stream index=*/0, &input_colour_space);

	D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_colour_space = input_colour_space;
	output_colour_space.Nominal_Range = (UINT)D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255; // We want full-range RGB out: black at 0, white at 255.
	video_context->VideoProcessorSetOutputColorSpace(processor.ptr, &output_colour_space);
}


D3DVideoProcessor::~D3DVideoProcessor()
{
	// The cached views hold references to their textures, so drop them before the processor and device go.
	input_views.clear();
	output_views.clear();
}


ComObHandle<ID3D11Texture2D> D3DVideoProcessor::createOutputTexture()
{
	ZoneScoped; // Tracy profiler

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width  = output_width;
	desc.Height = output_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // RENDER_TARGET is required to make a video processor output view of it.
	desc.CPUAccessFlags = 0;

	// Share with a legacy (KMT) handle rather than an NT handle.  D3D11_RESOURCE_MISC_SHARED_NTHANDLE has to be combined with
	// D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, and we can't use a keyed mutex: the only other user of this texture is OpenGL, and AMD's drivers don't
	// expose GL_EXT_win32_keyed_mutex, so GL can't take it.  See the comment in copyTextureToNewShareableTexture().
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	ComObHandle<ID3D11Texture2D> tex;
	const HRESULT hr = d3d_device->CreateTexture2D(&desc, nullptr, &tex.ptr);
	if(FAILED(hr) || !tex)
		throw glare::Exception("Failed to create " + toString(output_width) + "x" + toString(output_height) + " BGRA video output texture: " + PlatformUtils::COMErrorString(hr));

	return tex;
}


ComObHandle<ID3D11VideoProcessorInputView> D3DVideoProcessor::getInputView(ID3D11Texture2D* src_tex)
{
	const auto res = input_views.find(src_tex);
	if(res != input_views.end())
		return res->second;

	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC desc = {};
	desc.FourCC = 0; // 0 = use the format the texture was created with.
	desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;
	desc.Texture2D.ArraySlice = 0;

	ComObHandle<ID3D11VideoProcessorInputView> view;
	const HRESULT hr = video_device->CreateVideoProcessorInputView(src_tex, processor_enumerator.ptr, &desc, &view.ptr);
	if(FAILED(hr))
	{
		D3D11_TEXTURE2D_DESC tex_desc;
		src_tex->GetDesc(&tex_desc);
		throw glare::Exception("CreateVideoProcessorInputView failed for a " + toString(tex_desc.Width) + "x" + toString(tex_desc.Height) + " texture of DXGI format " +
			toString((int)tex_desc.Format) + ": " + PlatformUtils::COMErrorString(hr));
	}

	input_views[src_tex] = view;
	return view;
}


ComObHandle<ID3D11VideoProcessorOutputView> D3DVideoProcessor::getOutputView(ID3D11Texture2D* dest_tex)
{
	const auto res = output_views.find(dest_tex);
	if(res != output_views.end())
		return res->second;

	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC desc = {};
	desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;

	ComObHandle<ID3D11VideoProcessorOutputView> view;
	const HRESULT hr = video_device->CreateVideoProcessorOutputView(dest_tex, processor_enumerator.ptr, &desc, &view.ptr);
	if(FAILED(hr))
		throw glare::Exception("CreateVideoProcessorOutputView failed: " + PlatformUtils::COMErrorString(hr));

	output_views[dest_tex] = view;
	return view;
}


void D3DVideoProcessor::convert(const ComObHandle<ID3D11Texture2D>& src_tex, const ComObHandle<ID3D11Texture2D>& dest_tex)
{
	ZoneScoped; // Tracy profiler

	const ComObHandle<ID3D11VideoProcessorInputView>  input_view  = getInputView (src_tex.ptr);
	const ComObHandle<ID3D11VideoProcessorOutputView> output_view = getOutputView(dest_tex.ptr);

	D3D11_VIDEO_PROCESSOR_STREAM stream = {};
	stream.Enable = TRUE;
	stream.OutputIndex = 0;
	stream.InputFrameOrField = 0;
	stream.PastFrames = 0;   // We hand it one frame at a time: there is no deinterlacing or frame interpolation to reference neighbours for.
	stream.FutureFrames = 0;
	stream.pInputSurface = input_view.ptr;

	const HRESULT hr = video_context->VideoProcessorBlt(processor.ptr, output_view.ptr, /*OutputFrame=*/0, /*StreamCount=*/1, &stream);
	if(FAILED(hr))
		throw glare::Exception("VideoProcessorBlt failed: " + PlatformUtils::COMErrorString(hr));
}


MFScopedDeviceLock::MFScopedDeviceLock(IMFDXGIDeviceManager* device_manager_)
:	device_manager(device_manager_), device_handle(nullptr)
{
	ZoneScoped; // Tracy profiler

	HANDLE handle = nullptr;
	if(FAILED(device_manager->OpenDeviceHandle(&handle)))
		return;

	// fBlock=FALSE: fail with DXVA2_E_VIDEO_DEVICE_LOCKED instead of waiting if Media Foundation is currently using the device.
	if(FAILED(device_manager->LockDevice(handle, IID_PPV_ARGS(&device.ptr), /*fBlock=*/FALSE)))
	{
		device_manager->CloseDeviceHandle(handle);
		return;
	}

	device_handle = handle;
}


MFScopedDeviceLock::~MFScopedDeviceLock()
{
	if(device_handle)
	{
		device.release();

		device_manager->UnlockDevice(device_handle, /*fSaveState=*/FALSE);
		device_manager->CloseDeviceHandle(device_handle);
	}
}


#endif // #ifdef _WIN32
