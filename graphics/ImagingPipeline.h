/*=====================================================================
ImagingPipeline.h
-----------------
Copyright Glare Technologies Limited 2016 -
Generated at Wed Jul 13 13:44:31 +0100 2011
=====================================================================*/
#pragma once



#include "../indigo/RendererSettings.h"
#include "../indigo/RenderChannels.h"
#include <vector>


class PostProDiffraction;
class Image4f;
namespace Indigo { class TaskManager; }


namespace ImagingPipeline
{


/*
Adds together the weighted pixel values from each light layer.
Each layer is weighted by layer_scales.
Takes the alpha value from render_channels.alpha (and scales it by image_scale) if it is valid, otherwise uses alpha 1.
Writes the output to summed_buffer_out.
Multithreaded using task manager.
*/
void sumLightLayers(
	const std::vector<Vec3f>& layer_scales, 
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	const RenderChannels& render_channels, // Input image data
	Image4f& summed_buffer_out, 
	Indigo::TaskManager& task_manager
);


/*
Tonemaps some input image data, stored in render_channels, to some output image data, stored in ldr_buffer_out.
Also does downsizing of the supersampled internal buffer to the output image resolution.

The input data may be in XYZ colour space (as is the case for the usual rendering), or linear sRGB space (the case for some loaded images).

The output data will be stored as floating point data in ldr_buffer_out.
The output data will be in linear sRGB colour space.
The output data components will be in the range [0, 1].
*/
void doTonemap(
	std::vector<Image4f>& per_thread_tile_buffers, // Working memory
	const RenderChannels& render_channels, // Input image data
	const std::vector<Vec3f>& layer_weights, // Layer weights, with num samples scale factor folded in.
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	const Reference<PostProDiffraction>& post_pro_diffraction,
	Image4f& temp_summed_buffer, // Working memory
	Image4f& temp_AD_buffer, // Working memory
	Image4f& ldr_buffer_out, // Output image, has alpha channel.
	bool XYZ_colourspace, // Are the input layers in XYZ colour space?
	int margin_ssf1, // Margin width (for just one side), in pixels, at ssf 1.  This may be zero for loaded LDR images. (PNGs etc..)
	Indigo::TaskManager& task_manager
);


/*
Converts some tonemapped image data to non-linear sRGB space.
Does a few things in preperation for conversion to an 8-bit output image format, 
such as dithering and gamma correction.
Input colour space is linear sRGB
Output colour space is non-linear sRGB with the supplied gamma.
We also assume the output values are not premultiplied alpha.
*/
void toNonLinearSpace(
	Indigo::TaskManager& task_manager,
	const RendererSettings& renderer_settings,
	Image4f& ldr_buffer_in_out // Input and output image, has alpha channel.
);


}; // namespace ImagingPipeline
