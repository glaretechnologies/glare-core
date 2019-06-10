/*=====================================================================
ImagingPipeline.cpp
-------------------
Copyright Glare Technologies Limited 2018 -
Generated at Wed Jul 13 13:44:31 +0100 2011
=====================================================================*/
#include "ImagingPipeline.h"


#include "../indigo/ColourSpaceConverter.h"
#include "../indigo/ToneMapper.h"
#include "../indigo/ReinhardToneMapper.h"
#include "../indigo/LinearToneMapper.h"
#include "../indigo/PostProDiffraction.h"
#include "../indigo/SingleFreq.h"
#include "../graphics/ImageFilter.h"
#include "../graphics/Image4f.h"
#include "../graphics/bitmap.h"
#include "../graphics/SRGBUtils.h"
#include "../maths/mathstypes.h"
#include "../maths/Vec4f.h"
#include "../maths/Matrix4f.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#include "../utils/Timer.h"
#include "../utils/BufferedPrintOutput.h"
#include "../utils/ProfilerStore.h"
#include "../utils/SmallArray.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include <algorithm>


/*

We tonemap before downsizing to get rid of aliasing along high contrast edges.
Just clamping before downsizing also works quite well, seems not quite as good as tonemapping though.
Very similar for linear, looks a bit more aliased for camera tone mapping.
This is maybe due to the shape of the camera curve (slowly approaches max value).



For the tonemapping methods that naturally map to a non-linear space (camera and filmic), don't transform to linear then back to non-linear in ImagingPipeline::toNonLinearSpace().
Instead, transform to a 'near-linear' space by raising to the power of 2.  A near linear space is needed for best downsizing results.  This can be undone later with a sqrt call.
This is good because it removes some very expensive pow() calls.


A comparison of downsize-then-apply-gamma vs apply-gamma-then-downsize:
=======================================================================
Suppose a = 1/gamma

High contrast edges
====================
Suppose we have a black pixel (value 0) besides a white pixel (value 1):

Gamma correct then average(downsize):
------------------------------------
   ( 0.0^a + 1.0^a ) * 0.5
=  ( 0.0   + 1.0   ) * 0.5
=  0.5

Average then gamma correct:
----------------------------
=  ((0.0 + 1.0) * 0.5) ^ a

= (1.0 * 0.5) ^ a
= 0.5 ^ a


If we want the resulting pixel to be perceptually half-grey, then 'Average then gamma correct' is the correct option.


No contrast (flat colour)
========================

Gamma correct then average(downsize):
------------------------------------
   ( 0.6^a + 0.6^a ) * 0.5
=  ( 2*0.6^a ) * 0.5
=  0.6^a

Average then gamma correct:
----------------------------
=  ((0.6 + 0.6) * 0.5) ^ a

= 0.6 ^ a

In this case the result is the same either way.



Notes on alpha output pre-multiplied vs non-premultiplied
=========================================================
"PNG uses "unassociated" or "non-premultiplied" alpha so that images with separate transparency masks can be stored losslessly" - https://www.w3.org/TR/PNG-Rationale.html
"by convention, OpenEXR images are "premultiplied" - the color channel values are already matted against black using the alpha channel" - http://www.openexr.com/photoshop_plugin.html



Notes on alpha blending in non-linear colour spaces (e.g. sRGB)
===============================================================
Let background colour = B

Foreground colour = A_linear
foreground alpha = alpha_linear

Let
A_final = toSRGB(A_linear) / alpha_linear
alpha_final = alpha_linear

Blending with background

C = A_final * alpha_final  +  B_sRGB * (1 - alpha_final)

C = [toSRGB(A_linear) / alpha_linear] * alpha_linear + B_sRGB * (1 - alpha_linear)

C = toSRGB(A_linear)  +  B_sRGB * (1 - alpha_linear)										[1]



White foreground, black background (as in alpha_motion_blur_test3.igs)
--------------------------------------------------------
A_linear = 0.5, alpha_linear = 0.5
B_sRGB = 0
Desired C: toSRGB(0.5)


C = A_final * alpha_final  +  B_sRGB * (1 - alpha_final)
C = toSRGB(A_linear)  +  B_sRGB * (1 - alpha_linear)						[1]
C = toSRGB(0.5)       +  0
  = toSRGB(0.5)										[correct, want resulting blended colour to be same as for gradient]


Black foreground, white background (as in alpha_motion_blur_test3.igs)
--------------------------------------------------------
A_linear = 0.0, alpha_linear = 0.5
B_linear = 1
Desired C: toSRGB(0.5)


C = A_final * alpha_final  +  B_sRGB * (1 - alpha_final)
C = toSRGB(A_linear)  +  B_sRGB * (1 - alpha_linear)						[1]
  = 0                 +  toSRGB(1) * (1 - 0.5)
  =                      1 * 0.5
  = 0.5										[Wrong, probably want toSRGB(0.5)]




-----------------------------------------

For a white foreground, no alpha, A_linear = 0.5, we will have A_sRGB ~= 0.5^(1/2.2) ~= 0.75.
This is the case for half-way along a linear intensity ramp.

Take white background (B_sRGB = 1), and black foreground object, with some alpha value (alpha_final).
Blended colour C will be
C = B_sRGB * (1 - alpha_final)
C = (1 - alpha_final)
In the case where alpha = 0.5 (half way along linear transparency 'ramp')
C = 0.5.
This is not what we want, it should be ~0.75 as for the non-alpha case.

To acheive this however, the alpha value would somehow need to take into account the background colour, which won't be known when storing the foreground colour + alpha.
Basically it makes no sense to alpha blend with non-linear values, as if they were linear.
*/


namespace ImagingPipeline
{


const uint32 image_tile_size = 64;


// Does the pixel at (x, y) lie in one or more of the given render regions?
inline static bool pixelIsInARegion(ptrdiff_t x, ptrdiff_t y, const SmallArray<Rect2i, 8>& regions)
{
	const Vec2i v((int)x, (int)y);
	for(size_t i=0; i<regions.size(); ++i)
		if(regions[i].inHalfClosedRectangle(v))
			return true;
	return false;
}


/*
sumLightLayers
--------------
Adds together the weighted pixel values from each light layer.
Each layer is weighted by layer_scales.
Then overwrites with any render region data.
Takes the alpha value from render_channels.alpha (and scales it by image_scale) if it is valid, otherwise uses alpha 1.
Writes the output to summed_buffer_out.
Multithreaded using task manager.
*/
struct SumBuffersTaskClosure
{
	SumBuffersTaskClosure(const ArrayRef<Vec3f>& layer_scales_, float image_scale_, float region_image_scale_, const RenderChannels& render_channels_, Image4f& buffer_out_)
		: layer_scales(layer_scales_), image_scale(image_scale_), region_image_scale(region_image_scale_), render_channels(render_channels_), buffer_out(buffer_out_) {}

	const ArrayRef<Vec3f>& layer_scales;
	float image_scale;
	float region_image_scale;
	const RenderChannels& render_channels; // Input image data
	Image4f& buffer_out;
	int margin_ssf1;
	int ssf;
	bool zero_alpha_outside_region;
	const ArrayRef<RenderRegion>* render_regions;
	float region_alpha_bias;
	bool render_foreground_alpha;
};


class SumBuffersTask : public Indigo::Task
{
public:
	SumBuffersTask(const SumBuffersTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin(begin_), end(end_) {}

	virtual void run(size_t thread_index)
	{
		// Build layer weights.  Layer weight = light layer value * image_scale.
		SmallArray<Vec3f, 32> layer_weights       (closure.render_channels.layers.size());
		SmallArray<Vec3f, 32> region_layer_weights(closure.render_channels.layers.size());
		
		for(size_t i=0; i<closure.render_channels.layers.size(); ++i)
		{
			layer_weights[i]        = closure.layer_scales[i] * closure.image_scale;
			region_layer_weights[i] = closure.layer_scales[i] * closure.region_image_scale;
		}

		const ptrdiff_t rr_margin   = 1; // Pixels near the edge of the RR may not be fully bright due to splat filter, so we need a margin.

		const ArrayRef<RenderRegion>& render_regions = *closure.render_regions;
		SmallArray<Rect2i, 8> regions(render_regions.size()); // Render regions bounds in intermediate pixel coords
		for(size_t i=0; i<render_regions.size(); ++i)
			regions[i] = Rect2i(Vec2i(	(render_regions[i].x1 + closure.margin_ssf1) * closure.ssf + rr_margin,
										(render_regions[i].y1 + closure.margin_ssf1) * closure.ssf + rr_margin),
								Vec2i(	(render_regions[i].x2 + closure.margin_ssf1) * closure.ssf - rr_margin,
										(render_regions[i].y2 + closure.margin_ssf1) * closure.ssf - rr_margin));

		const bool render_foreground_alpha = closure.render_foreground_alpha;
		const int num_layers = (int)closure.render_channels.layers.size();
		const bool render_region_enabled = closure.render_channels.target_region_layers;

		const glare::AllocatorVector<float, 16>& front_data = closure.render_channels.front_data;
		const glare::AllocatorVector<float, 16>& region_data = closure.render_channels.region_data;
		const size_t stride = closure.render_channels.stride;
		const size_t alpha_offset = (size_t)closure.render_channels.alpha.offset;

		// Artificially bump up the alpha value a little, so that monte-carlo noise doesn't make a totally opaque object partially transparent.
		// we will use image_scale for this, as it naturally reduces to near zero as the render converges.
		// For region renders, the alpha value will be lower than normal, since region samples are weighted by the normed_image_rect_area.  So in this case the alpha_bias should be smaller.
		// (we will use alpha_bias = normed_image_rect_area)
		const float alpha_bias_factor        = 1.f + closure.image_scale;
		const float region_alpha_bias_factor = 1.f + closure.region_alpha_bias;
		const ptrdiff_t main_layer_num_components = closure.render_channels.layers[0].num_components; // This will be 4 for GPU rendering as alpha is interleaved.

		for(size_t i = begin; i < end; ++i)
		{
			Colour4f sum(0.0f);
			for(int z = 0; z < num_layers; ++z)
			{
				const Vec3f& scale = layer_weights[z];
				sum[0] += front_data[i * stride + z * main_layer_num_components + 0] * scale.x;
				sum[1] += front_data[i * stride + z * main_layer_num_components + 1] * scale.y;
				sum[2] += front_data[i * stride + z * main_layer_num_components + 2] * scale.z;
			}

			// Get alpha from alpha channel if it exists
			sum.x[3] = render_foreground_alpha ? (front_data[i * stride + alpha_offset] * alpha_bias_factor * closure.image_scale) : 1.f;

			if(render_region_enabled)
			{
				// If this pixel lies in a render region, set the pixel value to the value in the render region layer.
				const size_t x = i % closure.render_channels.getWidth();
				const size_t y = i / closure.render_channels.getWidth();

				if(pixelIsInARegion((ptrdiff_t)x, (ptrdiff_t)y, regions))
				{
					sum = Colour4f(0.f);
					for(ptrdiff_t z = 0; z < num_layers; ++z)
					{
						const Vec3f& scale = region_layer_weights[z];
						sum[0] += region_data[i * stride + z * main_layer_num_components + 0] * scale.x;
						sum[1] += region_data[i * stride + z * main_layer_num_components + 1] * scale.y;
						sum[2] += region_data[i * stride + z * main_layer_num_components + 2] * scale.z;
					}

					// Get alpha from (region) alpha channel if it exists
					sum.x[3] = render_foreground_alpha ? (region_data[i * stride + alpha_offset] * region_alpha_bias_factor * closure.region_image_scale) : 1.f;
				}
				else
				{
					// Zero out pixels not in the render region, if zero_alpha_outside_region is enabled, or if there are no samples on the main layers.
					if(closure.zero_alpha_outside_region || (closure.image_scale == 0.0))
						sum = Colour4f(0.f);
				}
			}

			closure.buffer_out.getPixel(i) = sum;
		}
	}

	const SumBuffersTaskClosure& closure;
	size_t begin, end;
};


void sumLightLayers(
	const ArrayRef<Vec3f>& layer_scales, // Light layer weights.
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	float region_image_scale,
	const RenderChannels& render_channels, // Input image data
	const ArrayRef<RenderRegion>& render_regions,
	int margin_ssf1,
	int ssf,
	bool zero_alpha_outside_region,
	float region_alpha_bias,
	bool render_foreground_alpha,
	Image4f& summed_buffer_out, 
	Indigo::TaskManager& task_manager
) 
{
	summed_buffer_out.resizeNoCopy(render_channels.getWidth(), render_channels.getHeight());

	SumBuffersTaskClosure closure(layer_scales, image_scale, region_image_scale, render_channels, summed_buffer_out);
	closure.render_regions = &render_regions;
	closure.margin_ssf1 = margin_ssf1;
	closure.ssf = ssf;
	closure.zero_alpha_outside_region = zero_alpha_outside_region;
	closure.region_alpha_bias = region_alpha_bias;
	closure.render_foreground_alpha = render_foreground_alpha;
	
	const size_t num_pixels = render_channels.getWidth() * render_channels.getHeight();
	task_manager.runParallelForTasks<SumBuffersTask, SumBuffersTaskClosure>(closure, 0, num_pixels);
}


struct ToneMapTaskClosure
{
	const ToneMapper* tone_mapper;
	const ToneMapperParams* tonemap_params;
	Image4f* temp_summed_buffer;
	int final_xres, final_yres, x_tiles;
};


class ToneMapTask : public Indigo::Task
{
public:
	ToneMapTask(const ToneMapTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		for(int tile = begin; tile < end; ++tile)
		{
			// Get the final image tile bounds for the tile index
			const int tile_x = tile % closure.x_tiles;
			const int tile_y = tile / closure.x_tiles;
			const int x_min  = tile_x * image_tile_size, x_max = std::min<int>(closure.final_xres, (tile_x + 1) * image_tile_size);
			const int y_min  = tile_y * image_tile_size, y_max = std::min<int>(closure.final_yres, (tile_y + 1) * image_tile_size);

			closure.tone_mapper->toneMapSubImage(*closure.tonemap_params, *closure.temp_summed_buffer, x_min, y_min, x_max, y_max);
		}
	}

	const ToneMapTaskClosure& closure;
	int begin, end;
};


inline static float averageXYZVal(const Colour4f& c)
{
	return (c.x[0] + c.x[1] + c.x[2]) * (1.0f / 3.0f);
}


inline static uint32_t pixelHash(uint32_t x)
{
	x  = (x ^ 12345391u) * 2654435769u;
	x ^= (x << 6) ^ (x >> 26);
	x *= 2654435769u;
	x += (x << 5) ^ (x >> 12);

	return x;
}


// NOTE: Do we want to dither alpha?
inline Colour4f ditherPixel(const Colour4f& c, ptrdiff_t pixel_i)
{
	const float ur = pixelHash((uint32)pixel_i) * Maths::uInt32ToUnitFloatScale();
	return c + Colour4f(-0.5f + ur, -0.5f + ur, -0.5f + ur, 0) * (1.0f / 255.0f);
}


struct CurveData
{
	const static int table_vals = 64;
	float colour_curve_data[(table_vals + 3) * 4]; // 32 table values plus min, max, scale for 5 (overall+RGB) curves

	static float curveTableEval(const float t, const float t_min, const float t_max, const float t_scl, float const* const table)
	{
		if(t <= t_min) return table[0];
		if(t >= t_max) return table[table_vals - 1];

		const float table_x = (t - t_min) * t_scl;
		const int table_i = (int)table_x;
		const float f = table_x - table_i;

		const float v0 = table[table_i + 0];
		const float v1 = table[table_i + 1];
		return v0 + (v1 - v0) * f;
	}
};


// Tonemap HDR image to LDR image
static void runPipelineFullBuffer(
	const RenderChannels& render_channels,
	const ArrayRef<RenderRegion>& render_regions,
	int ssf,
	const ArrayRef<Vec3f>& layer_weights,
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	float region_image_scale,
	float region_alpha_bias,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	const Reference<PostProDiffraction>& post_pro_diffraction,
	Image4f& temp_summed_buffer,
	Image4f& temp_AD_buffer,
	Image4f& ldr_buffer_out,
	bool image_buffer_in_XYZ,
	int margin_ssf1, // Margin width (for just one side), in pixels, at ssf 1.  This may be zero for loaded LDR images. (PNGs etc..)
	Indigo::TaskManager& task_manager,
	const CurveData& curve_data,
	bool apply_curves)
{
	//Timer t;
	//const bool PROFILE = false;

	// Get float XYZ->sRGB matrix
	Matrix3f XYZ_to_sRGB;
	if(image_buffer_in_XYZ)
		for(int i = 0; i < 9; ++i)
			XYZ_to_sRGB.e[i] = (float)renderer_settings.colour_space_converter->getSrcXYZTosRGB().e[i];
	else
		XYZ_to_sRGB = Matrix3f::identity();

	// Reinhard tonemapping needs a global average and max luminance, not just over each bucket, so we pre-compute this first if necessary.
	// This is pretty inefficient since we re-compute the summed pixels all over again while going over the tiles...
	float avg_lumi = 0, max_lumi = 0;
	const ReinhardToneMapper* reinhard = dynamic_cast<const ReinhardToneMapper*>(renderer_settings.tone_mapper.getPointer());
	if(reinhard != NULL)
		reinhard->computeLumiScales(render_channels, layer_weights, image_scale, avg_lumi, max_lumi);

	const ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi);


	//if(PROFILE) t.reset();
	temp_summed_buffer.resizeNoCopy(render_channels.getWidth(), render_channels.getHeight());
	sumLightLayers(layer_weights, image_scale, region_image_scale, render_channels, render_regions, margin_ssf1, ssf, 
		renderer_settings.zero_alpha_outside_region, region_alpha_bias, renderer_settings.render_foreground_alpha, temp_summed_buffer, task_manager);
	//if(PROFILE) conPrint("\tsumBuffers: " + t.elapsedString());

	// Apply diffraction filter if applicable
	if(renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && post_pro_diffraction.nonNull())
	{
		BufferedPrintOutput bpo;
		temp_AD_buffer.resizeNoCopy(render_channels.getWidth(), render_channels.getHeight());
		post_pro_diffraction->applyDiffractionFilterToImage(bpo, temp_summed_buffer, temp_AD_buffer, task_manager);
		
		// Do 'overbright' pixel spreading.
		// The idea here is that any really bright pixels can cause artifacts due to aperture diffraction, such as dark rings
		// a few pixels wide around bright, small light sources.
		// So for such pixels, we spread out the pixel's energy over a small region with a Gaussian filter.
		const bool DO_OVERBRIGHT_PIXEL_SPREADING = true;
		if(DO_OVERBRIGHT_PIXEL_SPREADING)
		{
			//conPrint("--------------- Doing overbright pixel spreading----------------");
			//Timer timer2;

			// We will write modified image back to temp_summed_buffer, so clear it first.
			temp_summed_buffer.zero();

			// Compute white threshold for the current tone mapper.
			// This is the value of a pixel that will get tone mapped to white.
			const float white_threshold = renderer_settings.tone_mapper->getWhiteThreshold(tonemap_params);

			// Compute the overbright threshold.
			// The lower this value is, the greater the number of pixels that will get blurred.
			// If it's too low too much of the image will be blurred.
			// If it's too high however, we will let A.D. artifacts through.
			float overbright_threshold = 10.0f * white_threshold;

			// Keep raising the overbright threshold until < NUM_OVERBRIGHT_PIXEL_THRESHOLD pixels are overbright.
			for(int z=0; z<10; ++z)
			{
				size_t num_overbright_pixels = 0; // Might be more than 2^32

				// Do a pass to get the number of overbright pixels.
				// If there are too many then don't do overbright pixel spreading, because it will take too long.
				for(int y=0; y<temp_AD_buffer.getHeight(); ++y)
				for(int x=0; x<temp_AD_buffer.getWidth(); ++x)
				{
					if(averageXYZVal(temp_AD_buffer.getPixel(x, y)) > overbright_threshold)
						num_overbright_pixels++;
				}

				// conPrint("overbright_threshold: " + toString(overbright_threshold));
				// conPrint("num_overbright_pixels: " + toString(num_overbright_pixels));

				// This threshold corresponds to about 10s of processing time at ssf5.
				const size_t NUM_OVERBRIGHT_PIXEL_THRESHOLD = 100000;

				if(num_overbright_pixels < NUM_OVERBRIGHT_PIXEL_THRESHOLD)
					break;

				overbright_threshold *= 10.0f;
			}

			// This constant (5.0) is chosen so that offset will be pretty small (~= 1e-7).
			const int filter_r = (int)ceil(ssf * 5.0);

			// Get the value of the Gaussian at filter_r
			const float std_dev = (float)ssf;
			const float gaussian_scale_factor = 1 / (Maths::get2Pi<float>() * std_dev*std_dev);
			const float gaussian_exponent_factor = -1 / (2*std_dev*std_dev);

			const float offset = Maths::eval2DGaussian<float>((float)(filter_r*filter_r), std_dev);
			//printVar(offset);

			for(int y=0; y<temp_AD_buffer.getHeight(); ++y)
			for(int x=0; x<temp_AD_buffer.getWidth(); ++x)
			{
				const Colour4f& in_colour = temp_AD_buffer.getPixel(x, y);

				if(averageXYZVal(in_colour) > overbright_threshold)
				{
					// conPrint("Pixel " + toString(x) + ", " + toString(y) + " is above threshold.");

					// Splat in a gaussian 
					int dx_begin = myMax(x - filter_r, 0);
					int dx_end   = myMin(x + filter_r, (int)temp_AD_buffer.getWidth());
					int dy_begin = myMax(y - filter_r, 0);
					int dy_end   = myMin(y + filter_r, (int)temp_AD_buffer.getHeight());

					for(int dy=dy_begin; dy<dy_end; ++dy)
					for(int dx=dx_begin; dx<dx_end; ++dx)
					{
						const float r2 = Maths::square((float)dx - (float)x) + Maths::square((float)dy - (float)y);

						// See http://mathworld.wolfram.com/GaussianFunction.html , eqn 9.
						const float gaussian = gaussian_scale_factor * std::exp(gaussian_exponent_factor * r2); 

						temp_summed_buffer.getPixel(dx, dy) += in_colour * myMax(gaussian - offset, 0.0f);
					}
				}
				else
				{
					temp_summed_buffer.getPixel(x, y) += in_colour;
				}

				// Just copy over the alpha directly, as we don't want to convolve it.
				temp_summed_buffer.getPixel(x, y).x[3] = in_colour.x[3];
			}

			//conPrint("Overbright pixel spreading took " + timer2.elapsedStringNPlaces(5));
		}
		else
		{
			temp_summed_buffer = temp_AD_buffer;
		}
	}


	// Either tonemap, or do equivalent operation for non-colour passes.
	if(renderer_settings.shadow_pass)
	{
		for(size_t i = 0; i < temp_summed_buffer.numPixels(); ++i)
		{
			float occluded_luminance   = temp_summed_buffer.getPixel(i).x[0];
			float unoccluded_luminance = temp_summed_buffer.getPixel(i).x[1];
			float unshadow_frac = myClamp(occluded_luminance / unoccluded_luminance, 0.0f, 1.0f);
			// Set to a black colour, with alpha value equal to the 'shadow fraction'.
			temp_summed_buffer.getPixel(i) = Colour4f(0, 0, 0, 1 - unshadow_frac);
		}
	}
	else
	{
		//if(PROFILE) t.reset();

		const int final_xres = (int)temp_summed_buffer.getWidth();
		const int final_yres = (int)temp_summed_buffer.getHeight();
		const int x_tiles = Maths::roundedUpDivide<int>(final_xres, (int)image_tile_size);
		const int y_tiles = Maths::roundedUpDivide<int>(final_yres, (int)image_tile_size);
		const int num_tiles = x_tiles * y_tiles;

		ToneMapTaskClosure closure;
		closure.tone_mapper = renderer_settings.tone_mapper.ptr();
		closure.tonemap_params = &tonemap_params;
		closure.temp_summed_buffer = &temp_summed_buffer;
		closure.final_xres = final_xres;
		closure.final_yres = final_yres;
		closure.x_tiles = x_tiles;

		task_manager.runParallelForTasks<ToneMapTask, ToneMapTaskClosure>(closure, 0, num_tiles);

		//if(PROFILE) conPrint("\tTone map: " + t.elapsedString());
	}

	// Components should be in range [0, 1]
	assert(temp_summed_buffer.minPixelComponent() >= 0.0f);
	//const float max_c = temp_summed_buffer.maxPixelComponent();
	assert(temp_summed_buffer.maxPixelComponent() <= 1.0f);

	// Compute final dimensions of LDR image.
	// This is the size after the margins have been trimmed off, and the image has been downsampled.
	const size_t supersample_factor = (size_t)ssf;
	const size_t border_width = (size_t)margin_ssf1;

	const size_t final_xres = render_channels.getWidth()  / supersample_factor - border_width * 2; // assert(final_xres == renderer_settings.getWidth());
	const size_t final_yres = render_channels.getHeight() / supersample_factor - border_width * 2; // assert(final_yres == renderer_settings.getWidth());
	ldr_buffer_out.resizeNoCopy(final_xres, final_yres);

	// Collapse super-sampled image down to final image size
	// NOTE: this trims off the borders
	if(supersample_factor > 1)
	{
		//if(PROFILE) t.reset();
		Image4f::downsampleImage(
			supersample_factor, // factor
			border_width,
			renderer_settings.getDownsizeFilterFunc().getFilterSpan(ssf),
			resize_filter,
			1.0f, // max component value
			temp_summed_buffer, // in
			ldr_buffer_out, // out
			task_manager
		);
		//if(PROFILE) conPrint("\tcollapseImage: " + t.elapsedString());
	}
	else
	{
		// Copy to temp_buffer 3, removing border.
		const int b = margin_ssf1;

		temp_summed_buffer.blitToImage(b, b, (int)temp_summed_buffer.getWidth() - b, (int)temp_summed_buffer.getHeight() - b, ldr_buffer_out, 0, 0);
	}

	if(apply_curves) // Apply colour curves
	{
		const size_t tile_buffer_pixels = ldr_buffer_out.numPixels();
		for(size_t i = 0; i < tile_buffer_pixels; ++i)
		{
			const float* const data = curve_data.colour_curve_data;
			const int n = CurveData::table_vals;
			const Colour4f col = ldr_buffer_out.getPixel(i);

			const float pre_r = CurveData::curveTableEval(col.x[0], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);
			const float pre_g = CurveData::curveTableEval(col.x[1], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);
			const float pre_b = CurveData::curveTableEval(col.x[2], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);

			const float curve_r = CurveData::curveTableEval(pre_r, data[1*3+0], data[1*3+1], data[1*3+2], &data[12+1*n]);
			const float curve_g = CurveData::curveTableEval(pre_g, data[2*3+0], data[2*3+1], data[2*3+2], &data[12+2*n]);
			const float curve_b = CurveData::curveTableEval(pre_b, data[3*3+0], data[3*3+1], data[3*3+2], &data[12+3*n]);

			ldr_buffer_out.getPixel(i).set(curve_r, curve_g, curve_b, col.x[3]);
		}
	}

	//conPrint("ldr_buffer_out.maxPixelComponent(): " + toString(ldr_buffer_out.maxPixelComponent()));
	
	assert(ldr_buffer_out.getWidth()  == final_xres); // renderer_settings.getWidth());
	assert(ldr_buffer_out.getHeight() == final_yres); // renderer_settings.getHeight());

	// Components should be in range [0, 1]
	assert(ldr_buffer_out.minPixelComponent() >= 0.0f);
	assert(ldr_buffer_out.maxPixelComponent() <= 1.0f);

	// For receiver/spectral rendering (which has margin 0), force alpha values to 1.
	// Otherwise pixels on the edge of the image get alpha < 1, which results in scaling when doing the alpha divide below.
	if(render_channels.hasSpectral())
		for(size_t i=0; i<ldr_buffer_out.numPixels(); ++i)
			ldr_buffer_out.getPixel(i).x[3] = 1; 
}


struct ImagePipelineTaskClosure
{
	std::vector<Image4f>* per_thread_tile_buffers;
	const RenderChannels* render_channels;
	const ArrayRef<Vec3f>* layer_weights;
	float image_scale;
	float region_image_scale;
	Image4f* ldr_buffer_out;
	const RendererSettings* renderer_settings;
	const float* resize_filter;
	const ToneMapperParams* tonemap_params;
	ptrdiff_t x_tiles, final_xres, final_yres, filter_size, margin_ssf1;
	const ArrayRef<RenderRegion>* render_regions;
	float region_alpha_bias;
	int subres_factor;
	bool render_foreground_alpha;
	bool do_tonemapping;
	int ssf;

	bool skip_curves;

	CurveData curve_data;

	const ChannelInfo* channel;
};


inline static Colour4f toColour4f(const Vec3f& v)
{
	return Colour4f(v.x, v.y, v.z, 0.f);
}


/*static const Colour3f colourForDensity(float density, float max_density)
{
	//return Colour3f(density / max_density);

	// From http://local.wasp.uwa.edu.au/~pbourke/texture_colour/colourramp/
	Colour3f c(1);

	const float vmin = 0;
	const float vmax = max_density;
	const float dv = vmax - vmin;
	const float v = density;

	if(v < (vmin + 0.25f * dv)) {
		c.r = 0;
		c.g = 4 * (v - vmin) / dv;
	}
	else if(v < (vmin + 0.5f * dv)) {
		c.r = 0;
		c.b = 1 + 4 * (vmin + 0.25f * dv - v) / dv;
	}
	else if(v < (vmin + 0.75f * dv)) {
		c.r = 4 * (v - vmin - 0.5f * dv) / dv;
		c.b = 0;
	}
	else {
		c.g = 1 + 4 * (vmin + 0.75f * dv - v) / dv;
		c.b = 0;
	}

	return c;
}*/


// Tonemap and downsizes (if downsizing is needed) a single image tile.
class ImagePipelineTask : public Indigo::Task
{
public:
	ImagePipelineTask() : closure_(0) {}

	virtual void run(size_t thread_index)
	{
		assert(this->closure_);
		const ImagePipelineTaskClosure& closure = *this->closure_;

		// Build layer weights.  Layer weight = light layer value * image_scale.
		const float image_scale        = closure.image_scale;
		const float region_image_scale = closure.region_image_scale;
		SmallArray<Colour4f, 32> layer_weights(closure.render_channels->layers.size());
		SmallArray<Colour4f, 32> region_layer_weights(closure.render_channels->layers.size());
		
		for(size_t i=0; i<closure.render_channels->layers.size(); ++i)
		{
			layer_weights[i]        = toColour4f((*closure.layer_weights)[i]) * closure.image_scale;
			region_layer_weights[i] = toColour4f((*closure.layer_weights)[i]) * closure.region_image_scale;
		}

		const bool render_foreground_alpha = closure.render_foreground_alpha;
		const bool apply_curves = !closure.skip_curves;
		const bool render_region_enabled = closure.render_channels->target_region_layers;//  closure.renderer_settings->render_region_enabled;
		const bool zero_alpha_outside_region = closure.renderer_settings->zero_alpha_outside_region;
		
		const bool blend_main_layers = closure.channel == NULL;
		const int source_render_channel_offset = closure.channel ? closure.channel->offset : 0;
		const bool colour3_channel = closure.channel ? (closure.channel->num_components >= 3) : false;
		const bool tonemap_channel = closure.do_tonemapping && (closure.channel == NULL || (closure.channel->type == ChannelInfo::ChannelType_MainLayers || closure.channel->type == ChannelInfo::ChannelType_Beauty));

		// We want to allow channels like position to take on negative values.
		// For other channels, like beauty channels, we want values to be >= 0.
		// These constants will only be used when we are not tonemapping.
		const float lower_clamping_bound = closure.channel ? ((closure.channel->type == ChannelInfo::ChannelType_NonBeauty) ? -std::numeric_limits<float>::infinity() : 0.f) : 0.f;
		const bool convert_from_XYZ_to_sRGB = closure.channel ? (closure.channel->type == ChannelInfo::ChannelType_MainLayers || closure.channel->type == ChannelInfo::ChannelType_Beauty) : true;

		// Transform from XYZ to sRGB
		Matrix4f colour_space_change;
		if(convert_from_XYZ_to_sRGB)
			colour_space_change = Matrix4f(closure.tonemap_params->XYZ_to_sRGB, Vec3f(0.0f)); // Bottom row should be (0,0,0,1) so that alpha value is not changed.
		else
			colour_space_change = Matrix4f::identity();

		const ptrdiff_t final_xres = closure.final_xres;
		const ptrdiff_t filter_size = closure.filter_size;


		const glare::AllocatorVector<float, 16>& front_data = closure.render_channels->front_data;
		const glare::AllocatorVector<float, 16>& region_data = closure.render_channels->region_data;
		const ptrdiff_t stride = closure.render_channels->stride;
		const ptrdiff_t alpha_offset = (size_t)closure.render_channels->alpha.offset;
		const ptrdiff_t main_layer_num_components = closure.render_channels->layers[0].num_components; // This will be 4 for GPU rendering as alpha is interleaved.


		const ptrdiff_t xres		= (ptrdiff_t)closure.render_channels->getWidth(); // in intermediate pixels
		//#ifndef NDEBUG
		const ptrdiff_t yres		= (ptrdiff_t)closure.render_channels->getHeight(); // in intermediate pixels
		//#endif
		const ptrdiff_t ss_factor   = (ptrdiff_t)closure.ssf;
		const ptrdiff_t gutter_pix  = (ptrdiff_t)closure.margin_ssf1;
		const ptrdiff_t filter_span = filter_size / 2 - 1;
		const ptrdiff_t num_layers  = (ptrdiff_t)closure.render_channels->layers.size();
		const ArrayRef<RenderRegion>& render_regions = *closure.render_regions;
		const float region_alpha_bias = closure.region_alpha_bias;

		const int rr_margin   = 1; // Pixels near the edge of the RR may not be fully bright due to splat filter, so we need a margin.

		SmallArray<Rect2i, 8> regions(render_regions.size()); // Render regions bounds in intermediate pixel coords
		for(size_t i=0; i<render_regions.size(); ++i)
			regions[i] = Rect2i(Vec2i(	(render_regions[i].x1 + (int)gutter_pix) * (int)ss_factor + rr_margin,
										(render_regions[i].y1 + (int)gutter_pix) * (int)ss_factor + rr_margin),
								Vec2i(	(render_regions[i].x2 + (int)gutter_pix) * (int)ss_factor - rr_margin,
										(render_regions[i].y2 + (int)gutter_pix) * (int)ss_factor - rr_margin));


		const float alpha_bias_factor        = 1.f + image_scale;
		const float region_alpha_bias_factor = 1.f + region_alpha_bias;

		Image4f* const ldr_buffer_out = closure.ldr_buffer_out;

		for(int tile = begin; tile < end; ++tile)
		{
			Image4f& tile_buffer = (*closure.per_thread_tile_buffers)[thread_index];

			// Get the final image tile bounds for the tile index
			const ptrdiff_t tile_x = (ptrdiff_t)tile % closure.x_tiles;
			const ptrdiff_t tile_y = (ptrdiff_t)tile / closure.x_tiles;
			const ptrdiff_t x_min  = tile_x * image_tile_size, x_max = std::min<ptrdiff_t>(closure.final_xres, (tile_x + 1) * image_tile_size); // in final px coords
			const ptrdiff_t y_min  = tile_y * image_tile_size, y_max = std::min<ptrdiff_t>(closure.final_yres, (tile_y + 1) * image_tile_size); // in final px coords

			if(ss_factor > closure.subres_factor) // Perform downsampling if needed
			{
				const ptrdiff_t bucket_min_x = (x_min + gutter_pix) * ss_factor - filter_span; assert(bucket_min_x >= 0); // coordinates in source intermediate buffer
				const ptrdiff_t bucket_min_y = (y_min + gutter_pix) * ss_factor - filter_span; assert(bucket_min_y >= 0); // coordinates in source intermediate buffer
				const ptrdiff_t bucket_max_x = ((x_max - 1) + gutter_pix) * ss_factor + filter_span + 1; assert(bucket_max_x <= xres);
				const ptrdiff_t bucket_max_y = ((y_max - 1) + gutter_pix) * ss_factor + filter_span + 1; assert(bucket_max_y <= yres);
				const ptrdiff_t bucket_span  = bucket_max_x - bucket_min_x;

				// First we get the weighted sum of all pixels in the layers
				size_t dst_addr = 0;
				for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
				for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
				{
					const ptrdiff_t src_pixel_offset = (y * xres + x) * stride;
					Colour4f sum(0.0f);

					if(blend_main_layers) // blend together main light layers (usual rendering).
					{
						for(ptrdiff_t z = 0; z < num_layers; ++z)
						{
							float r = front_data[src_pixel_offset + z * main_layer_num_components + 0];
							float g = front_data[src_pixel_offset + z * main_layer_num_components + 1];
							float b = front_data[src_pixel_offset + z * main_layer_num_components + 2];
							sum += Colour4f(r, g, b, 0.f) * layer_weights[z];
						}
					}
					else // else just tonemap one layer
					{
						if(colour3_channel)
						{
							float r = front_data[src_pixel_offset + source_render_channel_offset + 0];
							float g = front_data[src_pixel_offset + source_render_channel_offset + 1];
							float b = front_data[src_pixel_offset + source_render_channel_offset + 2];
							sum += Colour4f(r, g, b, 0.f) * image_scale;
						}
						else // Else if scalar channel:
						{
							float v = front_data[src_pixel_offset + source_render_channel_offset + 0];
							sum += Colour4f(v, v, v, 0.f) * image_scale;
						}
					}

					// Get alpha from alpha channel if we are doing a foreground-alpha render
					sum.x[3] = render_foreground_alpha ? (front_data[src_pixel_offset + alpha_offset] * alpha_bias_factor * image_scale) : 1.f;

					// If this pixel lies in a render region, set the pixel value to the value in the render region layer.
					if(render_region_enabled)
					{
						if(pixelIsInARegion(x, y, regions))
						{
							sum = Colour4f(0.f);
							if(blend_main_layers)
							{
								for(ptrdiff_t z = 0; z < num_layers; ++z)
								{
									float r = region_data[src_pixel_offset + z * main_layer_num_components + 0];
									float g = region_data[src_pixel_offset + z * main_layer_num_components + 1];
									float b = region_data[src_pixel_offset + z * main_layer_num_components + 2];
									sum += Colour4f(r, g, b, 0.f) * region_layer_weights[z];
								}
							}
							else
							{
								if(colour3_channel)
								{
									float r = region_data[src_pixel_offset + source_render_channel_offset + 0];
									float g = region_data[src_pixel_offset + source_render_channel_offset + 1];
									float b = region_data[src_pixel_offset + source_render_channel_offset + 2];
									sum += Colour4f(r, g, b, 0.f) * region_image_scale;
								}
								else // Else if scalar channel:
								{
									float v = region_data[src_pixel_offset + source_render_channel_offset + 0];
									sum += Colour4f(v, v, v, 0.f) * region_image_scale;
								}
							}

							sum.x[3] = render_foreground_alpha ? (region_data[src_pixel_offset + alpha_offset] * region_alpha_bias_factor * region_image_scale) : 1.f;
						}
						else
						{
							// Zero out pixels not in the render region, if zero_alpha_outside_region is enabled, or if there are no samples on the main layers.
							if(zero_alpha_outside_region || (image_scale == 0.0))
								sum = Colour4f(0.f);
						}
					}

					tile_buffer.getPixel(dst_addr++) = sum;
				}

				// Either tonemap, or do equivalent operation for non-colour passes.
				if(closure.renderer_settings->shadow_pass)
				{
					const size_t tile_buffer_pixels = tile_buffer.numPixels();
					for(size_t i = 0; i < tile_buffer_pixels; ++i)
					{
						float occluded_luminance   = tile_buffer.getPixel(i).x[0];
						float unoccluded_luminance = tile_buffer.getPixel(i).x[1];
						float unshadow_frac = myClamp(occluded_luminance / unoccluded_luminance, 0.0f, 1.0f);
						// Set to a black colour, with alpha value equal to the 'shadow fraction'.
						tile_buffer.getPixel(i).set(0, 0, 0, 1 - unshadow_frac);
					}
				}
				else
				{
					if(tonemap_channel) // Don't tonemap render passes like depth etc..
						closure.renderer_settings->tone_mapper->toneMapImage(*closure.tonemap_params, tile_buffer);
					else
					{
						const size_t tile_buffer_pixels = tile_buffer.numPixels();
						for(size_t i = 0; i < tile_buffer_pixels; ++i)
						{
							const Colour4f col = tile_buffer.getPixel(i);
							const Vec4f sRGB = colour_space_change * Vec4f(col.v); // Standard pipeline: transform from XYZ to sRGB
							tile_buffer.getPixel(i) = max(Colour4f(sRGB.v), Colour4f(lower_clamping_bound)); // Make sure >= lower_clamping_bound
						}
					}
				}

				if(apply_curves) // Apply colour curves
				{
					const size_t tile_buffer_pixels = tile_buffer.numPixels();
					for(size_t i = 0; i < tile_buffer_pixels; ++i)
					{
						const float* const data = closure.curve_data.colour_curve_data;
						const int n = CurveData::table_vals;
						const Colour4f col = tile_buffer.getPixel(i);

						const float pre_r = CurveData::curveTableEval(col.x[0], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);
						const float pre_g = CurveData::curveTableEval(col.x[1], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);
						const float pre_b = CurveData::curveTableEval(col.x[2], data[0*3+0], data[0*3+1], data[0*3+2], &data[12]);

						const float curve_r = CurveData::curveTableEval(pre_r, data[1*3+0], data[1*3+1], data[1*3+2], &data[12+1*n]);
						const float curve_g = CurveData::curveTableEval(pre_g, data[2*3+0], data[2*3+1], data[2*3+2], &data[12+2*n]);
						const float curve_b = CurveData::curveTableEval(pre_b, data[3*3+0], data[3*3+1], data[3*3+2], &data[12+3*n]);

						tile_buffer.getPixel(i).set(curve_r, curve_g, curve_b, col.x[3]);
					}
				}

				// Filter processed pixels into the final image
				for(ptrdiff_t y = y_min; y < y_max; ++y) // y is in final px coords
				for(ptrdiff_t x = x_min; x < x_max; ++x) // x is in final px coords
				{
					const ptrdiff_t pixel_min_x = (x + gutter_pix) * ss_factor - filter_span - bucket_min_x; // Coordinates in bucket pixels
					const ptrdiff_t pixel_min_y = (y + gutter_pix) * ss_factor - filter_span - bucket_min_y; // Coordinates in bucket pixels
					const ptrdiff_t pixel_max_x = (x + gutter_pix) * ss_factor + filter_span - bucket_min_x + 1;
					const ptrdiff_t pixel_max_y = (y + gutter_pix) * ss_factor + filter_span - bucket_min_y + 1;

					uint32 filter_addr = 0;
					Colour4f weighted_sum(0);
					for(ptrdiff_t v = pixel_min_y; v < pixel_max_y; ++v)
					for(ptrdiff_t u = pixel_min_x; u < pixel_max_x; ++u)
						weighted_sum.addMult(tile_buffer.getPixel(v * bucket_span + u), closure.resize_filter[filter_addr++]);

					assert(isFinite(weighted_sum.x[0]) && isFinite(weighted_sum.x[1]) && isFinite(weighted_sum.x[2]));

					ldr_buffer_out->getPixel(y * final_xres + x) = weighted_sum;
				}
			}
			else // No downsampling needed
			{
				// Since the pixels are 1:1 with the bucket bounds, simply offset the bucket rect by the left image margin / gutter pixels
				const ptrdiff_t bucket_min_x = x_min + gutter_pix;
				const ptrdiff_t bucket_min_y = y_min + gutter_pix;
				const ptrdiff_t bucket_max_x = myMin(x_max + gutter_pix, xres); assert(bucket_max_x <= xres);
				const ptrdiff_t bucket_max_y = myMin(y_max + gutter_pix, yres); assert(bucket_max_y <= yres);

				// First we get the weighted sum of all pixels in the layers
				size_t addr = 0;
				for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
				for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
				{
					const ptrdiff_t src_pixel_offset = (y * xres + x) * stride;
					Colour4f sum(0.0f);

					if(blend_main_layers) // blend together main light layers (usual rendering).
					{
						for(ptrdiff_t z = 0; z < num_layers; ++z)
						{
							float r = front_data[src_pixel_offset + z * main_layer_num_components + 0];
							float g = front_data[src_pixel_offset + z * main_layer_num_components + 1];
							float b = front_data[src_pixel_offset + z * main_layer_num_components + 2];
							sum += Colour4f(r, g, b, 0.f) * layer_weights[z];
						}
					}
					else // else read from one of the non-main layers.
					{
						if(colour3_channel)
						{
							float r = front_data[src_pixel_offset + source_render_channel_offset + 0];
							float g = front_data[src_pixel_offset + source_render_channel_offset + 1];
							float b = front_data[src_pixel_offset + source_render_channel_offset + 2];
							sum += Colour4f(r, g, b, 0.f) * image_scale;
						}
						else
						{
							float v = front_data[src_pixel_offset + source_render_channel_offset + 0];
							sum += Colour4f(v, v, v, 0.f) * image_scale;
						}
					}

					// Get alpha from alpha channel if it exists
					sum.x[3] = render_foreground_alpha ? (front_data[src_pixel_offset + alpha_offset] * alpha_bias_factor * image_scale) : 1.f;
					
					// If this pixel lies in a render region, set the pixel value to the value in the render region layer.
					if(render_region_enabled)
					{
						if(pixelIsInARegion(x, y, regions))
						{
							sum = Colour4f(0.f);
							if(blend_main_layers)
							{
								for(ptrdiff_t z = 0; z < num_layers; ++z)
								{
									float r = region_data[src_pixel_offset + z * main_layer_num_components + 0];
									float g = region_data[src_pixel_offset + z * main_layer_num_components + 1];
									float b = region_data[src_pixel_offset + z * main_layer_num_components + 2];
									sum += Colour4f(r, g, b, 0.f) * region_layer_weights[z];
								}
							}
							else
							{
								if(colour3_channel)
								{
									float r = region_data[src_pixel_offset + source_render_channel_offset + 0];
									float g = region_data[src_pixel_offset + source_render_channel_offset + 1];
									float b = region_data[src_pixel_offset + source_render_channel_offset + 2];
									sum += Colour4f(r, g, b, 0.f) * region_image_scale;
								}
								else // Else if scalar channel:
								{
									float v = region_data[src_pixel_offset + source_render_channel_offset + 0];
									sum += Colour4f(v, v, v, 0.f) * region_image_scale;
								}
							}

							sum.x[3] = render_foreground_alpha ? (region_data[src_pixel_offset + alpha_offset] * region_alpha_bias_factor * region_image_scale) : 1.f;
						}
						else
						{
							// Zero out pixels not in the render region, if zero_alpha_outside_region is enabled, or if there are no samples on the main layers.
							if(zero_alpha_outside_region || (image_scale == 0.0))
								sum = Colour4f(0.f);
						}
					}

					tile_buffer.getPixel(addr++) = sum;
				}

				// Either tonemap, or do equivalent operation for non-colour passes.
				if(closure.renderer_settings->shadow_pass)
				{
					for(size_t i = 0; i < tile_buffer.numPixels(); ++i)
					{
						float occluded_luminance   = tile_buffer.getPixel(i).x[0];
						float unoccluded_luminance = tile_buffer.getPixel(i).x[1];
						float unshadow_frac = myClamp(occluded_luminance / unoccluded_luminance, 0.0f, 1.0f);
						// Set to a black colour, with alpha value equal to the 'shadow fraction'.
						tile_buffer.getPixel(i).set(0, 0, 0, 1 - unshadow_frac);
					}
				}
				else
				{
					if(tonemap_channel)
						closure.renderer_settings->tone_mapper->toneMapImage(*closure.tonemap_params, tile_buffer);
					else
					{
						const size_t tile_buffer_pixels = tile_buffer.numPixels();
						for(size_t i = 0; i < tile_buffer_pixels; ++i)
						{
							const Colour4f col = tile_buffer.getPixel(i);
							const Vec4f sRGB = colour_space_change * Vec4f(col.v); // Standard pipeline: transform from XYZ to sRGB
							tile_buffer.getPixel(i) = max(Colour4f(sRGB.v), Colour4f(lower_clamping_bound)); // Make sure >= lower_clamping_bound

							// False colour for receiver channel:
							// const float v = tile_buffer.getPixel(i).x[0];
							// const Colour3f mapped_col = colourForDensity(v, /*max-val=*/0.6f);
							// tile_buffer.getPixel(i) = Colour4f(mapped_col.r, mapped_col.g, mapped_col.b, 1.f);
						}
					}
				}

				addr = 0;
				if(apply_curves) // Copy processed pixels into the final image, with colour curve adjustment
				{
					for(ptrdiff_t y = y_min; y < y_max; ++y)
					for(ptrdiff_t x = x_min; x < x_max; ++x)
					{
						const float* const data = closure.curve_data.colour_curve_data;
						const int n = CurveData::table_vals;
						const Colour4f col = tile_buffer.getPixel(addr++);

						const float pre_r = CurveData::curveTableEval(col.x[0], data[0], data[1], data[2], &data[12]);
						const float pre_g = CurveData::curveTableEval(col.x[1], data[0], data[1], data[2], &data[12]);
						const float pre_b = CurveData::curveTableEval(col.x[2], data[0], data[1], data[2], &data[12]);

						const float curve_r = CurveData::curveTableEval(pre_r, data[1*3+0], data[1*3+1], data[1*3+2], &data[12+1*n]);
						const float curve_g = CurveData::curveTableEval(pre_g, data[2*3+0], data[2*3+1], data[2*3+2], &data[12+2*n]);
						const float curve_b = CurveData::curveTableEval(pre_b, data[3*3+0], data[3*3+1], data[3*3+2], &data[12+3*n]);

						ldr_buffer_out->getPixel(y * final_xres + x).set(curve_r, curve_g, curve_b, col.x[3]);
					}
				}
				else // Copy processed pixels into the final image, without colour curve processing
				{
					for(ptrdiff_t y = y_min; y < y_max; ++y)
					for(ptrdiff_t x = x_min; x < x_max; ++x)
						ldr_buffer_out->getPixel(y * final_xres + x) = tile_buffer.getPixel(addr++);
				}
			}
		}
	}

	const ImagePipelineTaskClosure* closure_;
	int begin, end;
};


RunPipelineScratchState::RunPipelineScratchState()
{}


RunPipelineScratchState::~RunPipelineScratchState()
{}


void runPipeline(
	RunPipelineScratchState& scratch_state,
	const RenderChannels& render_channels,
	const ChannelInfo* channel,
	int final_width,
	int final_height,
	int ssf,
	const ArrayRef<RenderRegion>& render_regions,
	const ArrayRef<Vec3f>& layer_weights,
	float image_scale,
	float region_image_scale,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	const Reference<PostProDiffraction>& post_pro_diffraction,
	Image4f& ldr_buffer_out,
	bool& output_is_nonlinear,
	bool XYZ_colourspace,
	int margin_ssf1,
	Indigo::TaskManager& task_manager,
	int subres_factor,
	bool do_tonemapping
	)
{
	ScopeProfiler _scope("ImagingPipeline::runPipeline", 1);

	const bool no_curves =
		renderer_settings.overall_curve.isNull() &&
		renderer_settings.rgb_curves[0].isNull() &&
		renderer_settings.rgb_curves[1].isNull() &&
		renderer_settings.rgb_curves[2].isNull();
	const bool identity_curves =
		renderer_settings.overall_curve.nonNull() && renderer_settings.overall_curve->isIdentity() &&
		renderer_settings.rgb_curves[0].nonNull() && renderer_settings.rgb_curves[0]->isIdentity() &&
		renderer_settings.rgb_curves[1].nonNull() && renderer_settings.rgb_curves[1]->isIdentity() &&
		renderer_settings.rgb_curves[2].nonNull() && renderer_settings.rgb_curves[2]->isIdentity();
	const bool skip_curves = no_curves || identity_curves; // Only do curves processing if we have curve data and if it's not identity

	CurveData curve_data;
	if(!skip_curves) // Evaluate the overall and RGBA curves into tables
	{
		const int n = CurveData::table_vals;
		const float o_min = renderer_settings.overall_curve->knots[0], o_max = renderer_settings.overall_curve->knots[renderer_settings.overall_curve->knots.size() - 1];
		const float r_min = renderer_settings.rgb_curves[0]->knots[0], r_max = renderer_settings.rgb_curves[0]->knots[renderer_settings.rgb_curves[0]->knots.size() - 1];
		const float g_min = renderer_settings.rgb_curves[1]->knots[0], g_max = renderer_settings.rgb_curves[1]->knots[renderer_settings.rgb_curves[1]->knots.size() - 1];
		const float b_min = renderer_settings.rgb_curves[2]->knots[0], b_max = renderer_settings.rgb_curves[2]->knots[renderer_settings.rgb_curves[2]->knots.size() - 1];
		const float o_scl = (n - 1) / (o_max - o_min);
		const float r_scl = (n - 1) / (r_max - r_min);
		const float g_scl = (n - 1) / (g_max - g_min);
		const float b_scl = (n - 1) / (b_max - b_min);

		curve_data.colour_curve_data[0 * 3 + 0] = o_min; curve_data.colour_curve_data[0 * 3 + 1] = o_max; curve_data.colour_curve_data[0 * 3 + 2] = o_scl;
		curve_data.colour_curve_data[1 * 3 + 0] = r_min; curve_data.colour_curve_data[1 * 3 + 1] = r_max; curve_data.colour_curve_data[1 * 3 + 2] = r_scl;
		curve_data.colour_curve_data[2 * 3 + 0] = g_min; curve_data.colour_curve_data[2 * 3 + 1] = g_max; curve_data.colour_curve_data[2 * 3 + 2] = g_scl;
		curve_data.colour_curve_data[3 * 3 + 0] = b_min; curve_data.colour_curve_data[3 * 3 + 1] = b_max; curve_data.colour_curve_data[3 * 3 + 2] = b_scl;

		for(int z = 0; z < n; ++z)
		{
			const float u = z * (1.0f / n);
			curve_data.colour_curve_data[12 + 0 * n + z] = renderer_settings.overall_curve->evaluate(o_min + u * (o_max - o_min));
			curve_data.colour_curve_data[12 + 1 * n + z] = renderer_settings.rgb_curves[0]->evaluate(r_min + u * (r_max - r_min));
			curve_data.colour_curve_data[12 + 2 * n + z] = renderer_settings.rgb_curves[1]->evaluate(g_min + u * (g_max - g_min));
			curve_data.colour_curve_data[12 + 3 * n + z] = renderer_settings.rgb_curves[2]->evaluate(b_min + u * (b_max - b_min));
		}
	}
	else
		std::memset(curve_data.colour_curve_data, 0, sizeof(curve_data.colour_curve_data)); // Stop the compiler complaining about potentially uninitialised data.

	// Compute region_alpha_bias
	float region_alpha_bias = 1.f;
	if(!render_regions.empty())
	{
		const std::vector<Rect2f> normed_rrs = RendererSettings::computeNormedRenderRegions(render_regions, final_width, final_height, margin_ssf1);
		region_alpha_bias = RendererSettings::getSumNormedRectArea(normed_rrs);
	}


	// If diffraction filter needs to be appled, or the margin is zero (which is the case for numerical receiver mode), do non-bucketed tone mapping.
	// We do this for margin = 0 because the bucketed filtering code is not valid when margin = 0.
	// Don't do post-pro diffraction when subres_factor is > 1 (e.g. when doing realtime changes), as runPipelineFullBuffer() doesn't handle subres sizes, also not needed.
	if((channel == NULL) && (subres_factor == 1) && ((margin_ssf1 == 0) || (renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && /*camera*/post_pro_diffraction.nonNull())))
	{
		runPipelineFullBuffer(render_channels, render_regions, ssf, layer_weights, image_scale, region_image_scale, region_alpha_bias, renderer_settings, resize_filter, post_pro_diffraction, // camera,
							scratch_state.temp_summed_buffer, scratch_state.temp_AD_buffer,
							ldr_buffer_out, XYZ_colourspace, margin_ssf1, task_manager, curve_data, !skip_curves);
	}
	else
	{
		// Grab some unsigned constants for convenience
		const ptrdiff_t ss_factor   = (ptrdiff_t)ssf;
		const ptrdiff_t filter_size = (ptrdiff_t)renderer_settings.getDownsizeFilterFunc().getFilterSpan((int)ss_factor);

		// Compute final dimensions of LDR image.
		// This is the size after the margins have been trimmed off, and the image has been downsampled.
		ptrdiff_t final_xres = ldr_buffer_out.getWidth();
		ptrdiff_t final_yres = ldr_buffer_out.getHeight();

		const ptrdiff_t x_tiles = Maths::roundedUpDivide<ptrdiff_t>(final_xres, (ptrdiff_t)image_tile_size);
		const ptrdiff_t y_tiles = Maths::roundedUpDivide<ptrdiff_t>(final_yres, (ptrdiff_t)image_tile_size);
		const ptrdiff_t num_tiles = x_tiles * y_tiles;
		ptrdiff_t tile_buffer_size;
		
		if(ss_factor == 1 || subres_factor > 1)
			tile_buffer_size = image_tile_size; // We won't be doing downsampling.  Therefore we don't need a margin for the downsample filter.
		else
			tile_buffer_size = image_tile_size * ss_factor + filter_size;

		const size_t num_tasks = myMax<size_t>(1, task_manager.getNumThreads()); // Task manager may have zero threads, in which case use one task.

		// Ensure that we have sufficiently many buffers of sufficient size for as many threads as the task manager uses.
		scratch_state.per_thread_tile_buffers.resize(num_tasks);
		for(size_t tile_buffer = 0; tile_buffer < scratch_state.per_thread_tile_buffers.size(); ++tile_buffer)
		{
			scratch_state.per_thread_tile_buffers[tile_buffer].resizeNoCopy(tile_buffer_size, tile_buffer_size);
			scratch_state.per_thread_tile_buffers[tile_buffer].zero(); // NEW
		}

		// Get float XYZ->sRGB matrix
		Matrix3f XYZ_to_sRGB;
		if(XYZ_colourspace)
			for(int i = 0; i < 9; ++i)
				XYZ_to_sRGB.e[i] = (float)renderer_settings.colour_space_converter->getSrcXYZTosRGB().e[i];
		else
			XYZ_to_sRGB = Matrix3f::identity();

		// Reinhard tonemapping needs a global average and max luminance, not just over each bucket, so we pre-compute this first if necessary.
		// This is pretty inefficient since we re-compute the summed pixels all over again while going over the tiles...
		float avg_lumi = 0, max_lumi = 0;
		const ReinhardToneMapper* reinhard = dynamic_cast<const ReinhardToneMapper*>(renderer_settings.tone_mapper.getPointer());
		if(reinhard != NULL)
			reinhard->computeLumiScales(render_channels, layer_weights, image_scale, avg_lumi, max_lumi);

		ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi);

		ImagePipelineTaskClosure closure;
		closure.per_thread_tile_buffers = &scratch_state.per_thread_tile_buffers;
		closure.render_channels = &render_channels;
		closure.layer_weights = &layer_weights;
		closure.image_scale = image_scale;
		closure.region_image_scale = region_image_scale;
		closure.ldr_buffer_out = &ldr_buffer_out;
		closure.renderer_settings = &renderer_settings;
		closure.resize_filter = resize_filter;
		closure.render_regions = &render_regions;
		closure.tonemap_params = &tonemap_params;
		closure.render_foreground_alpha = renderer_settings.render_foreground_alpha;
		closure.channel = channel;
		closure.do_tonemapping = do_tonemapping;

		closure.x_tiles = x_tiles;
		closure.final_xres = final_xres;
		closure.final_yres = final_yres;
		closure.filter_size = filter_size;
		closure.margin_ssf1 = margin_ssf1;
		closure.region_alpha_bias = region_alpha_bias;
		closure.subres_factor = subres_factor;
		closure.ssf = ssf;

		closure.skip_curves = skip_curves;
		if(!skip_curves) closure.curve_data = curve_data;

		// Timer timer;

		// Allocate (or check are already allocated) ImagePipelineTasks
		
		scratch_state.tasks.resize(num_tasks);
		for(size_t i=0; i<num_tasks; ++i)
			if(scratch_state.tasks[i].isNull())
				scratch_state.tasks[i] = new ImagePipelineTask();

		const size_t num_tiles_per_task = Maths::roundedUpDivide(num_tiles, (ptrdiff_t)num_tasks);
		for(size_t i=0; i<num_tasks; ++i)
		{
			ImagePipelineTask* task = scratch_state.tasks[i].downcastToPtr<ImagePipelineTask>();
			task->closure_ = &closure;
			task->begin = myMin<int>((int)(num_tiles_per_task * i      ), (int)num_tiles);
			task->end   = myMin<int>((int)(num_tiles_per_task * (i + 1)), (int)num_tiles);
		}

		task_manager.runTasks(scratch_state.tasks);
		
		// conPrint("Image pipeline parallel loop took " + timer.elapsedString());
	}
	
	output_is_nonlinear = renderer_settings.tone_mapper.nonNull() ? renderer_settings.tone_mapper->outputIsNonLinear() : false;

	// TEMP HACK: Chromatic abberation
	/*Image temp;
	ImageFilter::chromaticAberration(ldr_buffer_out, temp, 0.002f);
	ldr_buffer_out = temp;
	ldr_buffer_out.clampInPlace(0, 1);*/
}


ToNonLinearSpaceScratchState::ToNonLinearSpaceScratchState()
{}


ToNonLinearSpaceScratchState::~ToNonLinearSpaceScratchState()
{}


/*
Converts some tonemapped image data to non-linear sRGB space.
Does a few things in preparation for conversion to an 8-bit output image format, such as dithering and gamma correction.
Input colour space is linear sRGB.
Output colour space is non-linear sRGB with the supplied gamma.
We also assume the output values are not premultiplied alpha.
*/
class ToNonLinearSpaceTask : public Indigo::Task
{
public:
	virtual void run(size_t thread_index)
	{
		const bool input_is_nonlinear = m_input_is_nonlinear;
		const bool dithering = m_dithering;
		uint8* const uint8_buf_out = uint8_buffer_out ? uint8_buffer_out->getPixelNonConst(0, 0) : NULL;

		// If shadow pass is enabled, don't apply gamma to the alpha, as it looks bad.
		const Colour4f gamma_mask = m_shadow_pass ? Colour4f(bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)).v) : Colour4f(bitcastToVec4f(Vec4i(0, 0, 0, 0)).v);

		Colour4f* const pixel_data = &ldr_buffer_in_out->getPixel(0);
		for(size_t z = begin; z<end; ++z)
		{
			Colour4f col = pixel_data[z];
			//Colour4f original_col = col;

			/////// Gamma correct (convert from linear sRGB to non-linear sRGB space) ///////
			if(input_is_nonlinear)
			{
				col.v = _mm_sqrt_ps(col.v); // data was in non-linear sRGB space, but raised to the power of 2.  Apply inverse.
			}
			else
			{
				const Colour4f sRGBcol = fastApproxLinearSRGBToNonLinearSRGB(col);
				col = select(col, sRGBcol, gamma_mask);
			}

			//if(input_is_nonlinear)
			//{
			//	const Colour4f sRGBcol(_mm_sqrt_ps(original_col.v)); // sRGB apart from alpha, which is garbarge
			//	col = select(sRGBcol, col, toColour4f(Vec4i(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0))); // col[0] = sRGBcol[0], col[1] = sRGBcol[1], col[2] = sRGBcol[2], col[3] = col[3]
			//}

			////// Dither ///////
			if(dithering)
				col = ditherPixel(col, z);

			////// Do alpha divide. Needed to compensate for alpha multiplication during blending //////
			col = max(Colour4f(0.0f), col); // Make sure alpha > 0

			const float recip_alpha = 1 / col.x[3];
			col *= Colour4f(recip_alpha, recip_alpha, recip_alpha, 1.0f);

			col = clamp(col, Colour4f(0.0f), Colour4f(1.0f));

			pixel_data[z] = col;

			const Vec4i col2 = toVec4i(col * Colour4f(255.9f));
			if(uint8_buf_out)
			{
				uint8_buf_out[z*4 + 0] = (uint8)col2.x[0];
				uint8_buf_out[z*4 + 1] = (uint8)col2.x[1];
				uint8_buf_out[z*4 + 2] = (uint8)col2.x[2];
				uint8_buf_out[z*4 + 3] = (uint8)col2.x[3];
			}
		}
	}

	Image4f* ldr_buffer_in_out;
	Bitmap* uint8_buffer_out;
	size_t begin, end;
	bool m_input_is_nonlinear, m_dithering, m_shadow_pass;
};


void toNonLinearSpace(
	Indigo::TaskManager& task_manager,
	ToNonLinearSpaceScratchState& scratch_state,
	bool shadow_pass,
	bool dithering,
	Image4f& ldr_buffer_in_out, // Input and output image, has alpha channel.
	bool input_is_nonlinear,
	Bitmap* uint8_buffer_out // May be NULL
	)
{
	ScopeProfiler _scope("ImagingPipeline::toNonLinearSpace", 1);

	if(uint8_buffer_out)
		uint8_buffer_out->resizeNoCopy(ldr_buffer_in_out.getWidth(), ldr_buffer_in_out.getHeight(), 4);

	const size_t num_tasks = myMax<size_t>(1, task_manager.getNumThreads()); // Task manager may have zero threads, in which case use one task.

	// Allocate (or check are already allocated) ToNonLinearSpaceTasks
	scratch_state.tasks.resize(num_tasks);
	for(size_t i=0; i<num_tasks; ++i)
		if(scratch_state.tasks[i].isNull())
			scratch_state.tasks[i] = new ToNonLinearSpaceTask();

	const size_t num_pixels_per_task = Maths::roundedUpDivide(ldr_buffer_in_out.numPixels(), num_tasks);
	for(size_t i=0; i<num_tasks; ++i)
	{
		ToNonLinearSpaceTask* task = scratch_state.tasks[i].downcastToPtr<ToNonLinearSpaceTask>();
		task->m_shadow_pass = shadow_pass;
		task->m_dithering = dithering;
		task->ldr_buffer_in_out = &ldr_buffer_in_out;
		task->uint8_buffer_out = uint8_buffer_out;
		task->m_input_is_nonlinear = input_is_nonlinear;
		task->begin = myMin(num_pixels_per_task * i      , ldr_buffer_in_out.numPixels());
		task->end   = myMin(num_pixels_per_task * (i + 1), ldr_buffer_in_out.numPixels());
	}

	task_manager.runTasks(scratch_state.tasks);

	// Components should be in range [0, 1]
	assert(ldr_buffer_in_out.minPixelComponent() >= 0.0f);
	assert(ldr_buffer_in_out.maxPixelComponent() <= 1.0f);
}


} // namespace ImagingPipeline
