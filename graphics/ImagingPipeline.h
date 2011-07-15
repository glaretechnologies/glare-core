/*=====================================================================
ImagingPipeline.h
-----------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Jul 13 13:44:31 +0100 2011
=====================================================================*/
#pragma once

#include <vector>

#include "image.h"

#include "../indigo/PrintOutput.h"
#include "../indigo/RendererSettings.h"


class Camera;


namespace ImagingPipeline
{


class BufferedPrintOutput : public PrintOutput
{
public:
	virtual ~BufferedPrintOutput() { }

	virtual void print(const std::string& s) { msgs.push_back(s); }
	virtual void printStr(const std::string& s) { msgs.push_back(s); }

	std::vector<std::string> msgs;
};


const uint32 image_tile_size = 64;



void sumBuffers(const std::vector<Vec3f>& layer_scales, const std::vector<Image>& buffers, Image& buffer_out);


void doTonemapFullBuffer(
	const std::vector<Image>& layers,
	const std::vector<Vec3f>& layer_weights,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	Camera* camera,
	Image& temp_summed_buffer,
	Image& temp_AD_buffer,
	Image& ldr_buffer_out,
	bool image_buffer_in_XYZ);


void doTonemap(
	std::vector<Image>& per_thread_tile_buffers,
	const std::vector<Image>& layers,
	const std::vector<Vec3f>& layer_weights,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	Camera* camera,
	Image& temp_summed_buffer,
	Image& temp_AD_buffer,
	Image& ldr_buffer_out,
	bool XYZ_colourspace);


#ifdef BUILD_TESTS

void test();

#endif


}; // namespace ImagingPipeline
