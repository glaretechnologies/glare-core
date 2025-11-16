/*=====================================================================
Direct3DUtils.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/ComObHandle.h"
#include <string>
struct ID3D11Device;
struct ID3D11Texture2D;
struct IMFDXGIDeviceManager;


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
