/*=====================================================================
EmbreeDeviceHandle.h
--------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once

/*
* So suppress this warning with #pragma warning:

d:\programming\embree\embree_3.12.1_install-vs2019\include\embree3\rtcore_common.h(234,1): warning C4324: 'RTCPointQuery': structure was padded due to alignment specifier

We'll do this in this separate file for prettiness.
*/

#ifdef _MSC_VER
#pragma warning(push, 0) // Disable warnings
#endif

#include <embree3/rtcore.h>

#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


using namespace EmbreeGlare;


class EmbreeDeviceHandle
{
public:
	EmbreeDeviceHandle() : embree_device(NULL) {}
	EmbreeDeviceHandle(RTCDeviceTy* embree_device_) : embree_device(embree_device_) {}
	~EmbreeDeviceHandle()
	{
		if(embree_device)
			rtcReleaseDevice(embree_device);
	}

	RTCDeviceTy* ptr() { return embree_device; }

private:
	EmbreeDeviceHandle(const EmbreeDeviceHandle& );
	EmbreeDeviceHandle& operator = (const EmbreeDeviceHandle& );
	
	RTCDeviceTy* embree_device;
};
