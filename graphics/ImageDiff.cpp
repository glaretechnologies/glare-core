/*=====================================================================
ImageDiff.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-04-30 12:12:50 +0100
=====================================================================*/
#include "ImageDiff.h"


#include "PNGDecoder.h"
#include "Map2D.h"
#include "Image.h"
#include "ImageMap.h"
#include "Bitmap.h"
#include "imformatdecoder.h"
#include "TextDrawer.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"


// Throws Indigo::Exception
void ImageDiff::writeImageDiffOfPNGs(const std::string& indigo_base_dir_path, const std::string& image_a_path, const std::string& image_b_path, const std::string& output_png_path, float diff_scale)
{
	try
	{
		Reference<Map2D> a = PNGDecoder::decode(image_a_path);
		Reference<Map2D> b = PNGDecoder::decode(image_b_path);

		//NOTE: assuming loading 8-bit PNGs here.
		if(!dynamic_cast<ImageMap<uint8_t, UInt8ComponentValueTraits>* >(a.getPointer()))
			throw Indigo::Exception("image a was not 8-bit.");
		if(!dynamic_cast<ImageMap<uint8_t, UInt8ComponentValueTraits>* >(b.getPointer()))
			throw Indigo::Exception("image b was not 8-bit.");

		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > a_bitmap = a.downcast<ImageMap<uint8_t, UInt8ComponentValueTraits> >();
		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > b_bitmap = b.downcast<ImageMap<uint8_t, UInt8ComponentValueTraits> >();

		if((a_bitmap->getWidth() != b_bitmap->getWidth()) || (a_bitmap->getHeight() != b_bitmap->getHeight()) || (a_bitmap->getN() != b_bitmap->getN()))
			throw Indigo::Exception("Image dimensions or num components don't match.");

		Bitmap diff_image(a_bitmap->getWidth(), a_bitmap->getHeight(), a_bitmap->getN(), NULL);

		// Compute difference image

		for(unsigned int y=0; y<a_bitmap->getHeight(); ++y)
		for(unsigned int x=0; x<a_bitmap->getWidth(); ++x)
		for(unsigned int c=0; c<a_bitmap->getN(); ++c)
		{
			float a_val = (float)a_bitmap->getPixel(x, y)[c];
			float b_val = (float)b_bitmap->getPixel(x, y)[c];

			float scaled_diff = (b_val - a_val) * (1.f / 256) * diff_scale;

			uint8_t res_val = (uint8_t)myClamp<int>((int)(256 * (0.5f + scaled_diff)), 0, 255);
			
			diff_image.setPixelComp(x, y, c, res_val);
		}

		// Draw a reference neutral grey box in bottom right corner:
		const int W = 100;
		for(int y=myMax(0, (int)a_bitmap->getHeight() - W); y<(int)a_bitmap->getHeight(); ++y)
		for(int x=myMax(0, (int)a_bitmap->getWidth() - W);  x<(int)a_bitmap->getWidth();  ++x)
			for(unsigned int c=0; c<a_bitmap->getN(); ++c)
		{
			uint8_t val = (uint8_t)myClamp<int>((int)(256 * (0.5f)), 0, 255);

			diff_image.setPixelComp(x, y, c, val);
		}

		
		TextDrawerRef text_drawer = TextDrawerRef(new TextDrawer(
			FileUtils::join(indigo_base_dir_path, "font.png"),
			FileUtils::join(indigo_base_dir_path, "font.ini")
		));

		text_drawer->drawText("Reference grey", diff_image, 
			(int)a_bitmap->getWidth() - W + 10,
			(int)a_bitmap->getHeight() - W + 10
		);
		text_drawer->drawText("= zero diff", diff_image, 
			(int)a_bitmap->getWidth() - W + 10,
			(int)a_bitmap->getHeight() - W + 20
		);

		const std::string description = "'" + image_b_path + "' - '" + image_a_path + "', difference scale=" + ::toString(diff_scale);
		
		text_drawer->drawText("'" + image_b_path + "' - ", diff_image, 
			10,
			(int)a_bitmap->getHeight() - 24
		);
		text_drawer->drawText("'" + image_a_path + "', difference scale=" + ::toString(diff_scale), diff_image, 
			10,
			(int)a_bitmap->getHeight() - 12
		);


		PNGDecoder::write(diff_image, output_png_path);
	}
	catch(ImFormatExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
	catch(TextDrawerExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
}


// Old code for taking diff of two IGIs:

/*try
{
	std::vector<std::string> dummy_layer_names;
			
	IndigoImage a_inimage;
	Indigo::Vector<Image> a_layers;
	const std::string a_path = args.getArgStringValue("--diff", 0);
	a_inimage.read(a_path, a_layers, dummy_layer_names);

	Image a;
	const std::vector<Vec3f> a_scales(a_layers.size(), Vec3f((float)(1.0 / a_inimage.header.num_samples)));
	ImagingPipeline::sumBuffers(a_scales, a_layers, a, task_manager);
	//sumBuffers(renderer_settings.layer_controls, a_layers, a);
	//a.scale((float)(1.0 / a_inimage.header.num_samples));

	a.normalise(); //TEMP

	printVar(a_inimage.header.num_samples);
	conPrint(a_path + " average luminance: " + doubleToStringScientific(a.averageLuminance(), 7));

	IndigoImage b_inimage;
	Indigo::Vector<Image> b_layers;
	const std::string b_path = args.getArgStringValue("--diff", 1);
	b_inimage.read(b_path, b_layers, dummy_layer_names);

	Image b;
	const std::vector<Vec3f> b_scales(b_layers.size(), Vec3f((float)(1.0 / b_inimage.header.num_samples)));
	ImagingPipeline::sumBuffers(b_scales, b_layers, b, task_manager);
	//sumBuffers(renderer_settings.layer_controls, b_layers, b);
	//b.scale((float)(1.0 / b_inimage.header.num_samples));

	b.normalise(); //TEMP

	printVar(b_inimage.header.num_samples);
	conPrint(b_path + " average luminance: " + doubleToStringScientific(b.averageLuminance(), 7));

	a.subImage(b, 0, 0);

	conPrint("difference average luminance: " + doubleToStringScientific(a.averageLuminance(), 7));

	a.scale((float)::stringToDouble(args.getArgStringValue("--diff", 3)));

	for(size_t i = 0; i < a.numPixels(); ++i)
	{
		a.getPixel(i) += Colour3f(0.5);
		a.getPixel(i).clampInPlace(0.0, 1.0);
	}

	Bitmap ldr_a((unsigned int)a.getWidth(), (unsigned int)a.getHeight(), 3, NULL);
	a.copyRegionToBitmap(ldr_a, 0, 0, (int)a.getWidth(), (int)a.getHeight());
	std::map<std::string, std::string> metadata;
	//a.saveToPng(args.getArgValue("--diff", 2), metadata, 0);
	PNGDecoder::write(ldr_a, metadata, args.getArgStringValue("--diff", 2));
	exit(0);
}
catch(ImFormatExcep& e)
{
	throw IndigoDriverExcep("ImFormatExcep: " + e.what());
}
catch(IndigoImageExcep& e)
{
	throw IndigoDriverExcep("IndigoImageExcep: " + e.what());
}*/

