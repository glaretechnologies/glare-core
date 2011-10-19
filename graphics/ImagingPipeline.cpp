/*=====================================================================
ImagingPipeline.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Jul 13 13:44:31 +0100 2011
=====================================================================*/
#include "ImagingPipeline.h"


#include "../indigo/ColourSpaceConverter.h"
#include "../indigo/ToneMapper.h"
#include "../indigo/ReinhardToneMapper.h"
#include "../indigo/PostProDiffraction.h"
#include "../simpleraytracer/camera.h"
#include "../utils/platform.h"
#include <omp.h>
#include <iostream>


#ifdef BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/MasterBuffer.h"
#include "../indigo/LinearToneMapper.h"
#include "../indigo/CameraToneMapper.h"


#endif


namespace ImagingPipeline
{


void sumBuffers(const std::vector<Vec3f>& layer_scales, const Indigo::Vector<Image>& buffers, Image& buffer_out)
{
	//Timer t;
	buffer_out.resize(buffers[0].getWidth(), buffers[0].getHeight());

	#ifndef OSX
	#pragma omp parallel for
	#endif
	for(int i = 0; i < (int)buffers[0].numPixels(); ++i)
	{
		Image::ColourType c(0.0f);

		for(size_t z = 0; z < buffers.size(); ++z)
		{
			const Vec3f& scale = layer_scales[z];

			c.r += buffers[z].getPixel(i).r * scale.x;
			c.g += buffers[z].getPixel(i).g * scale.y;
			c.b += buffers[z].getPixel(i).b * scale.z;
		}

		buffer_out.getPixel(i) = c;
	}
	//std::cout << "sumBuffers: " << t.elapsedString() << std::endl;
}


// Tonemap HDR image to LDR image
void doTonemapFullBuffer(
	const Indigo::Vector<Image>& layers,
	const std::vector<Vec3f>& layer_weights,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	//Camera* camera,
	Reference<PostProDiffraction>& post_pro_diffraction,
	Image& temp_summed_buffer,
	Image& temp_AD_buffer,
	Image& ldr_buffer_out,
	bool image_buffer_in_XYZ)
{
	//Timer t;
	//const bool PROFILE = false;

	//if(PROFILE) t.reset();
	temp_summed_buffer.resize(layers[0].getWidth(), layers[0].getHeight());
	sumBuffers(layer_weights, layers, temp_summed_buffer);
	//if(PROFILE) print_messages_out.push_back("\tsumBuffers: " + t.elapsedString());

	// Apply diffraction filter if applicable
	if(renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && /*camera*/post_pro_diffraction.nonNull())
	{
		BufferedPrintOutput bpo;
		//if(PROFILE) t.reset();
		temp_AD_buffer.resize(layers[0].getWidth(), layers[0].getHeight());
		//camera->applyDiffractionFilterToImage(bpo, temp_summed_buffer, temp_AD_buffer);
		post_pro_diffraction->applyDiffractionFilterToImage(bpo, temp_summed_buffer, temp_AD_buffer);
		temp_summed_buffer = temp_AD_buffer;
		//for(size_t z = 0; z < bpo.msgs.size(); ++z)
		//	print_messages_out.push_back(bpo.msgs[z]);
		//if(PROFILE) print_messages_out.push_back("\tDiffraction filter application: " + t.elapsedString());
	}

	// Either tonemap, or do render foreground alpha
	if(renderer_settings.render_foreground_alpha || renderer_settings.material_id_tracer || renderer_settings.depth_pass)
	{
		for(size_t i = 0; i < temp_summed_buffer.numPixels(); ++i)
			temp_summed_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
	}
	else
	{
		//if(PROFILE) t.reset();
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
			reinhard->computeLumiScales(XYZ_to_sRGB, layers, layer_weights, avg_lumi, max_lumi);

		const ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi);
		renderer_settings.tone_mapper->toneMapImage(tonemap_params, temp_summed_buffer);
		//if(PROFILE) print_messages_out.push_back("\tTone map: " + t.elapsedString());
	}

	// Components should be in range [0, 1]
	assert(temp_summed_buffer.minPixelComponent() >= 0.0f);
	assert(temp_summed_buffer.maxPixelComponent() <= 1.0f);

	// Compute final dimensions of LDR image.
	// This is the size after the margins have been trimmed off, and the image has been downsampled.
	const size_t supersample_factor = (size_t)renderer_settings.super_sample_factor;
	const size_t border_width = (size_t)renderer_settings.getMargin();

	const size_t final_xres = layers[0].getWidth()  / supersample_factor - border_width * 2; // assert(final_xres == renderer_settings.getWidth());
	const size_t final_yres = layers[0].getHeight() / supersample_factor - border_width * 2; // assert(final_yres == renderer_settings.getWidth());
	ldr_buffer_out.resize(final_xres, final_yres);

	// Collapse super-sampled image down to final image size
	// NOTE: this trims off the borders
	if(supersample_factor > 1)
	{
		//if(PROFILE) t.reset();
		Image::downsampleImage(
			supersample_factor, // factor
			border_width,
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterSpan(renderer_settings.super_sample_factor),
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterData(renderer_settings.super_sample_factor),
			1.0f, // max component value
			temp_summed_buffer, // in
			ldr_buffer_out // out
			);
		//if(PROFILE) print_messages_out.push_back("\tcollapseImage: " + t.elapsedString());
	}
	else
	{
		// Copy to temp_buffer 3, removing border.
		const int b = RendererSettings::getMargin();

		temp_summed_buffer.blitToImage(b, b, (int)temp_summed_buffer.getWidth() - b, (int)temp_summed_buffer.getHeight() - b, ldr_buffer_out, 0, 0);
	}

	//conPrint("ldr_buffer_out.maxPixelComponent(): " + toString(ldr_buffer_out.maxPixelComponent()));

	assert(ldr_buffer_out.getWidth()  == final_xres); // renderer_settings.getWidth());
	assert(ldr_buffer_out.getHeight() == final_yres); // renderer_settings.getHeight());

	// Components should be in range [0, 1]
	assert(ldr_buffer_out.minPixelComponent() >= 0.0f);
	assert(ldr_buffer_out.maxPixelComponent() <= 1.0f);

	// Zero out pixels not in the render region
	if(renderer_settings.render_region)
		for(ptrdiff_t y = 0; y < (ptrdiff_t)ldr_buffer_out.getHeight(); ++y)
		for(ptrdiff_t x = 0; x < (ptrdiff_t)ldr_buffer_out.getWidth();  ++x)
			if( x < renderer_settings.render_region_x1 || x >= renderer_settings.render_region_x2 ||
				y < renderer_settings.render_region_y1 || y >= renderer_settings.render_region_y2)
				ldr_buffer_out.setPixel(x, y, Colour3f(0, 0, 0));
}


void doTonemap(	
	std::vector<Image>& per_thread_tile_buffers,
	const Indigo::Vector<Image>& layers,
	const std::vector<Vec3f>& layer_weights,
	const RendererSettings& renderer_settings,
	const float* const resize_filter,
	//Camera* camera,
	Reference<PostProDiffraction>& post_pro_diffraction,
	Image& temp_summed_buffer,
	Image& temp_AD_buffer,
	Image& ldr_buffer_out,
	bool XYZ_colourspace)
{
	// Apply diffraction filter if required
	if(renderer_settings.aperture_diffraction && renderer_settings.post_process_diffraction && /*camera*/post_pro_diffraction.nonNull())
	{
		doTonemapFullBuffer(layers, layer_weights, renderer_settings, resize_filter, post_pro_diffraction, // camera,
							temp_summed_buffer, temp_AD_buffer,
							ldr_buffer_out, XYZ_colourspace);
		return;
	}

	// Grab some unsigned constants for convenience
	const ptrdiff_t xres		= (ptrdiff_t)layers[0].getWidth();
	const ptrdiff_t yres		= (ptrdiff_t)layers[0].getHeight();
	const ptrdiff_t ss_factor   = (ptrdiff_t)renderer_settings.super_sample_factor;
	const ptrdiff_t gutter_pix  = (ptrdiff_t)renderer_settings.getMargin();
	const ptrdiff_t filter_size = (ptrdiff_t)renderer_settings.getDownsizeFilterFuncNonConst()->getFilterSpan((int)ss_factor);
	const ptrdiff_t filter_span = filter_size / 2 - 1;

	// Compute final dimensions of LDR image.
	// This is the size after the margins have been trimmed off, and the image has been downsampled.
	const ptrdiff_t stripped_xres = xres / ss_factor - gutter_pix * 2;
	const ptrdiff_t stripped_yres = yres / ss_factor - gutter_pix * 2;
	const ptrdiff_t final_xres = stripped_xres; //assert(final_xres == renderer_settings.getWidth());
	const ptrdiff_t final_yres = stripped_yres; //assert(final_yres == renderer_settings.getHeight());
	ldr_buffer_out.resize(final_xres, final_yres);

	const ptrdiff_t x_tiles = (final_xres + image_tile_size - 1) / image_tile_size; // rounded up :)
	const ptrdiff_t y_tiles = (final_yres + image_tile_size - 1) / image_tile_size;
	const ptrdiff_t num_tiles = x_tiles * y_tiles;
	const ptrdiff_t tile_buffer_size = (image_tile_size * ss_factor) + filter_size;


	// Ensure that we have sufficiently many buffers of sufficient size for as many threads as OpenMP will spawn
#ifndef OSX
	const size_t omp_num_threads = (size_t)omp_get_max_threads();
#else
	const size_t omp_num_threads = 1;
#endif
	per_thread_tile_buffers.resize(omp_num_threads);
	for(size_t tile_buffer = 0; tile_buffer < per_thread_tile_buffers.size(); ++tile_buffer)
		per_thread_tile_buffers[tile_buffer].resize(tile_buffer_size, tile_buffer_size);

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
		reinhard->computeLumiScales(XYZ_to_sRGB, layers, layer_weights, avg_lumi, max_lumi);

	ToneMapperParams tonemap_params(XYZ_to_sRGB, avg_lumi, max_lumi);


	#ifndef OSX
	#pragma omp parallel for// schedule(dynamic, 1)
	#endif
	for(int tile = 0; tile < (int)num_tiles; ++tile)
	{
#ifndef OSX
		const uint32 thread_num = (uint32_t)omp_get_thread_num();
#else
		const uint32 thread_num = 0;
#endif
		Image& tile_buffer = per_thread_tile_buffers[thread_num];

		// Get the final image tile bounds for the tile index
		const ptrdiff_t tile_x = (ptrdiff_t)tile % x_tiles;
		const ptrdiff_t tile_y = (ptrdiff_t)tile / x_tiles;
		const ptrdiff_t x_min  = tile_x * image_tile_size, x_max = std::min<ptrdiff_t>(final_xres, (tile_x + 1) * image_tile_size);
		const ptrdiff_t y_min  = tile_y * image_tile_size, y_max = std::min<ptrdiff_t>(final_yres, (tile_y + 1) * image_tile_size);

		// Perform downsampling if needed
		if(ss_factor > 1)
		{
			const ptrdiff_t bucket_min_x = (x_min + gutter_pix) * ss_factor + ss_factor / 2 - filter_span;
			const ptrdiff_t bucket_min_y = (y_min + gutter_pix) * ss_factor + ss_factor / 2 - filter_span;
			const ptrdiff_t bucket_max_x = ((x_max - 1) + gutter_pix) * ss_factor + ss_factor / 2 + filter_span + 1; assert(bucket_max_x <= xres);
			const ptrdiff_t bucket_max_y = ((y_max - 1) + gutter_pix) * ss_factor + ss_factor / 2 + filter_span + 1; assert(bucket_max_y <= yres);
			const ptrdiff_t bucket_span  = bucket_max_x - bucket_min_x;

			// First we get the weighted sum of all pixels in the layers
			size_t dst_addr = 0;
			for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
			for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
			{
				const ptrdiff_t src_addr = y * xres + x;
				Image::ColourType sum(0.0f);

				for(ptrdiff_t z = 0; z < (ptrdiff_t)layers.size(); ++z)
				{
					const Image::ColourType& c = layers[z].getPixel(src_addr);
					const Vec3f& s = layer_weights[z];

					sum.r += c.r * s.x;
					sum.g += c.g * s.y;
					sum.b += c.b * s.z;
				}

				tile_buffer.getPixel(dst_addr++) = sum;
			}

			// Either tonemap, or do render foreground alpha
			if(renderer_settings.render_foreground_alpha || renderer_settings.material_id_tracer || renderer_settings.depth_pass)
			{
				const ptrdiff_t tile_buffer_pixels = tile_buffer.numPixels();
				for(ptrdiff_t i = 0; i < tile_buffer_pixels; ++i)
					tile_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
			}
			else
				renderer_settings.tone_mapper->toneMapImage(tonemap_params, tile_buffer);

			// Filter processed pixels into the final image
			for(ptrdiff_t y = y_min; y < y_max; ++y)
			for(ptrdiff_t x = x_min; x < x_max; ++x)
			{
				const ptrdiff_t pixel_min_x = (x + gutter_pix) * ss_factor + ss_factor / 2 - filter_span - bucket_min_x;
				const ptrdiff_t pixel_min_y = (y + gutter_pix) * ss_factor + ss_factor / 2 - filter_span - bucket_min_y;
				const ptrdiff_t pixel_max_x = (x + gutter_pix) * ss_factor + ss_factor / 2 + filter_span - bucket_min_x + 1;
				const ptrdiff_t pixel_max_y = (y + gutter_pix) * ss_factor + ss_factor / 2 + filter_span - bucket_min_y + 1;

				uint32 filter_addr = 0;
				Colour3f weighted_sum(0);
				for(ptrdiff_t v = pixel_min_y; v < pixel_max_y; ++v)
				for(ptrdiff_t u = pixel_min_x; u < pixel_max_x; ++u)
					weighted_sum.addMult(tile_buffer.getPixel(v * bucket_span + u), resize_filter[filter_addr++]);

				assert(isFinite(weighted_sum.r) && isFinite(weighted_sum.g) && isFinite(weighted_sum.b));

				weighted_sum.clampInPlace(0, 1); // Ensure result is in [0, 1]
				ldr_buffer_out.getPixel(y * final_xres + x) = weighted_sum;
			}
		}
		else // No downsampling needed
		{
			// Since the pixels are 1:1 with the bucket bounds, simply offset the bucket rect by the left image margin / gutter pixels
			const ptrdiff_t bucket_min_x = x_min + gutter_pix;
			const ptrdiff_t bucket_min_y = y_min + gutter_pix;
			const ptrdiff_t bucket_max_x = x_max + gutter_pix; assert(bucket_max_x <= xres);
			const ptrdiff_t bucket_max_y = y_max + gutter_pix; assert(bucket_max_y <= yres);
			const ptrdiff_t bucket_span  = bucket_max_x - bucket_min_x;

			// First we get the weighted sum of all pixels in the layers
			size_t addr = 0;
			for(ptrdiff_t y = bucket_min_y; y < bucket_max_y; ++y)
			for(ptrdiff_t x = bucket_min_x; x < bucket_max_x; ++x)
			{
				const ptrdiff_t src_addr = y * xres + x;
				Image::ColourType sum(0.0f);

				for(ptrdiff_t z = 0; z < (ptrdiff_t)layers.size(); ++z)
				{
					const Image::ColourType& c = layers[z].getPixel(src_addr);
					const Vec3f& scale = layer_weights[z];

					sum.r += c.r * scale.x;
					sum.g += c.g * scale.y;
					sum.b += c.b * scale.z;
				}

				tile_buffer.getPixel(addr++) = sum;
			}

			// Either tonemap, or do render foreground alpha
			if(renderer_settings.render_foreground_alpha || renderer_settings.material_id_tracer || renderer_settings.depth_pass)
				for(size_t i = 0; i < tile_buffer.numPixels(); ++i)
					tile_buffer.getPixel(i).clampInPlace(0.0f, 1.0f);
			else
				renderer_settings.tone_mapper->toneMapImage(tonemap_params, tile_buffer);

			// Copy processed pixels into the final image
			addr = 0;
			for(ptrdiff_t y = y_min; y < y_max; ++y)
			for(ptrdiff_t x = x_min; x < x_max; ++x)
				ldr_buffer_out.getPixel(y * final_xres + x) = tile_buffer.getPixel(addr++);
		}
	}

	// Zero out pixels not in the render region
	if(renderer_settings.render_region)
		for(ptrdiff_t y = 0; y < (ptrdiff_t)ldr_buffer_out.getHeight(); ++y)
		for(ptrdiff_t x = 0; x < (ptrdiff_t)ldr_buffer_out.getWidth();  ++x)
			if( x < renderer_settings.render_region_x1 || x >= renderer_settings.render_region_x2 ||
				y < renderer_settings.render_region_y1 || y >= renderer_settings.render_region_y2)
				ldr_buffer_out.setPixel(x, y, Colour3f(0, 0, 0));
}


#ifdef BUILD_TESTS

void test()
{
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
		tone_mappers[2] = Reference<ToneMapper>(new CameraToneMapper(0, 200, "data/camera_response_functions/dscs315.txt"));
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

		const int image_ss_xres = RendererSettings::computeFullWidth(image_final_xres, image_ss_factor);
		const int image_ss_yres = RendererSettings::computeFullHeight(image_final_yres, image_ss_factor);

		RendererSettings renderer_settings;
		renderer_settings.logging = false;
		renderer_settings.setWidth(image_final_xres);
		renderer_settings.setHeight(image_final_yres);
		renderer_settings.super_sample_factor = image_ss_factor;
		renderer_settings.tone_mapper = tone_mappers[tm];


		MasterBuffer master_buffer((uint32)image_ss_xres, (uint32)image_ss_yres, (int)image_layers, 1, 1);
		master_buffer.setNumSamples(1);

		Indigo::Vector< ::Image>& layers = master_buffer.getBuffers();
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

			layers[i].getPixel(y * image_ss_xres + x) = Colour3f((dx * dx + dy * dy < r_max * r_max) ? 1.0f : 0.0f);
		}

		::Image temp_summed_buffer, temp_AD_buffer, temp_ldr_buffer;
		std::vector< ::Image> temp_tile_buffers;


		doTonemap(
			temp_tile_buffers,
			layers,
			layer_weights,
			renderer_settings,
			renderer_settings.getDownsizeFilterFuncNonConst()->getFilterData(image_ss_factor),
			post_pro_diffraction,
			temp_summed_buffer,
			temp_AD_buffer,
			temp_ldr_buffer,
			false);


		testAssert(image_final_xres == (int)temp_ldr_buffer.getWidth());
		testAssert(image_final_yres == (int)temp_ldr_buffer.getHeight());

		// On the biggest image report a quantitative difference
		if(res == test_res_num - 1)
		{
			// Get the integral of all pixels in the image
			double pixel_sum = 0;
			for(size_t i = 0; i < temp_ldr_buffer.numPixels(); ++i)
				pixel_sum += temp_ldr_buffer.getPixel(i).r;
			const double integral = pixel_sum / (double)temp_ldr_buffer.numPixels();

			const double expected_sum = 0.78539816339744830961566084581988; // pi / 4
			const double allowable_error = 1200.0 / temp_ldr_buffer.numPixels();

			const double abs_error = fabs(expected_sum - integral);
			
			std::cout << "Image integral: " << integral << ", abs error: " << abs_error << ", allowable error: " << allowable_error << std::endl;

			testAssert(abs_error <= allowable_error);
		}
	}
}

#endif


} // namespace ImagingPipeline
