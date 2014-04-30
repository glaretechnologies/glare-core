/*=====================================================================
ImageDiff.h
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-04-30 12:12:50 +0100
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
ImageDiff
-------------------
Computes the pixel differences between two PNGs.
Saves the resulting image (which may be scaled) to output_png_path.
=====================================================================*/
class ImageDiff
{
public:

	// Throws Indigo::Exception
	static void writeImageDiffOfPNGs(const std::string& indigo_base_dir_path, 
		const std::string& image_a_path, const std::string& image_b_path, const std::string& output_png_path, float diff_scale);

private:

};
