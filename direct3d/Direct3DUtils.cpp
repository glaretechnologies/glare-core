/*=====================================================================
Direct3DUtils.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Direct3DUtils.h"


#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include <tracy/Tracy.hpp>
#include <cassert>
#ifdef _WIN32
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <mfidl.h>
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

	const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

#ifndef NDEBUG // If in debug config:
	const UINT flags = D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#else
	const UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#endif
	HRESULT hr = D3D11CreateDevice(
		NULL, // pAdapter - use the default adaptor
		D3D_DRIVER_TYPE_HARDWARE, // DriverType
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
	your_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

	ComObHandle<ID3D11Texture2D> texture_copy;
	HRESULT hr = d3d_device->CreateTexture2D(&your_desc, nullptr, &texture_copy.ptr);
	if(!(SUCCEEDED(hr) && texture_copy))
		throw glare::Exception("Failed to copy texture: " + PlatformUtils::COMErrorString(hr));

	ComObHandle<IDXGIKeyedMutex> texture_copy_mutex = texture_copy.getInterface<IDXGIKeyedMutex>();
	texture_copy_mutex->AcquireSync(0, INFINITE);

	d3d_context->CopyResource(/*dest=*/texture_copy.ptr, /*source=*/src_tex.ptr); // Copy the texture
	//d3d_context->Flush();

	texture_copy_mutex->ReleaseSync(0);

	return texture_copy;
}


void Direct3DUtils::copyTextureToExistingShareableTexture(const ComObHandle<ID3D11Device>& d3d_device, const ComObHandle<ID3D11Texture2D>& src_tex, ComObHandle<ID3D11Texture2D>& dest_tex)
{
	ComObHandle<ID3D11DeviceContext> d3d_context;
	d3d_device->GetImmediateContext(&d3d_context.ptr);

	ComObHandle<IDXGIKeyedMutex> texture_copy_mutex = dest_tex.getInterface<IDXGIKeyedMutex>();
	texture_copy_mutex->AcquireSync(0, INFINITE);

	d3d_context->CopyResource(/*dest=*/dest_tex.ptr, /*source=*/src_tex.ptr); // Copy the texture
	//d3d_context->Flush();

	texture_copy_mutex->ReleaseSync(0);
}


HANDLE Direct3DUtils::getSharedHandleForTexture(ComObHandle<ID3D11Texture2D>& tex)
{
#ifndef NDEBUG
	D3D11_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);

	assert((desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) != 0);
	assert((desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) != 0);
#endif

	ComObHandle<IDXGIResource1> dxgi_resource = tex.getInterface<IDXGIResource1>();

	HANDLE shared_handle = nullptr;
	HRESULT hr = dxgi_resource->CreateSharedHandle(/*security attributes=*/nullptr,
		DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, // access
		nullptr, // name
		&shared_handle
	);
	if(!(SUCCEEDED(hr) && shared_handle))
		throw glare::Exception("Failed to get shared handle from texture: " + PlatformUtils::COMErrorString(hr));

	return shared_handle;
}


#endif // #ifdef _WIN32
