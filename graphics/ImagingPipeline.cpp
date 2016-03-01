/*=====================================================================
ImagingPipeline.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Jul 13 13:44:31 +0100 2011
=====================================================================*/
#include "ImagingPipeline.h"


#include "../indigo/ColourSpaceConverter.h"
#include "../indigo/BufferedPrintOutput.h"
#include "../indigo/ToneMapper.h"
#include "../indigo/ReinhardToneMapper.h"
#include "../indigo/LinearToneMapper.h"
#include "../indigo/PostProDiffraction.h"
#include "../indigo/globals.h"
#include "../indigo/SingleFreq.h"
#include "../simpleraytracer/camera.h"
#include "../graphics/ImageFilter.h"
#include "../graphics/Image4f.h"
#include "../maths/mathstypes.h"
#include "../utils/Platform.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include <algorithm>


#if BUILD_TESTS


#include "../utils/PlatformUtils.h"
#include "../indigo/TestUtils.h"
#include "../indigo/MasterBuffer.h"
#include "../indigo/LinearToneMapper.h"
#include "../indigo/CameraToneMapper.h"


#endif


namespace ImagingPipeline
{


const uint32 image_tile_size = 64;


struct SumBuffersTaskClosure
{
	SumBuffersTaskClosure(const std::vector<Vec3f>& layer_scales_, float image_scale_, const RenderChannels& render_channels_, Image4f& buffer_out_) 
		: layer_scales(layer_scales_), image_scale(image_scale_), render_channels(render_channels_), buffer_out(buffer_out_) {}

	const std::vector<Vec3f>& layer_scales;
	float image_scale;
	const RenderChannels& render_channels; // Input image data
	Image4f& buffer_out;
};


class SumBuffersTask : public Indigo::Task
{
public:
	SumBuffersTask(const SumBuffersTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const bool have_alpha_channel = closure.render_channels.hasAlpha();

		if(closure.render_channels.hasSpectral())
		{
			for(int i = begin; i < end; ++i)
			{
				Colour4f sum(0.0f);
				for(ptrdiff_t z = 0; z < (ptrdiff_t)closure.render_channels.spectral.getN(); ++z) // For each wavelength bin:
				{
					const float wavelen = MIN_WAVELENGTH + (0.5f + z) * WAVELENGTH_SPAN / closure.render_channels.spectral.getN();
					const Vec3f xyz = SingleFreq::getXYZ_CIE_2DegForWavelen(wavelen); // TODO: pull out of the loop.
					const float pixel_comp_val = closure.render_channels.spectral.getData()[i * closure.render_channels.spectral.getN() + z] * closure.image_scale;
					sum[0] += xyz.x * pixel_comp_val;
					sum[1] += xyz.y * pixel_comp_val;
					sum[2] += xyz.z * pixel_comp_val;
				}
				sum.x[3] = 1;
				closure.buffer_out.getPixel(i) = sum;
			}
		}
		else
		{
			const int num_layers = (int)closure.render_channels.layers.size();

			for(int i = begin; i < end; ++i)
			{
				Colour4f sum(0.0f);
				for(int z = 0; z < num_layers; ++z)
				{
					const Vec3f& scale = closure.layer_scales[z];
					sum.x[0] += closure.render_channels.layers[z].image.getPixel(i).r * scale.x;
					sum.x[1] += closure.render_channels.layers[z].image.getPixel(i).g * scale.y;
					sum.x[2] += closure.render_channels.layers[z].image.getPixel(i).b * scale.z;
				}

				// Get alpha from alpha channel if it exists
				if(have_alpha_channel)
					sum.x[3] = closure.render_channels.alpha.getData()[i] * closure.image_scale;
				else
					sum.x[3] = 1.0f;

				closure.buffer_out.getPixel(i) = sum;
			}
		}
	}

	const SumBuffersTaskClosure& closure;
	int begin, end;
};


void sumLightLayers(
	const std::vector<Vec3f>& layer_scales, 
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	const RenderChannels& render_channels, // Input image data
	Image4f& summed_buffer_out, 
	Indigo::TaskManager& task_manager
)
{
	//Timer t;
	summed_buffer_out.resize(render_channels.layers[0].image.getWidth(), render_channels.layers[0].image.getHeight());

	const int num_pixels = (int)render_channels.layers[0].image.numPixels();

	SumBuffersTaskClosure closure(layer_scales, image_scale, render_channels, summed_buffer_out);

	task_manager.runParallelForTasks<SumBuffersTask, SumBuffersTaskClosure>(closure, 0, num_pixels);

	//std::cout << "sumBuffers: " << t.elapsedString() << std::endl;
}


struct ToneMapTaskClosure
{
	const RendererSettings* renderer_settings;
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

			closure.renderer_settings->tone_mapper->toneMapSubImage(*closure.tonemap_params, *closure.temp_summed_buffer, x_min, y_min, x_max, y_max);
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


// Tonemap HDR image to LDR image
void doTonemapFullBuffer(
	const RenderChannels& render_channels,
	const std::vector<Vec3f>& layer_weights,
	float image_scale, // A scale factor based on the number of samples taken and image resolution. (from PathSampler::getScale())
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	const Reference<PostProDiffraction>& post_pro_diffraction,
	Image4f& temp_summed_buffer,
	Image4f& temp_AD_buffer,
	Image4f& ldr_buffer_out,
	bool image_buffer_in_XYZ,
	int margin_ssf1, // Margin width (for just one side), in pixels, at ssf 1.  This may be zero for loaded LDR images. (PNGs etc..)
	float gamma,
	Indigo::TaskManager& task_manager)
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
		reinhard->computeLumiScales(XYZ_to_sRGB, render_channels, layer_weights, avg_lumi, max_lumi);

	const ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi, gamma);


	//if(PROFILE) t.reset();
	temp_summed_buffer.resize(render_channels.layers[0].image.getWidth(), render_channels.layers[0].image.getHeight());
	sumLightLayers(layer_weights, image_scale, render_channels, temp_summed_buffer, task_manager);
	//if(PROFILE) conPrint("\tsumBuffers: " + t.elapsedString());

	// Apply diffraction filter if applicable
	if(renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && post_pro_diffraction.nonNull())
	{
		BufferedPrintOutput bpo;
		temp_AD_buffer.resize(render_channels.layers[0].image.getWidth(), render_channels.layers[0].image.getHeight());
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
			const int filter_r = (int)ceil(renderer_settings.super_sample_factor * 5.0);

			// Get the value of the Gaussian at filter_r
			const float std_dev = (float)renderer_settings.super_sample_factor;
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
	else if(renderer_settings.material_id_tracer || renderer_settings.depth_pass)
	{
		for(size_t i = 0; i < temp_summed_buffer.numPixels(); ++i)
			temp_summed_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
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
		closure.renderer_settings = &renderer_settings;
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
	assert(temp_summed_buffer.maxPixelComponent() <= 1.0f);

	// Compute final dimensions of LDR image.
	// This is the size after the margins have been trimmed off, and the image has been downsampled.
	const size_t supersample_factor = (size_t)renderer_settings.super_sample_factor;
	const size_t border_width = (size_t)margin_ssf1;

	const size_t final_xres = render_channels.layers[0].image.getWidth()  / supersample_factor - border_width * 2; // assert(final_xres == renderer_settings.getWidth());
	const size_t final_yres = render_channels.layers[0].image.getHeight() / supersample_factor - border_width * 2; // assert(final_yres == renderer_settings.getWidth());
	ldr_buffer_out.resize(final_xres, final_yres);

	// Collapse super-sampled image down to final image size
	// NOTE: this trims off the borders
	if(supersample_factor > 1)
	{
		//if(PROFILE) t.reset();
		Image4f::downsampleImage(
			supersample_factor, // factor
			border_width,
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterSpan(renderer_settings.super_sample_factor),
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterData(renderer_settings.super_sample_factor),
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


	const bool dithering = renderer_settings.dithering;
	
	for(size_t z=0; z<ldr_buffer_out.numPixels(); ++z)
	{
		Colour4f col = ldr_buffer_out.getPixel(z);

		////// Dither ///////
		if(dithering)
			col = ditherPixel(col, z); 

		////// Do alpha divide //////
		col = max(Colour4f(0.0f), col); // Make sure alpha > 0

		const float recip_alpha = 1 / col.x[3];
		col *= Colour4f(recip_alpha, recip_alpha, recip_alpha, 1.0f);
		
		col = clamp(col, Colour4f(0.0f), Colour4f(1.0f));

		ldr_buffer_out.getPixel(z) = col;
	}

	// Components should be in range [0, 1]
	assert(ldr_buffer_out.minPixelComponent() >= 0.0f);
	assert(ldr_buffer_out.maxPixelComponent() <= 1.0f);

	// Zero out pixels not in the render region
	if(renderer_settings.render_region && renderer_settings.renderRegionIsValid())
		for(ptrdiff_t y = 0; y < (ptrdiff_t)ldr_buffer_out.getHeight(); ++y)
		for(ptrdiff_t x = 0; x < (ptrdiff_t)ldr_buffer_out.getWidth();  ++x)
			if( x < renderer_settings.render_region_x1 || x >= renderer_settings.render_region_x2 ||
				y < renderer_settings.render_region_y1 || y >= renderer_settings.render_region_y2)
				ldr_buffer_out.setPixel(x, y, Colour4f(0));
}


struct ImagePipelineTaskClosure
{
	std::vector<Image4f>* per_thread_tile_buffers;
	const RenderChannels* render_channels;
	const std::vector<Vec3f>* layer_weights;
	float image_scale;
	Image4f* ldr_buffer_out;
	const RendererSettings* renderer_settings;
	const float* resize_filter;
	const ToneMapperParams* tonemap_params;

	ptrdiff_t x_tiles, final_xres, final_yres, filter_size, margin_ssf1;
};


class ImagePipelineTask : public Indigo::Task
{
public:
	ImagePipelineTask(const ImagePipelineTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const bool have_alpha_channel = closure.render_channels->hasAlpha();

		//const ptrdiff_t x_tiles = closure.x_tiles;
		const ptrdiff_t final_xres = closure.final_xres;
		//const ptrdiff_t final_yres = closure.final_yres;
		const ptrdiff_t filter_size = closure.filter_size;

		const ptrdiff_t xres		= (ptrdiff_t)(closure.render_channels->layers)[0].image.getWidth();
		#ifndef  NDEBUG
		const ptrdiff_t yres		= (ptrdiff_t)(closure.render_channels->layers)[0].image.getHeight();
		#endif
		const ptrdiff_t ss_factor   = (ptrdiff_t)closure.renderer_settings->super_sample_factor;
		const ptrdiff_t gutter_pix  = (ptrdiff_t)closure.margin_ssf1; //closure.renderer_settings->getMargin();
		const ptrdiff_t filter_span = filter_size / 2 - 1;
		const bool      dithering   = closure.renderer_settings->dithering; // Compute outside loop.

		for(int tile = begin; tile < end; ++tile)
		{
			Image4f& tile_buffer = (*closure.per_thread_tile_buffers)[thread_index];

			// Get the final image tile bounds for the tile index
			const ptrdiff_t tile_x = (ptrdiff_t)tile % closure.x_tiles;
			const ptrdiff_t tile_y = (ptrdiff_t)tile / closure.x_tiles;
			const ptrdiff_t x_min  = tile_x * image_tile_size, x_max = std::min<ptrdiff_t>(closure.final_xres, (tile_x + 1) * image_tile_size);
			const ptrdiff_t y_min  = tile_y * image_tile_size, y_max = std::min<ptrdiff_t>(closure.final_yres, (tile_y + 1) * image_tile_size);

			// Perform downsampling if needed
			if(ss_factor > 1)
			{
				const ptrdiff_t bucket_min_x = (x_min + gutter_pix) * ss_factor + ss_factor / 2 - filter_span; assert(bucket_min_x >= 0);
				const ptrdiff_t bucket_min_y = (y_min + gutter_pix) * ss_factor + ss_factor / 2 - filter_span; assert(bucket_min_y >= 0);
				const ptrdiff_t bucket_max_x = ((x_max - 1) + gutter_pix) * ss_factor + ss_factor / 2 + filter_span + 1; assert(bucket_max_x <= xres);
				const ptrdiff_t bucket_max_y = ((y_max - 1) + gutter_pix) * ss_factor + ss_factor / 2 + filter_span + 1; assert(bucket_max_y <= yres);
				const ptrdiff_t bucket_span  = bucket_max_x - bucket_min_x;

				// First we get the weighted sum of all pixels in the layers
				size_t dst_addr = 0;
				for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
				for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
				{
					const ptrdiff_t src_addr = y * xres + x;
					Colour4f sum(0.0f);

					/*if(closure.render_channels->hasSpectral())
					{
						for(ptrdiff_t z = 0; z < (ptrdiff_t)closure.render_channels->spectral.getN(); ++z) // For each wavelength bin:
						{
							const float wavelen = MIN_WAVELENGTH + (0.5f + z) * WAVELENGTH_SPAN / closure.render_channels->spectral.getN();
							const Vec3f xyz = SingleFreq::getXYZ_CIE_2DegForWavelen(wavelen); // TODO: pull out of the loop.
							const float pixel_comp_val = closure.render_channels->spectral.getPixel((unsigned int)x, (unsigned int)y)[z] * closure.image_scale;
							sum[0] += xyz.x * pixel_comp_val;
							sum[1] += xyz.y * pixel_comp_val;
							sum[2] += xyz.z * pixel_comp_val;
						}
					}
					else*/
					{
						for(ptrdiff_t z = 0; z < (ptrdiff_t)closure.render_channels->layers.size(); ++z)
						{
							const Image::ColourType& c = (closure.render_channels->layers)[z].image.getPixel(src_addr);
							const Vec3f& s = (*closure.layer_weights)[z];

							sum.x[0] += c.r * s.x;
							sum.x[1] += c.g * s.y;
							sum.x[2] += c.b * s.z;
						}
					}

					// Get alpha from alpha channel if it exists
					if(have_alpha_channel)
					{
						const float raw_alpha = closure.render_channels->alpha.getPixel((unsigned int)x, (unsigned int)y)[0];
						sum.x[3] = ( (raw_alpha > 0) ? (1 + raw_alpha) : 0.f ) * closure.image_scale;
					}
					else
						sum.x[3] = 1.0f;

					tile_buffer.getPixel(dst_addr++) = sum;
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
						tile_buffer.getPixel(i) = Colour4f(0, 0, 0, 1 - unshadow_frac);
					}
				}
				else if(closure.renderer_settings->material_id_tracer || closure.renderer_settings->depth_pass)
				{
					const ptrdiff_t tile_buffer_pixels = tile_buffer.numPixels();
					for(ptrdiff_t i = 0; i < tile_buffer_pixels; ++i)
						tile_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
				}
				else
					closure.renderer_settings->tone_mapper->toneMapImage(*closure.tonemap_params, tile_buffer);

				// Filter processed pixels into the final image
				for(ptrdiff_t y = y_min; y < y_max; ++y)
				for(ptrdiff_t x = x_min; x < x_max; ++x)
				{
					const ptrdiff_t pixel_min_x = (x + gutter_pix) * ss_factor + ss_factor / 2 - filter_span - bucket_min_x;
					const ptrdiff_t pixel_min_y = (y + gutter_pix) * ss_factor + ss_factor / 2 - filter_span - bucket_min_y;
					const ptrdiff_t pixel_max_x = (x + gutter_pix) * ss_factor + ss_factor / 2 + filter_span - bucket_min_x + 1;
					const ptrdiff_t pixel_max_y = (y + gutter_pix) * ss_factor + ss_factor / 2 + filter_span - bucket_min_y + 1;

					uint32 filter_addr = 0;
					Colour4f weighted_sum(0);
					for(ptrdiff_t v = pixel_min_y; v < pixel_max_y; ++v)
					for(ptrdiff_t u = pixel_min_x; u < pixel_max_x; ++u)
						weighted_sum.addMult(tile_buffer.getPixel(v * bucket_span + u), closure.resize_filter[filter_addr++]);

					assert(isFinite(weighted_sum.x[0]) && isFinite(weighted_sum.x[1]) && isFinite(weighted_sum.x[2]));

					////// Dither ///////
					if(dithering)
						weighted_sum = ditherPixel(weighted_sum, y*xres + x); 

					////// Do alpha divide //////
					const float recip_alpha = 1 / weighted_sum.x[3];
					weighted_sum *= Colour4f(recip_alpha, recip_alpha, recip_alpha, 1.0f);

					weighted_sum.clampInPlace(0, 1); // Ensure result is in [0, 1]
					(*closure.ldr_buffer_out).getPixel(y * final_xres + x) = weighted_sum;
				}
			}
			else // No downsampling needed
			{
				// Since the pixels are 1:1 with the bucket bounds, simply offset the bucket rect by the left image margin / gutter pixels
				const ptrdiff_t bucket_min_x = x_min + gutter_pix;
				const ptrdiff_t bucket_min_y = y_min + gutter_pix;
				const ptrdiff_t bucket_max_x = x_max + gutter_pix; assert(bucket_max_x <= xres);
				const ptrdiff_t bucket_max_y = y_max + gutter_pix; assert(bucket_max_y <= yres);

				// First we get the weighted sum of all pixels in the layers
				size_t addr = 0;
				for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
				for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
				{
					const ptrdiff_t src_addr = y * xres + x;
					Colour4f sum(0.0f);

					if(closure.render_channels->hasSpectral())
					{
						for(ptrdiff_t z = 0; z < (ptrdiff_t)closure.render_channels->spectral.getN(); ++z) // For each wavelength bin:
						{
							const float wavelen = MIN_WAVELENGTH + (0.5f + z) * WAVELENGTH_SPAN / closure.render_channels->spectral.getN();
							const Vec3f xyz = SingleFreq::getXYZ_CIE_2DegForWavelen(wavelen); // TODO: pull out of the loop.
							const float pixel_comp_val = closure.render_channels->spectral.getPixel((unsigned int)x, (unsigned int)y)[z] * closure.image_scale;
							sum[0] += xyz.x * pixel_comp_val;
							sum[1] += xyz.y * pixel_comp_val;
							sum[2] += xyz.z * pixel_comp_val;
						}
					}
					else
					{
						for(ptrdiff_t z = 0; z < (ptrdiff_t)closure.render_channels->layers.size(); ++z)
						{
							const Image::ColourType& c = (closure.render_channels->layers)[z].image.getPixel(src_addr);
							const Vec3f& scale = (*closure.layer_weights)[z];

							sum.x[0] += c.r * scale.x;
							sum.x[1] += c.g * scale.y;
							sum.x[2] += c.b * scale.z;
						}
					}

					// Get alpha from alpha channel if it exists
					if(have_alpha_channel) // closure.renderer_settings->render_foreground_alpha)//closure.render_channels->alpha.getWidth() > 0)
					{
						const float raw_alpha = closure.render_channels->alpha.getPixel((unsigned int)x, (unsigned int)y)[0];
						sum.x[3] = ( (raw_alpha > 0) ? (1 + raw_alpha) : 0.f ) * closure.image_scale;
					}
					else
						sum.x[3] = 1.0f;

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
						tile_buffer.getPixel(i) = Colour4f(0, 0, 0, 1 - unshadow_frac);
					}
				}
				else if(closure.renderer_settings->material_id_tracer || closure.renderer_settings->depth_pass)
				{
					for(size_t i = 0; i < tile_buffer.numPixels(); ++i)
						tile_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
				}
				else
				{
					closure.renderer_settings->tone_mapper->toneMapImage(*closure.tonemap_params, tile_buffer);
				}
			

				// Copy processed pixels into the final image
				addr = 0;
				for(ptrdiff_t y = y_min; y < y_max; ++y)
				for(ptrdiff_t x = x_min; x < x_max; ++x)
				{
					Colour4f col = tile_buffer.getPixel(addr++);

					////// Dither ///////
					if(dithering)
						col = ditherPixel(col, y*xres + x); 

					////// Do alpha divide //////
					const float recip_alpha = 1 / col.x[3];
					col *= Colour4f(recip_alpha, recip_alpha, recip_alpha, 1.0f);
					
					col = clamp(col, Colour4f(0.0f), Colour4f(1.0f));

					(*closure.ldr_buffer_out).getPixel(y * final_xres + x) = col;
				}
			}
		}
	}

	const ImagePipelineTaskClosure& closure;
	int begin, end;
};


void doTonemap(	
	std::vector<Image4f>& per_thread_tile_buffers,
	const RenderChannels& render_channels,
	const std::vector<Vec3f>& layer_weights,
	float image_scale,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	const Reference<PostProDiffraction>& post_pro_diffraction,
	Image4f& temp_summed_buffer,
	Image4f& temp_AD_buffer,
	Image4f& ldr_buffer_out,
	bool XYZ_colourspace,
	int margin_ssf1,
	float gamma,
	Indigo::TaskManager& task_manager
	)
{
	// If diffraction filter needs to be appled, or the margin is zero (which is the case for numerical receiver mode), do non-bucketed tone mapping.
	// We do this for margin = 0 because the bucketed filtering code is not valid when margin = 0.
	if((renderer_settings.getMargin() == 0) || (renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && /*camera*/post_pro_diffraction.nonNull()))
	{
		doTonemapFullBuffer(render_channels, layer_weights, image_scale, renderer_settings, resize_filter, post_pro_diffraction, // camera,
							temp_summed_buffer, temp_AD_buffer,
							ldr_buffer_out, XYZ_colourspace, margin_ssf1, gamma, task_manager);
		return;
	}

	// Grab some unsigned constants for convenience
	const ptrdiff_t xres		= (ptrdiff_t)render_channels.layers[0].image.getWidth();
	const ptrdiff_t yres		= (ptrdiff_t)render_channels.layers[0].image.getHeight();
	const ptrdiff_t ss_factor   = (ptrdiff_t)renderer_settings.super_sample_factor;
	const ptrdiff_t filter_size = (ptrdiff_t)renderer_settings.getDownsizeFilterFuncNonConst()->getFilterSpan((int)ss_factor);

	// Compute final dimensions of LDR image.
	// This is the size after the margins have been trimmed off, and the image has been downsampled.
	const ptrdiff_t final_xres = RendererSettings::computeFinalWidth((int)xres, (int)ss_factor, margin_ssf1);
	const ptrdiff_t final_yres = RendererSettings::computeFinalWidth((int)yres, (int)ss_factor, margin_ssf1);
	assert(final_xres == renderer_settings.getWidth());
	assert(final_yres == renderer_settings.getHeight());
	ldr_buffer_out.resize(final_xres, final_yres);

	const ptrdiff_t x_tiles = Maths::roundedUpDivide<ptrdiff_t>(final_xres, (ptrdiff_t)image_tile_size - 1);
	const ptrdiff_t y_tiles = Maths::roundedUpDivide<ptrdiff_t>(final_yres, (ptrdiff_t)image_tile_size - 1);
	const ptrdiff_t num_tiles = x_tiles * y_tiles;
	const ptrdiff_t tile_buffer_size = (image_tile_size * ss_factor) + filter_size;


	// Ensure that we have sufficiently many buffers of sufficient size for as many threads as the task manager uses.
	per_thread_tile_buffers.resize(task_manager.getNumThreads());
	for(size_t tile_buffer = 0; tile_buffer < per_thread_tile_buffers.size(); ++tile_buffer)
	{
		per_thread_tile_buffers[tile_buffer].resize(tile_buffer_size, tile_buffer_size);
		per_thread_tile_buffers[tile_buffer].zero(); // NEW
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
		reinhard->computeLumiScales(XYZ_to_sRGB, render_channels, layer_weights, avg_lumi, max_lumi);

	ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi, gamma);

	ImagePipelineTaskClosure closure;
	closure.per_thread_tile_buffers = &per_thread_tile_buffers;
	closure.render_channels = &render_channels;
	closure.layer_weights = &layer_weights;
	closure.image_scale = image_scale;
	closure.ldr_buffer_out = &ldr_buffer_out;
	closure.renderer_settings = &renderer_settings;
	closure.resize_filter = resize_filter;
	closure.tonemap_params = &tonemap_params;
	closure.x_tiles = x_tiles;
	closure.final_xres = final_xres;
	closure.final_yres = final_yres;
	closure.filter_size = filter_size;
	closure.margin_ssf1 = margin_ssf1;

	task_manager.runParallelForTasks<ImagePipelineTask, ImagePipelineTaskClosure>(closure, 0, num_tiles);

	//conPrint("Image pipeline parallel loop took " + timer.elapsedString());

	// TEMP HACK: Chromatic abberation
	/*Image temp;
	ImageFilter::chromaticAberration(ldr_buffer_out, temp, 0.002f);
	ldr_buffer_out = temp;
	ldr_buffer_out.clampInPlace(0, 1);*/

	// Zero out pixels not in the render region
	if(renderer_settings.render_region && renderer_settings.renderRegionIsValid())
		for(ptrdiff_t y = 0; y < (ptrdiff_t)ldr_buffer_out.getHeight(); ++y)
		for(ptrdiff_t x = 0; x < (ptrdiff_t)ldr_buffer_out.getWidth();  ++x)
			if( x < renderer_settings.render_region_x1 || x >= renderer_settings.render_region_x2 ||
				y < renderer_settings.render_region_y1 || y >= renderer_settings.render_region_y2)
				ldr_buffer_out.setPixel(x, y, Colour4f(0.0f));
}


#ifdef BUILD_TESTS


static void checkToneMap(const int W, const int ssf, const RenderChannels& render_channels, Image4f& ldr_image_out, float image_scale)
{
	Indigo::TaskManager task_manager(1);

	// Temp buffers
	::Image4f temp_summed_buffer, temp_AD_buffer;
	std::vector< ::Image4f> temp_tile_buffers;

	const float layer_normalise = image_scale;
	std::vector<Vec3f> layer_weights(1, Vec3f(layer_normalise, layer_normalise, layer_normalise)); // No gain	

	RendererSettings renderer_settings;
	renderer_settings.logging = false;
	renderer_settings.setWidth(W);
	renderer_settings.setHeight(W);
	renderer_settings.super_sample_factor = ssf;
	renderer_settings.tone_mapper = new LinearToneMapper(1);
	renderer_settings.colour_space_converter = new ColourSpaceConverter(1.0/3.0, 1.0/3.0);
	renderer_settings.dithering = false; // Turn dithering off, otherwise will mess up tests

	ImagingPipeline::doTonemap(
		temp_tile_buffers,
		render_channels,
		layer_weights,
		layer_normalise, // image scale
		renderer_settings,
		renderer_settings.getDownsizeFilterFuncNonConst()->getFilterData(ssf),
		Reference<PostProDiffraction>(),
		temp_summed_buffer,
		temp_AD_buffer,
		ldr_image_out,
		false, // XYZ_colourspace
		renderer_settings.getMargin(), // margin at ssf1
		2.2f, // gamma
		task_manager
	);
}


void test()
{
	// Constant colour of (1,1,1), no alpha, ssf1
	{
		const int W = 1000;
		const int ssf = 1;
		const int full_W = RendererSettings::computeFullWidth(W, ssf, RendererSettings::defaultMargin());
		
		RenderChannels render_channels;
		render_channels.layers.push_back(Layer());
		render_channels.layers.back().image.resize(full_W, full_W);
		render_channels.layers.back().image.set(Colour3f(1.0f));

		Image4f ldr_buffer;
		const float image_scale = 1.f;
		checkToneMap(W, ssf, render_channels, ldr_buffer, image_scale);
		testAssert(ldr_buffer.getWidth() == W && ldr_buffer.getHeight() == W);
		
		for(int y=0; y<W; ++y)
		for(int x=0; x<W; ++x)
		{
			//printVar(ldr_buffer.getPixel(x, y));
			testAssert(epsEqual(ldr_buffer.getPixel(x, y), Colour4f(1, 1, 1, 1), 1.0e-4f));
		}
	}


	// Constant colour of (1,1,1), no alpha, ssf2
	{
		const int W = 1000;
		const int ssf = 2;
		const int full_W = RendererSettings::computeFullWidth(W, ssf, RendererSettings::defaultMargin());

		RenderChannels render_channels;
		render_channels.layers.push_back(Layer());
		render_channels.layers.back().image.resize(full_W, full_W);
		render_channels.layers.back().image.set(Colour3f(1.0f));

		Image4f ldr_buffer;
		const float image_scale = 1.f;
		checkToneMap(W, ssf, render_channels, ldr_buffer, image_scale);
		testAssert(ldr_buffer.getWidth() == W && ldr_buffer.getHeight() == W);
		
		for(int y=0; y<W; ++y)
		for(int x=0; x<W; ++x)
		{
			//printVar(ldr_buffer.getPixel(x, y));
			testAssert(epsEqual(ldr_buffer.getPixel(x, y), Colour4f(1, 1, 1, 1), 1.0e-4f));
		}
	}


	{
		const int W = 1000;
		const int ssf = 1;
		const int full_W = RendererSettings::computeFullWidth(W, ssf, RendererSettings::defaultMargin());

		// Alpha gets biased up in the imaging pipeline, so use a large multiplier to make the bias effect small.
		const float value_factor = 10000.f;

		// Try with constant colour of (0.2, 0.4, 0.6) and alpha 0.5
		const float alpha = 0.5f;
		RenderChannels render_channels;
		render_channels.layers.push_back(Layer());
		render_channels.layers.back().image.resize(full_W, full_W);
		render_channels.layers.back().image.set(Colour3f(0.2f, 0.4f, 0.6f) * value_factor);

		render_channels.alpha.resize(full_W, full_W, 1);
		render_channels.alpha.set(alpha * value_factor);

		Image4f ldr_buffer;
		const float image_scale = 1.f / value_factor;
		checkToneMap(W, ssf, render_channels, ldr_buffer, image_scale);
		testAssert(ldr_buffer.getWidth() == W && ldr_buffer.getHeight() == W);
		
		for(int y=0; y<W; ++y)
		for(int x=0; x<W; ++x)
		{
			/*
			gamma-corrected alpha is 0.5^(1/2.2) = 0.7297400528407231
			gamma-corrected red = 0.2^(1/2.2) = 0.4811565050522864

			so final red = 0.2^(1/2.2) / 0.5^(1/2.2) = (0.2/0.5)^(1/2.2) = 0.6593532905028939

			final green = (0.4/0.5)^(1/2.2) = 0.9035454309190944

			final blue = 1 as is clamped to 1.
			*/

			//printVar(ldr_buffer.getPixel(x, y));
			testAssert(epsEqual(ldr_buffer.getPixel(x, y), Colour4f(0.6593532905028939f, 0.9035454309190944f, 1.0f, 0.7297400528407231f), 1.0e-4f));
		}
	}




	{
	Indigo::TaskManager task_manager;

	const int test_res_num = 6;
	const int test_res[test_res_num] = { 1, 3, 5, 7, 11, 128 };

	const int test_ss_num = 3;
	const int test_ss[test_ss_num] = { 1, 2, 3 };

	const int test_layers_num = 2;
	const int test_layers[test_layers_num] = { 1, 2 };

	const int test_tonemapper_num = 3;
	std::vector<Reference<ToneMapper> > tone_mappers(test_tonemapper_num);
	try
	{
		tone_mappers[0] = Reference<ToneMapper>(new LinearToneMapper(1));
		tone_mappers[1] = Reference<ToneMapper>(new ReinhardToneMapper(4, 1, 6));
		tone_mappers[2] = Reference<ToneMapper>(new CameraToneMapper(0, 200, PlatformUtils::getResourceDirectoryPath() + "/data/camera_response_functions/dscs315.txt"));
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ToneMapperExcep& e)
	{
		failTest(e.what());
	}

	Reference<PostProDiffraction> post_pro_diffraction(NULL); // Don't test post_pro_diffraction currently


	for(int res = 0; res < test_res_num; ++res)
	for(int ss  = 0; ss  < test_ss_num; ++ss)
	for(int l   = 0; l   < test_layers_num; ++l)
	for(int tm  = 0; tm  < test_tonemapper_num; ++tm)
	{
		const int image_final_xres	= test_res[res];
		const int image_final_yres	= test_res[res];
		const int image_ss_factor	= test_ss[ss];
		const int image_layers		= test_layers[l];

		const int image_ss_xres = RendererSettings::computeFullWidth(image_final_xres, image_ss_factor, RendererSettings::defaultMargin());
		const int image_ss_yres = RendererSettings::computeFullHeight(image_final_yres, image_ss_factor, RendererSettings::defaultMargin());

		RendererSettings renderer_settings;
		renderer_settings.logging = false;
		renderer_settings.setWidth(image_final_xres);
		renderer_settings.setHeight(image_final_yres);
		renderer_settings.super_sample_factor = image_ss_factor;
		renderer_settings.tone_mapper = tone_mappers[tm];

		std::vector<std::string> layer_names(image_layers, "");

		MasterBuffer master_buffer(
			(uint32)image_ss_xres, 
			(uint32)image_ss_yres, 
			(int)image_layers,
			image_final_xres,
			image_final_yres,
			layer_names,
			false // need back buffer
		);
		master_buffer.setNumSamples(1);

		Indigo::Vector< ::Layer>& layers = master_buffer.getRenderChannels().layers;
		assert(layers.size() == image_layers);

		const float layer_normalise = 1.0f / image_layers;
		std::vector<Vec3f> layer_weights(image_layers, Vec3f(layer_normalise, layer_normalise, layer_normalise)); // No gain

		// Fill the layers with a solid circle
		const int r_max = image_ss_xres / 2;
		for(int i = 0; i < image_layers;  ++i)
		for(int y = 0; y < image_ss_yres; ++y)
		for(int x = 0; x < image_ss_xres; ++x)
		{
			const int dx = x - r_max, dy = y - r_max;

			layers[i].image.getPixel(y * image_ss_xres + x) = Colour3f((dx * dx + dy * dy < r_max * r_max) ? 1.0f : 0.0f);
		}

		// Fill alpha channel to alpha 1
		master_buffer.getRenderChannels().alpha.set(1.0f);

		::Image4f temp_summed_buffer, temp_AD_buffer, temp_ldr_buffer;
		std::vector< ::Image4f> temp_tile_buffers;

		doTonemap(
			temp_tile_buffers,
			master_buffer.getRenderChannels(),
			layer_weights,
			1.0f, // image scale
			renderer_settings,
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterData(image_ss_factor),
			post_pro_diffraction,
			temp_summed_buffer,
			temp_AD_buffer,
			temp_ldr_buffer,
			false,
			renderer_settings.getMargin(), // margin at ssf1
			2.2f, // gamma
			task_manager);


		testAssert(image_final_xres == (int)temp_ldr_buffer.getWidth());
		testAssert(image_final_yres == (int)temp_ldr_buffer.getHeight());

		// On the biggest image report a quantitative difference
		if(res == test_res_num - 1)
		{
			// Get the integral of all pixels in the image
			double pixel_sum = 0;
			for(size_t i = 0; i < temp_ldr_buffer.numPixels(); ++i)
				pixel_sum += temp_ldr_buffer.getPixel(i).x[0];
			const double integral = pixel_sum / (double)temp_ldr_buffer.numPixels();

			const double expected_sum = 0.78539816339744830961566084581988; // pi / 4
			const double allowable_error = 1200.0 / temp_ldr_buffer.numPixels();

			const double abs_error = fabs(expected_sum - integral);
			
			conPrint("Image integral: " + toString(integral) + ", abs error: " + toString(abs_error) + ", allowable error: " + toString(allowable_error));

			testAssert(abs_error <= allowable_error);
		}
	}

	}
}


#endif // BUILD_TESTS


} // namespace ImagingPipeline
