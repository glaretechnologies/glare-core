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

=====================================================================*/
class Direct3DUtils
{
public:
#if defined(_WIN32)
	static void createGPUDeviceAndMFDeviceManager(ComObHandle<ID3D11Device>& device_out, ComObHandle<IMFDXGIDeviceManager>& device_manager_out);


	// For debugging: dump a texture to disk
	static void saveTextureToBmp(const std::string& filename, ID3D11Texture2D* texture);
#endif
};
