/*=====================================================================
ImagingPipelineTests.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-03-30 21:17:33 +0200
=====================================================================*/
#include "ImagingPipelineTests.h"



#if BUILD_TESTS
#include "ImagingPipeline.h"
#include "Image4f.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/TaskManager.h"
#include "../indigo/ColourSpaceConverter.h"
#include "../indigo/TestUtils.h"
#include "../indigo/MasterBuffer.h"
#include "../indigo/CameraToneMapper.h"
#include "../indigo/LinearToneMapper.h"
#include "../indigo/ReinhardToneMapper.h"
#include "../indigo/PostProDiffraction.h"
#endif



namespace ImagingPipelineTests
{


#ifdef BUILD_TESTS


static void checkToneMap(const int W, const int ssf, const RenderChannels& render_channels, Image4f& ldr_image_out, float image_scale)
{
	Indigo::TaskManager task_manager(1);

	// Temp buffers
	::Image4f temp_summed_buffer, temp_AD_buffer;
	std::vector< ::Image4f> temp_tile_buffers;

	const float layer_normalise = image_scale;
	std::vector<Vec3f> layer_weights(1, Vec3f(1.f)); // No gain	

	RendererSettings renderer_settings;
	renderer_settings.logging = false;
	renderer_settings.setWidth(W);
	renderer_settings.setHeight(W);
	renderer_settings.super_sample_factor = ssf;
	renderer_settings.tone_mapper = new LinearToneMapper(1);
	renderer_settings.colour_space_converter = new ColourSpaceConverter(1.0/3.0, 1.0/3.0);
	renderer_settings.dithering = false; // Turn dithering off, otherwise will mess up tests

	std::vector<float> filter_data;
	renderer_settings.getDownsizeFilterFunc().getFilterDataVec(renderer_settings.super_sample_factor, filter_data);

	std::vector<RenderRegion> render_regions;

	ImagingPipeline::doTonemap(
		temp_tile_buffers,
		render_channels,
		render_regions,
		layer_weights,
		layer_normalise, // image scale
		layer_normalise, // region image scale
		renderer_settings,
		filter_data.data(),
		Reference<PostProDiffraction>(),
		temp_summed_buffer,
		temp_AD_buffer,
		ldr_image_out,
		false, // XYZ_colourspace
		RendererSettings::defaultMargin(), // margin at ssf1
		task_manager
	);

	ImagingPipeline::toNonLinearSpace(task_manager, renderer_settings, ldr_image_out);
}


// Convert from linear sRGB to non-linear (compressed) sRGB.
// From https://en.wikipedia.org/wiki/SRGB
static float refLinearsRGBtosRGB(float x)
{
	if(x <= 0.0031308f)
		return 12.92f * x;
	else
		return (1 + 0.055f) * pow(x, 1 / 2.4f) - 0.055f;
}


void test()
{
	conPrint("ImagingPipeline::test()");

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
			const float expected_alpha = refLinearsRGBtosRGB(0.5f);
			const float expected_red = refLinearsRGBtosRGB(0.2f) / expected_alpha;
			const float expected_green = refLinearsRGBtosRGB(0.4f) / expected_alpha;
			const float expected_blue = 1.f; // Should be clamped to 1.

			//printVar(ldr_buffer.getPixel(x, y));
			//printVar(Colour4f(expected_red, expected_green, 1.0f, expected_alpha));
			testAssert(epsEqual(ldr_buffer.getPixel(x, y), Colour4f(expected_red, expected_green, expected_blue, expected_alpha), 2.0e-4f));
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
			RendererSettings::defaultMargin(), // margin width
			image_ss_factor,
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

		std::vector<float> filter_data;
		renderer_settings.getDownsizeFilterFunc().getFilterDataVec(renderer_settings.super_sample_factor, filter_data);

		ImagingPipeline::doTonemap(
			temp_tile_buffers,
			master_buffer.getRenderChannels(),
			std::vector<RenderRegion>(),
			layer_weights,
			1.0f, // image scale
			1.0f, // region image scale
			renderer_settings,
			filter_data.data(),
			post_pro_diffraction,
			temp_summed_buffer,
			temp_AD_buffer,
			temp_ldr_buffer,
			false,
			RendererSettings::defaultMargin(), // margin at ssf1
			task_manager);

		ImagingPipeline::toNonLinearSpace(task_manager, renderer_settings, temp_ldr_buffer);

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
			const double allowable_error = 1600.0 / temp_ldr_buffer.numPixels();

			const double abs_error = fabs(expected_sum - integral);
			
			conPrint("Image integral: " + toString(integral) + ", abs error: " + toString(abs_error) + ", allowable error: " + toString(allowable_error));

			testAssert(abs_error <= allowable_error);
		}
	}

	}
}


#endif // BUILD_TESTS


} // end namespace ImagingPipelineTests 


