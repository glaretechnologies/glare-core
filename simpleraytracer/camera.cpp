/*=====================================================================
camera.cpp
----------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "camera.h"


#include "../maths/vec2.h"
#include "../maths/Matrix2.h"
#include "../raytracing/matutils.h"
#include "../indigo/SingleFreq.h"
#include "../indigo/ColourSpaceConverter.h"
#include "../utils/stringutils.h"
#include "../graphics/image.h"
#include "../graphics/ImageFilter.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../indigo/DiffractionFilter.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"
#include "../graphics/MitchellNetravali.h"
#include "../indigo/RendererSettings.h"
#include "../indigo/Distribution2.h"
#include "../indigo/Aperture.h"
#include "../indigo/CircularAperture.h"
#include "../utils/fileutils.h"
#include "../utils/MTwister.h"
#include "../indigo/IndigoImage.h"
#include "../graphics/imformatdecoder.h"
#include "../indigo/SpectralVector.h"


Camera::Camera(const Vec3d& pos_, const Vec3d& ws_updir, const Vec3d& forwards_, 
		double lens_radius_, double focus_distance_, double aspect_ratio_, double sensor_width_, double lens_sensor_dist_, 
		//const std::string& white_balance, 
		double bloom_weight_, double bloom_radius_, bool autofocus_, 
		bool polarising_filter_, double polarising_angle_,
		double glare_weight_, double glare_radius_, int glare_num_blades_,
		double exposure_duration_,
		Aperture* aperture_,
		const std::string& base_indigo_path,
		double lens_shift_up_distance_,
		double lens_shift_right_distance_
		)
:	pos(pos_),
	//ws_up(ws_updir),
	forwards(forwards_),
	lens_radius(lens_radius_),
	focus_distance(focus_distance_),
	aspect_ratio(aspect_ratio_),
	sensor_to_lens_dist(lens_sensor_dist_),
	bloom_weight(bloom_weight_),
	bloom_radius(bloom_radius_),
	autofocus(autofocus_),
	polarising_filter(polarising_filter_),
	polarising_angle(polarising_angle_),
	glare_weight(glare_weight_),
	glare_radius(glare_radius_),
	glare_num_blades(glare_num_blades_),
	exposure_duration(exposure_duration_),
//	colour_space_converter(NULL),
	diffraction_filter(NULL),
	aperture(aperture_),
	lens_shift_up_distance(lens_shift_up_distance_),
	lens_shift_right_distance(lens_shift_right_distance_)
{
	if(lens_radius <= 0.0)
		throw CameraExcep("lens_radius must be > 0.0");
	if(focus_distance <= 0.0)
		throw CameraExcep("focus_distance must be > 0.0");
	if(aspect_ratio <= 0.0)
		throw CameraExcep("aspect_ratio must be > 0.0");
	if(sensor_to_lens_dist <= 0.0)
		throw CameraExcep("sensor_to_lens_dist must be > 0.0");
	if(bloom_weight < 0.0)
		throw CameraExcep("bloom_weight must be >= 0.0");
	if(bloom_radius < 0.0)
		throw CameraExcep("bloom_radius must be >= 0.0");
	if(glare_weight < 0.0)
		throw CameraExcep("glare_weight must be >= 0.0");
	if(glare_radius < 0.0)
		throw CameraExcep("glare_radius must be >= 0.0");
	if(glare_weight > 0.0 && glare_num_blades <= 3)
		throw CameraExcep("glare_num_blades must be >= 3");
	if(exposure_duration <= 0.0)
		throw CameraExcep("exposure_duration must be > 0.0");

	up = ws_updir;
	up.removeComponentInDir(forwards);
	up.normalise();

	right = ::crossProduct(forwards, up);

	assert(forwards.isUnitLength());
	assert(ws_updir.isUnitLength());
	assert(up.isUnitLength());
	assert(right.isUnitLength());

	sensor_width = sensor_width_;
	sensor_height = sensor_width / aspect_ratio;

	lens_center = pos + up * lens_shift_up_distance + right * lens_shift_right_distance;
	lens_botleft = pos + up * (lens_shift_up_distance - lens_radius) + right * (lens_shift_right_distance - lens_radius);

	sensor_center = pos - forwards * sensor_to_lens_dist;
	sensor_botleft = sensor_center - up * sensor_height * 0.5 - right * sensor_width * 0.5;
	sensor_to_lens_dist_focus_dist_ratio = sensor_to_lens_dist / focus_distance;
	focus_dist_sensor_to_lens_dist_ratio = focus_distance / sensor_to_lens_dist;
	recip_sensor_width = 1.0 / sensor_width;
	recip_sensor_height = 1.0 / sensor_height;
	//uniform_lens_pos_pdf = 1.0 / (NICKMATHS_PI * lens_radius * lens_radius);
	uniform_sensor_pos_pdf = 1.0 / (sensor_width * sensor_height);

	recip_unoccluded_aperture_area = 1.0 / (4.0 * lens_radius * lens_radius);

	/*try
	{
		const Vec3d whitepoint = ColourSpaceConverter::whitepoint(white_balance);
		colour_space_converter = new ColourSpaceConverter(whitepoint.x, whitepoint.y);
	}
	catch(ColourSpaceConverterExcep& e)
	{
		throw CameraExcep("ColourSpaceConverterExcep: " + e.what());
	}/

	//------------------------------------------------------------------------
	//calculate polarising Vector
	//------------------------------------------------------------------------
	this->polarising_vec = getRightDir();
	
	const Vec2d components = Matrix2d::rotationMatrix(::degreeToRad(this->polarising_angle)) * Vec2d(1.0, 0.0);
	this->polarising_vec = getRightDir() * components.x + getUpDir() * components.y;
	assert(this->polarising_vec.isUnitLength());


	

/*	assert(!aperture_image);
	if(!circular_aperture_)
	{
		//TEMP
		Bitmap aperture_bitmap;
		PNGDecoder::decode("generated_aperture.png", aperture_bitmap);

		Array2d<double> aperture_visibility(aperture_bitmap.getWidth(), aperture_bitmap.getHeight());

		for(int y=0; y<aperture_bitmap.getHeight(); ++y)
			for(int x=0; x<aperture_bitmap.getWidth(); ++x)
				aperture_visibility.elem(x, y) = aperture_bitmap.getPixel(x, aperture_bitmap.getHeight() - 1 - y)[0] > 128 ? 1.0 : 0.0;//(float)aperture_bitmap.getPixel(x, y)[0] / 255.0f;

		aperture_image = new Distribution2(aperture_visibility);
	}*/
	
	// Save aperture preview to disk
	{
	
	Array2d<double> ap_vis;
	aperture->getVisibilityMap(ap_vis);

	Image aperture_preview_image(ap_vis.getWidth(), ap_vis.getHeight());

	for(unsigned int y=0; y<ap_vis.getHeight(); ++y)
		for(unsigned int x=0; x<ap_vis.getWidth(); ++x)
			aperture_preview_image.setPixel(x, y, Colour3f((float)ap_vis.elem(x, y)));


	std::map<std::string, std::string> metadata;
	try
	{
		//aperture_preview_image.saveToPng(FileUtils::join(base_indigo_path, "aperture_preview.png"), metadata, 0);
		Bitmap ldr_image(aperture_preview_image.getWidth(), aperture_preview_image.getHeight(), 3, NULL);
		aperture_preview_image.copyRegionToBitmap(ldr_image, 0, 0, aperture_preview_image.getWidth(), aperture_preview_image.getHeight());
		PNGDecoder::write(ldr_image, metadata, FileUtils::join(base_indigo_path, "aperture_preview.png"));
	}
	catch(ImageExcep& e)
	{
		conPrint("ImageExcep: " + e.what());
	}
	}


	// Construct RGB diffraction filter image if needed
	/*if(RendererSettings::getInstance().aperture_diffraction && RendererSettings::getInstance().post_process_diffraction)
	{
	}*/


	assert(aperture);
}


Camera::~Camera()
{
	delete aperture;
	//delete diffraction_filter;
	//delete colour_space_converter;
}


void Camera::prepareForDiffractionFilter(const std::string& base_indigo_path_, int main_buffer_width_, int main_buffer_height_)
{
	base_indigo_path = base_indigo_path_;
	main_buffer_width = main_buffer_width_;
	main_buffer_height = main_buffer_height_;
}


void Camera::buildDiffractionFilter(/*const std::string& base_indigo_path*/) const
{
	// Calculate diffraction distribution if needed.
	//try
	//{
		diffraction_filter = std::auto_ptr<DiffractionFilter>(new DiffractionFilter(
			lens_radius, 
			*aperture,
			base_indigo_path
			));
	//}
	//catch(DiffractionFilterExcep& e)
	//{
	//	throw CameraExcep(e.what());
	//}
}


void Camera::buildDiffractionFilterImage(/*int main_buffer_width, int main_buffer_height, MTwister& rng, const std::string& base_indigo_path*/) const
{
	assert(main_buffer_width > 0 && main_buffer_height > 0);

	conPrint("Creating diffraction filter image...");

	MTwister rng(1);

	// Copy scalar diffraction filter to 3-component image
	//Image filter(diffraction_filter->getDiffractionFilter().getWidth(), diffraction_filter->getDiffractionFilter().getHeight());
	//filter.zero();

	//for(int y=0; y<filter.getHeight(); ++y)
	//	for(int x=0; x<filter.getWidth(); ++x)
	//		filter.setPixel(x, y, Colour3f((float)diffraction_filter->getDiffractionFilter().elem(x, y)));

	//const double red_wavelength_m = 700.0e-9;
	//const double green_wavelength_m = 550.0e-9; 
	//const double blue_wavelength_m = 400.0e-9; 

	//void spectralConvolution(const Image& filter, const Vec3d& xyz_filter_scales, Image& result_out) const;

	//const int image_width = RendererSettings::getInstance().getWidth() * RendererSettings::getInstance().super_sample_factor; // TEMP PRETTY NASTY
	//const double pixel_scale_factor = this->sensor_to_lens_dist * (double)image_width / this->sensor_width;

	/*diffraction_filter_image = std::auto_ptr<Image>(new Image());

	Image::buildRGBFilter(
		filter, // original filter
		Vec3d(diffraction_filter->getXYScale(red_wavelength_m), diffraction_filter->getXYScale(green_wavelength_m), diffraction_filter->getXYScale(blue_wavelength_m)) * pixel_scale_factor / (double)filter.getWidth(),
		*diffraction_filter_image // result_out
		);*/

	// for each pixel of original image
	/*for(int y=0; y<filter.getHeight(); ++y)
		for(int x=0; x<filter.getWidth(); ++x)
		{
			const Vec2d normed_coords(
				(double)x / (double)filter.getWidth(),
				(double)y / (double)filter.getHeight()
				);

			const int NUM_WAVELENGTH_SAMPLES = 10;
				for(int i=0; i<NUM_WAVELENGTH_SAMPLES; ++i)
				{
					const double wvlen_nm = MIN_WAVELENGTH + ((double)i / (double)(NUM_WAVELENGTH_SAMPLES - 1)) * WAVELENGTH_SPAN;
					assert(Maths::inRange(wvlen_nm, MIN_WAVELENGTH, MAX_WAVELENGTH));

					const double wvlen_m = wvlen_nm * 1.0e-9;

					const double xy_scale = diffraction_filter->getXYScale(wvlen_m);

					const Vec2d scaled_normed_coords = Vec2d(0.5, 0.5) + (normed_coords - Vec2d(0.5, 0.5)) * xy_scale * pixel_scale_factor / (double)filter.getWidth();

					const Vec2d scaled_pixel_coords(scaled_normed_coords.x * (double)filter.getWidth(), scaled_normed_coords.y * (double)filter.getHeight());
					
					//Splat
					const int sx = (int)scaled_pixel_coords.x;
					const int sy = (int)scaled_pixel_coords.y;

					if(sx >= 0 && sx < filter.getWidth() && sy >= 0 && sy < filter.getHeight())
					{
						const double diff_value = diffraction_filter->getDiffractionFilter().elem(x, y);
						const Vec3d xyz_col = SingleFreq::getXYZ_CIE_2DegForWavelen(wvlen_nm);
						const Colour3f splatcol(
							(float)(diff_value * xyz_col.x),
							(float)(diff_value * xyz_col.y),
							(float)(diff_value * xyz_col.z)
							);

						filter.getPixel(sx, sy).add(splatcol);
					}
				}
		}*/

	
	//assert(!diffraction_filter_image->get())

	diffraction_filter_image = std::auto_ptr<Image>(new Image(
		513, //diffraction_filter->getDiffractionFilter().getWidth(),
		513 //diffraction_filter->getDiffractionFilter().getHeight()
		));
	//diffraction_filter_image->zero();


	const int sensor_x_res = main_buffer_width;
	const int sensor_y_res = main_buffer_height;

	//const int src_image_res = diffraction_filter->getDiffractionFilter().getWidth()
	
	//const double sensor_displament_for_angle = 1.0; // m / radian
	// Units of (m / radian) * m * src_pixels / m = pixels / radian
	//const double pixel_scale_factor = sensor_displament_for_angle * this->sensor_to_lens_dist * (double)src_image_res / this->sensor_width;

	MitchellNetravali<double> mn(
		0.6, // B
		0.2 // C
		);

	// max_x_angular_deviation = max pixel deviation divided by physical size of a pixel
	//NOTE: using small angle approximation here (even if not valid) because that's what the diffraction filter code will be doing as well
	const double sensor_pixel_width = this->sensor_width / (double)sensor_x_res;
	const double sensor_pixel_height = this->sensor_height / (double)sensor_y_res;
	
	//const double max_x_angular_deviation = ((double)diffraction_filter_image->getWidth() / 2.0) * ();
	//const double max_y_angular_deviation = ((double)diffraction_filter_image->getHeight() / 2.0) * (this->sensor_width / (double)sensor_y_res);

	// For each pixel in the XYZ diffraction map
	for(unsigned int y=0; y<diffraction_filter_image->getHeight(); ++y)
		for(unsigned int x=0; x<diffraction_filter_image->getWidth(); ++x)
		{
			// Get normalised coordinates of this pixel
			const Vec2d normed_coords(
				(double)x / (double)diffraction_filter_image->getWidth(),
				(double)y / (double)diffraction_filter_image->getHeight()
				);

			// Get distance from center pixel of filter
			const int d_pixel_x = (int)x - (int)diffraction_filter_image->getWidth() / 2;
			const int d_pixel_y = (int)y - (int)diffraction_filter_image->getHeight() / 2;

			// Get physical distance this corresponds to on sensor
			const double sensor_dist_x = (double)d_pixel_x * sensor_pixel_width;
			const double sensor_dist_y = (double)d_pixel_y * sensor_pixel_height;

			// What angular deviation does this correspond to?
			const double x_angle = sensor_dist_x / this->sensor_to_lens_dist;
			const double y_angle = sensor_dist_y / this->sensor_to_lens_dist;


			// For each of a few wavelengths chosen over the visible range...
			Vec3d sum(0.0, 0.0, 0.0);
			const int NUM_WAVELENGTH_SAMPLES = 20;
			for(int i=0; i<NUM_WAVELENGTH_SAMPLES; ++i)
			{
				// Sample a wavelength
				const double wvlen_nm = MIN_WAVELENGTH + (((double)i + rng.unitRandom()) / (double)NUM_WAVELENGTH_SAMPLES) * WAVELENGTH_SPAN;
				assert(Maths::inRange(wvlen_nm, MIN_WAVELENGTH, MAX_WAVELENGTH));
				const double wvlen_m = wvlen_nm * 1.0e-9;

				// Get the number of times bigger the dest features are than the source features (in pixels),

				// units of (radians / m) * (pixels / radian) = pixels / m
				//const double scale = diffraction_filter->getXYScale(wvlen_m) * pixel_scale_factor;

				// Get normalised coordinates of the corresponding point in the src image
				//const Vec2d src_normed_coords = Vec2d(0.5, 0.5) + (normed_coords - Vec2d(0.5, 0.5)) / scale;

				const double max_angular_deviations = diffraction_filter->getXYScale(wvlen_m);

				const Vec2d src_normed_coords = Vec2d(0.5, 0.5) + Vec2d(x_angle, y_angle) / max_angular_deviations;

				// Src pixel width (if diffraction pattern was projected onto sensor)
				const double src_pixel_width = (max_angular_deviations * this->sensor_to_lens_dist) / ((double)diffraction_filter->getDiffractionFilter().getWidth() * 0.5);

				// Pixel scale.  This will be > 1 if source pixels are larger than destination pixels
				const double x_scale = src_pixel_width / sensor_pixel_width;
				const double y_scale = src_pixel_width / sensor_pixel_height;

				// Convert to pixel coords
				const Vec2d src_pixel_coords(
					src_normed_coords.x * (double)diffraction_filter->getDiffractionFilter().getWidth(), 
					src_normed_coords.y * (double)diffraction_filter->getDiffractionFilter().getHeight());

				// Find the center pixel of the sampling filter in the source image.
				const int center_sx = Maths::floorToInt(src_pixel_coords.x);
				const int center_sy = Maths::floorToInt(src_pixel_coords.y);

				const Vec3d xyz_col = toVec3d(SingleFreq::getXYZ_CIE_2DegForWavelen(wvlen_nm));

				if(x_scale > 1.0)
				{
					// Just point sample
					if(center_sx > 0 && center_sx < (int)diffraction_filter->getDiffractionFilter().getWidth() &&
						center_sy > 0 && center_sy < (int)diffraction_filter->getDiffractionFilter().getHeight())
					{
						const double diff_value = diffraction_filter->getDiffractionFilter().elem(center_sx, center_sy);
						sum += xyz_col * diff_value;
					}
				}
				else
				{
					
					// Using MN filter radius, in units of source image pixels.
					const double rad_x = myMax(2.0 / x_scale, 2.0);
					const double rad_y = myMax(2.0 / y_scale, 2.0);
					const int filter_rad_x = (int)ceil(rad_x);
					const int filter_rad_y = (int)ceil(rad_y);

					// Get the min and max bounds of the filter support on the source image.
					const int min_sx = myMax(0, center_sx - filter_rad_x);
					const int min_sy = myMax(0, center_sy - filter_rad_y);
					const int max_sx = myMin((int)diffraction_filter->getDiffractionFilter().getWidth(), center_sx + filter_rad_x + 1);
					const int max_sy = myMin((int)diffraction_filter->getDiffractionFilter().getHeight(), center_sy + filter_rad_y + 1);

					

					for(int sy=min_sy; sy<max_sy; ++sy)
					{
						const double dy = fabs(((double)sy - src_pixel_coords.y) / rad_y);
						//assert(dy <= 3.0);
						const double filter_y = mn.eval(dy);

						for(int sx=min_sx; sx<max_sx; ++sx)
						{
							const double dx = fabs(((double)sx - src_pixel_coords.x) / rad_x);
							//assert(dx <= 3.0);
							const double filter_x = mn.eval(dx);
							
							const double filter_val = filter_x * filter_y;

							const double diff_value = diffraction_filter->getDiffractionFilter().elem(sx, sy);

							sum += xyz_col * diff_value * filter_val;
						}
					}
				}
			}

			diffraction_filter_image->getPixel(x, y) = Colour3f((float)sum.x, (float)sum.y, (float)sum.z);
			diffraction_filter_image->getPixel(x, y).lowerClampInPlace(0.0f);
		}


	// Normalise filter image
	double X_sum = 0.0, Y_sum = 0.0, Z_sum = 0.0;
	for(unsigned int i=0; i<diffraction_filter_image->numPixels(); ++i)
	{
		//filter.getPixel(i)
		X_sum += (double)diffraction_filter_image->getPixel(i).r;
		Y_sum += (double)diffraction_filter_image->getPixel(i).g;
		Z_sum += (double)diffraction_filter_image->getPixel(i).b;
	}

	assert(X_sum > 0.0 && Y_sum > 0.0 && Z_sum > 0.0);
	const float X_scale = (float)(1.0 / X_sum);
	const float Y_scale = (float)(1.0 / Y_sum);
	const float Z_scale = (float)(1.0 / Z_sum);
	for(unsigned int i=0; i<diffraction_filter_image->numPixels(); ++i)
	{
		diffraction_filter_image->getPixel(i).r *= X_scale;
		diffraction_filter_image->getPixel(i).g *= Y_scale;
		diffraction_filter_image->getPixel(i).b *= Z_scale;
	}

	//TEMP HACK: move everything down to the right a bit
	//Image temp = *diffraction_filter_image;
	//diffraction_filter_image->zero();
	//temp.blitToImage(*diffraction_filter_image, 1, 1);
	
	//TEMP HACK: 
	//diffraction_filter_image->zero();
	//diffraction_filter_image->setPixel(255, 255, Colour3f(1.0f));

	


	//diffraction_filter_image = std::auto_ptr<Image>(new Image(filter));

	// for each sampled wavelength across visible spectrum
		// get XYZ weights
			//splat onto XYZ filter
				

// normalise filter for X, Y, Z separately.


	//TEMP: save diffraction image-------------------------------------------------------
	Image save_image = *diffraction_filter_image;

	//IndigoImage igi;
	//igi.write(save_image, 1.0, 1, "XYZ_diffraction_preview.igi");

	//save_image.scale(100000.0f);
	save_image.scale(10000.0f / save_image.maxLuminance());
	save_image.posClamp();

	try
	{
		//save_image.saveToPng(FileUtils::join(preview_save_dir, "diffraction_preview.png"), dummy_metadata, 0);
		Bitmap ldr_image(save_image.getWidth(), save_image.getHeight(), 3, NULL);
		save_image.copyRegionToBitmap(ldr_image, 0, 0, save_image.getWidth(), save_image.getHeight());
		std::map<std::string, std::string> dummy_metadata;
		PNGDecoder::write(ldr_image, dummy_metadata, FileUtils::join(base_indigo_path, "XYZ_diffraction_preview.png"));
	}
	catch(ImageExcep& e)
	{
		conPrint("ImageExcep: " + e.what());
	}
	catch(ImFormatExcep& e)
	{
		conPrint("ImageExcep: " + e.what());
	}
	//---------------------------------------------------------------------------------------

	conPrint("\tDone.");


}



const Vec3d Camera::sampleSensor(const SamplePair& samples) const
{
	return sensor_botleft + 
		right * samples.x * sensor_width + 
		up * samples.y * sensor_height;
}

double Camera::sensorPDF(const Vec3d& p) const
{
	assert(epsEqual(uniform_sensor_pos_pdf, 1.0 / (sensor_width * sensor_height)));
	return uniform_sensor_pos_pdf;
}

const Vec3d Camera::sampleLensPos(const SamplePair& samples) const
{
/*	if(aperture_image)
	{
		//const Vec2d offset = aperture_image->sample(samples) * lens_radius * 2.0;
		const Vec2d normed_lenspos = aperture_image->sample(samples);

		assert(aperture_image->value(normed_lenspos) > 0.0);

		return lens_center + 
			up * (2.0 * normed_lenspos.y - 1.0) * lens_radius + 
			right * (2.0 * normed_lenspos.x - 1.0) * lens_radius;
	}
	else
	{
		const Vec2d discpos = MatUtils::sampleUnitDisc(samples) * lens_radius;
		return lens_center + up * discpos.y + right * discpos.x;
	}*/

	assert(aperture->sampleAperture(samples).inHalfClosedInterval(0.0, 1.0));

	const Vec2d normed_lenspos = aperture->sampleAperture(samples);

	return lens_center + 
			up * (2.0 * normed_lenspos.y - 1.0) * lens_radius + 
			right * (2.0 * normed_lenspos.x - 1.0) * lens_radius;
}

double Camera::lensPosPDF(const Vec3d& lenspos) const
{
/*	if(aperture_image)
	{
		// Then the lens rectangle was sampled uniformly.
		// so pdf = 1 / 2d
		//return 1.0 / (4.0 * lens_radius*lens_radius);

		const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos);
		assert(normed_lenspoint.x >= 0.0 && normed_lenspoint.x < 1.0 && normed_lenspoint.y >= 0.0 && normed_lenspoint.y < 1.0);

		assert(aperture_image->value(normed_lenspoint) > 0.0);
		assert(isFinite(aperture_image->value(normed_lenspoint)));
		return aperture_image->value(normed_lenspoint) / (4.0 * lens_radius * lens_radius); // TEMP TODO: precompute divide
	}
	else
	{
		assert(lens_radius > 0.0);
		assert(epsEqual(uniform_lens_pos_pdf, 1.0 / (NICKMATHS_PI * lens_radius * lens_radius)));
		return uniform_lens_pos_pdf;
	}*/


	const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos);
	assert(normed_lenspoint.inHalfClosedInterval(0.0, 1.0));
	
	assert(aperture->pdf(normed_lenspoint) > 0.0);
	assert(isFinite(aperture->pdf(normed_lenspoint)));

	//return aperture->pdf(normed_lenspoint) / (4.0 * lens_radius * lens_radius); // TEMP TODO: precompute divide
	assert(epsEqual(recip_unoccluded_aperture_area, 1.0 / (4.0 * lens_radius * lens_radius)));
	return aperture->pdf(normed_lenspoint) * recip_unoccluded_aperture_area;
}

double Camera::lensPosSolidAnglePDF(const Vec3d& sensorpos, const Vec3d& lenspos) const
{
	//NOTE: wtf is up with this func???

	const double pdf_A = lensPosPDF(lenspos);

	//NOTE: this is a very slow way of doing this!!!!
	const double costheta = fabs(dot(forwards, normalise(lenspos - sensorpos)));

	return MatUtils::areaToSolidAnglePDF(pdf_A, lenspos.getDist2(sensorpos), costheta);

	/*const float costheta = fabs(dot(forwards, normalise(lenspos - sensorpos)));

	const float pdf_solidangle = pdf_A * sensor_to_lens_dist * sensor_to_lens_dist / (costheta * costheta * costheta);

	return pdf_solidangle;*/
}

const Vec3d Camera::lensExitDir(const Vec3d& sensorpos, const Vec3d& lenspos) const
{
	// The target point can be found by following the line from sensorpos, through the lens center,
	// and for a distance of focus_distance.

	const double sensor_up = distUpOnSensorFromCenter(sensorpos);
	const double sensor_right = distRightOnSensorFromCenter(sensorpos);

	const double target_up_dist = (lens_shift_up_distance - sensor_up) * focus_dist_sensor_to_lens_dist_ratio;
	const double target_right_dist = (lens_shift_right_distance - sensor_right) * focus_dist_sensor_to_lens_dist_ratio;

	const Vec3d target_point = lens_center + forwards * focus_distance + 
		up * target_up_dist + 
		right * target_right_dist;

	return normalise(target_point - lenspos);
}

double Camera::lensPosVisibility(const Vec3d& lenspos) const
{
	/*if(aperture_image)
	{
		const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos);

		if(normed_lenspoint.x < 0.0 || normed_lenspoint.x >= 1.0 || normed_lenspoint.y < 0.0 || normed_lenspoint.y >= 1.0)
			return 0.0;

		assert(aperture_image->value(normed_lenspoint) == 0.0 || aperture_image->value(normed_lenspoint) == 1.0);
		return aperture_image->value(normed_lenspoint);

		//const int ap_image_x = (int)(normed_lenspoint.x * aperture_image->getWidth());
		//if(ap_image_x < 0 || ap_image_x >= aperture_image->getWidth())
		//	return 0.0;

		//const int ap_image_y = (int)((1.0 - normed_lenspoint.y) * aperture_image->getHeight()); // y increases downwards in aperture_image
		//if(ap_image_y < 0 || ap_image_y >= aperture_image->getHeight())
		//	return 0.0;

		//assert(Maths::inRange(aperture_image->elem(ap_image_x, ap_image_y), 0.f, 1.f));
		//return (double)aperture_image->elem(ap_image_x, ap_image_y);

	}
	else
	{
		return 1.0;
	}*/

	const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos);

	if(normed_lenspoint.x < 0.0 || normed_lenspoint.x >= 1.0 || normed_lenspoint.y < 0.0 || normed_lenspoint.y >= 1.0)
		return 0.0;

	return aperture->visibility(normed_lenspoint);
}


const Vec3d Camera::sensorPosForLensIncidentRay(const Vec3d& lenspos, const Vec3d& raydir, bool& hitsensor_out) const
{
	assert(raydir.isUnitLength());

	hitsensor_out = true;

	// Follow ray back to focus_distance away to the target point
	const double lens_up = distUpOnLensFromCenter(lenspos);
	const double lens_right = distRightOnLensFromCenter(lenspos);
	const double forwards_comp = -dot(raydir, forwards);

	if(forwards_comp <= 0.0)
	{
		hitsensor_out = false;
		return Vec3d(0,0,0);
	}
	const double ray_dist_to_target_plane = focus_distance / forwards_comp;
	
	// Compute the up and right components of target point
	const double target_up = lens_up - dot(raydir, up) * ray_dist_to_target_plane;
	const double target_right = lens_right - dot(raydir, right) * ray_dist_to_target_plane;

	// Compute corresponding sensorpos
	const double sensor_up = -target_up * sensor_to_lens_dist_focus_dist_ratio + lens_shift_up_distance;
	const double sensor_right = -target_right * sensor_to_lens_dist_focus_dist_ratio + lens_shift_right_distance;

	if(fabs(sensor_up) > sensor_height * 0.5 || fabs(sensor_right) > sensor_width * 0.5)
	{
		hitsensor_out = false;
		return Vec3d(0,0,0);
	}

	return sensor_center + up * sensor_up + right * sensor_right;
}

const Vec2d Camera::imCoordsForSensorPos(const Vec3d& sensorpos) const
{
	const double sensor_up_dist = dot(sensorpos - sensor_botleft, up);
	const double sensor_right_dist = dot(sensorpos - sensor_botleft, right);
	
	return Vec2d(1.0 - (sensor_right_dist * recip_sensor_width), sensor_up_dist * recip_sensor_height);
}

const Vec3d Camera::sensorPosForImCoords(const Vec2d& imcoords) const
{
	return sensor_botleft + 
		up * imcoords.y * sensor_height + 
		right * (1.0 - imcoords.x) * sensor_width;
}










double Camera::traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	return -1.0f;//TEMP
}

js::AABBox tempzero(Vec3f(0,0,0), Vec3f(0,0,0));

const js::AABBox& Camera::getAABBoxWS() const
{
	return tempzero;
}


void Camera::getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	return;
}
bool Camera::doesFiniteRayHit(const Ray& ray, double raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const
{
	return false;
}




const std::string Camera::getName() const { return "Camera"; }


const Camera::Vec3Type Camera::getShadingNormal(const HitInfo& hitinfo) const { return toVec3f(forwards); }


const Camera::Vec3Type Camera::getGeometricNormal(const HitInfo& hitinfo) const { return toVec3f(forwards); }


unsigned int Camera::getNumTexCoordSets() const { return 0; }


unsigned int Camera::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


/*
void Camera::lookAt(const Vec3& target)
{
	setForwardsDir(normalise(target - getPos()));
}

void Camera::setForwardsDir(const Vec3& forwards_)
{
	forwards = forwards_;
	rightdir = ::crossProduct(forwards, current_up);
}
*/


//for a ray heading into the camera
/*const Vec2 Camera::getNormedImagePointForRay(const Vec3& unitray, bool& fell_on_image_out) const
{	
	fell_on_image_out = true;
	//assert(0);
	//return Vec2(0.5f, 0.5f);
	const float forwardcomp = unitray.dot(this->getForwardsDir()) * -1.0f;
	if(forwardcomp <= 0.0f)
		fell_on_image_out = false;

	const float upcomp = unitray.dot(this->up) / forwardcomp;
	const float rightcomp = unitray.dot(this->right) / forwardcomp;

	const Vec2 coords(-rightcomp/width + 0.5f, upcomp/height + 0.5f);

	fell_on_image_out = fell_on_image_out && 
		(coords.x >= 0.0f && coords.x <= 1.0f && coords.y >= 0.0f && coords.y <= 1.0f);

	return coords;
}*/
	
const Vec3d Camera::getRayUnitDirForNormedImageCoords(double x, double y) const
{
	::fatalError("Camera::getRayUnitDirForNormedImageCoords");
	return Vec3d(0,0,0);
	//return normalise(getForwardsDir() + getRightDir() * (-width_2 + width*x)
	//			+ getCurrentUpDir() * -(-height_2 + height*y));
}

const Vec3d Camera::getRayUnitDirForImageCoords(double x, double y, double width, double height) const
{
	::fatalError("Camera::getRayUnitDirForImageCoords");
	/*const float xfrac = x / width;
	const float yfrac = y / height;

	return getRayUnitDirForNormedImageCoords(xfrac, yfrac);*/
	return Vec3d(0,0,0);
}



//assuming the image plane is sampled uniformly
double Camera::getExitRaySolidAnglePDF(const Vec3d& dir) const
{
	::fatalError("Camera::getExitRaySolidAnglePDF");
	return 1.0f;
	/*
	const float image_area = width * height;
	const float pdf_A = 1.0f / image_area;

	const float costheta = dot(getForwardsDir(), dir);

	//pdf_sa = pdf_A * d^2 / cos(theta)
	//d = 1 /cos(theta)
	//d^2 = 1 / cos(theta)^2
	const float pdf_solidangle = pdf_A / (costheta * costheta * costheta);
	return pdf_solidangle;*/
}


/*void Camera::convertFromXYZToSRGB(Image& image) const
{
	assert(colour_space_converter);
	colour_space_converter->convertFromXYZToSRGB(image);
}
*/

void Camera::setFocusDistance(double fd)
{
	this->focus_distance = fd;

	// recompute precomputed constants
	sensor_to_lens_dist_focus_dist_ratio = sensor_to_lens_dist / focus_distance;
	focus_dist_sensor_to_lens_dist_ratio = focus_distance / sensor_to_lens_dist;
}

void Camera::emitterInit()
{
	::fatalError("Cameras may not be emitters.");
}

const Vec3d Camera::sampleSurface(const SamplePair& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										  HitInfo& hitinfo_out) const
{
	assert(0);
	return Vec3d(0.f, 0.f, 0.f);
}

double Camera::surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const
{
	assert(0);
	return 0.f;
}

double Camera::surfaceArea(const Matrix3d& to_parent) const
{
	::fatalError("Camera::surfaceArea()");
	return 0.f;
}

const Camera::TexCoordsType Camera::getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	return TexCoordsType(0,0);
}


void Camera::subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, 
		const std::vector<Plane<double> >& camera_clip_planes){}


void Camera::build(const std::string& indigo_base_dir_path, const RendererSettings& settings) {} // throws GeometryExcep


/*int Camera::UVSetIndexForName(const std::string& uvset_name) const
{
	::fatalError("Camera::UVSetIndexForName");
	return 0;
}*/



const Vec3d Camera::diffractRay(const SamplePair& samples, const Vec3d& dir, const SpectralVector& wavelengths, double direction_sign, SpectralVector& weights_out) const
{
	//assert(RendererSettings::getInstance().aperture_diffraction);
	if(diffraction_filter.get() == NULL)
	{
		// Lazily construct diffraction filter + filter image
		buildDiffractionFilter();
	}

	assert(diffraction_filter.get() != NULL);


	//if(!this->diffraction_filter || !RendererSettings::getInstance().aperture_diffraction)
	//{
	//	weights_out.set(1.0);
	//	return dir;
	//}
	
	// Form a basis with k in direction of ray, using cam right as i
	const Vec3d k = dir;
	Vec3d i = getRightDir();
	i.removeComponentInDir(k);
	i.normalise();
	const Vec3d j = ::crossProduct(i, k);

	const Vec2d filterpos = diffraction_filter->sampleFilter(samples, wavelengths[PRIMARY_WAVELENGTH_INDEX]);

	assert(filterpos.x >= -0.5 && filterpos.x < 0.5);
	assert(filterpos.y >= -0.5 && filterpos.y < 0.5);

	

	diffraction_filter->evaluate(filterpos, wavelengths, weights_out);

	//const double pdf = weights_out.sum() / (double)TOTAL_NUM_WAVELENGTHS;
	//weights_out /= pdf;//camera.diffraction_filter->filterPDF(filterpos, wavelengths[PRIMARY_WAVELENGTH_INDEX]);
	weights_out *= (double)TOTAL_NUM_WAVELENGTHS / weights_out.sum();

	//const Vec2d offset = filterpos;

	const double z = sqrt(1.0 - filterpos.length2());

	// Generate scattered direction
	const Vec3d out = i * filterpos.x * direction_sign + j * filterpos.y + k * z;
	assert(out.isUnitLength());
	return out;
}


void Camera::applyDiffractionFilterToImage(const Image& cam_diffraction_filter_image, Image& image)
{
	conPrint("Applying diffraction filter...");

	Image out;
	ImageFilter::convolveImage(
		image, // in
		cam_diffraction_filter_image, //filter
		out // result out
		);

	image = out;

	conPrint("\tDone.");
}


void Camera::applyDiffractionFilterToImage(Image& image) const
{
	if(diffraction_filter_image.get() == NULL)
	{
		// Lazily construct diffraction filter + filter image
		buildDiffractionFilter();
		buildDiffractionFilterImage();
	}

	assert(this->diffraction_filter_image.get() != NULL);

	applyDiffractionFilterToImage(
		*this->diffraction_filter_image,
		image
		);
}


/*double Camera::getHorizontalAngleOfView() const // including to left and right, in radians
{
	return 2.0 * atan(sensor_width / (2.0 * sensor_to_lens_dist));
}

double Camera::getVerticalAngleOfView() const // including to up and down, in radians
{
	return 2.0 * atan(sensor_height / (2.0 * sensor_to_lens_dist));
}*/



void Camera::getViewVolumeClippingPlanes(std::vector<Plane<double> >& planes_out) const
{
	planes_out.resize(0);

	planes_out.push_back(
		Plane<double>(
			lens_center + getForwardsDir() * 0.01, // NOTE: bit of a hack here: use a close near clipping plane
			getForwardsDir() * -1.0
			)
		); // back of frustrum


	// Compute some points on the camera sensor
	const Vec3d sensor_bottom = this->getSensorCenter() - getUpDir() * sensor_height * 0.5;
	const Vec3d sensor_top = this->getSensorCenter() + getUpDir() * sensor_height * 0.5;
	const Vec3d sensor_left = this->getSensorCenter() - getRightDir() * sensor_width * 0.5;
	const Vec3d sensor_right = this->getSensorCenter() + getRightDir() * sensor_width * 0.5;

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(getUpDir(), lens_center - sensor_right))
			)
		); // left

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(lens_center - sensor_left, getUpDir()))
			)
		); // right

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(lens_center - sensor_top, getRightDir()))
			)
		); // bottom

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(getRightDir(), lens_center - sensor_bottom))
			)
		); // top
}
	
void Camera::getSubElementSurfaceAreas(const Matrix3<Vec3RealType>& to_parent, std::vector<double>& surface_areas_out) const
{
	assert(0);
}

void Camera::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Vec3d& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	assert(0);
}

double Camera::subElementSamplingPDF(unsigned int sub_elem_index, const Vec3d& pos, double sub_elem_area_ws) const
{
	assert(0);
	return 1.0;
}

void Camera::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const
{
	assert(0);
}

void Camera::getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, 
									  TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const
{
	assert(0);
}


void Camera::unitTest()
{
	conPrint("Camera::unitTest()");

	MTwister rng(1);
	const Vec3d forwards(0,1,0);
	const double lens_sensor_dist = 0.25;
	const double sensor_width = 0.5001;
	const double aspect_ratio = 1.333;
	const double focus_distance = 10.0;
	Camera cam(
		Vec3d(0,0,0), // pos
		Vec3d(0,0,1), // up
		forwards, // forwards
		0.1f, // lens_radius
		focus_distance, // focus distance
		aspect_ratio, // aspect ration
		sensor_width, // sensor_width
		lens_sensor_dist, // lens_sensor_dist
		//"A",
		0.f, 
		1.f, // bloom radius
		false, // autofocus
		false, // polarising_filter
		0.f, //polarising_angle
		0.f, //glare_weight
		1.f, //glare_radius
		5, //glare
		1.f / 200.f, //shutter_open_duration
		//800.f //film speed
		new CircularAperture(Array2d<float>()),
		".", // base indigo path
		0.0, // lens_shift_up_distance
		0.0 // lens_shift_right_distance
		);



	//------------------------------------------------------------------------
	// test sensorPosForImCoords()
	//------------------------------------------------------------------------
	// Image coords in middle of image.
	Vec3d sensorpos = cam.sensorPosForImCoords(Vec2d(0.5, 0.5));
	testAssert(epsEqual(Vec3d(0, -lens_sensor_dist, 0), sensorpos));

	Vec3d v = cam.lensExitDir(sensorpos, cam.getPos());
	testAssert(epsEqual(v, forwards));

	// Image coords at top left of image => bottom right of sensor
	sensorpos = cam.sensorPosForImCoords(Vec2d(0.0, 0.0));
	testAssert(epsEqual(Vec3d(sensor_width/2.0, -lens_sensor_dist, -sensor_width/(aspect_ratio * 2.0)), sensorpos));

	// Test conversion of image coords to sensorpos and back
	Vec2d imcoords = Vec2d(0.1, 0.1);
	sensorpos = cam.sensorPosForImCoords(imcoords);
	testAssert(epsEqual(cam.imCoordsForSensorPos(sensorpos), imcoords));


	//------------------------------------------------------------------------
	// test sensorPosForLensIncidentRay()
	//------------------------------------------------------------------------
	// Test ray in middle of lens, in reverse camera dir.
	bool hitsensor;
	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), forwards * -1.0, hitsensor);
	testAssert(hitsensor);
	testAssert(epsEqual(sensorpos, cam.getSensorCenter()));
	imcoords = cam.imCoordsForSensorPos(sensorpos);
	testAssert(epsEqual(imcoords, Vec2d(0.5, 0.5)));

	//near bottom left of sensor as seen facing in backwards dir
	sensorpos = cam.sensorPosForImCoords(Vec2d(0.1, 0.1));
	//so ray should go a) up and b)left
	Vec3d dir = cam.lensExitDir(sensorpos, cam.getPos());
	testAssert(dot(dir, cam.getUpDir()) > 0.0);//goes up
	testAssert(dot(dir, cam.getRightDir()) < 0.0);//goes left


	// Test ray incoming from right at a 45 degree angle.
	// so 45 degree angle to sensor should be maintained.
	Vec3d indir = normalise(Vec3d(-1, -1, 0));//45 deg angle
	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), indir, hitsensor);
	testAssert(::epsEqual(sensorpos.x, -lens_sensor_dist));
	testAssert(::epsEqual(sensorpos.y, -lens_sensor_dist));
	testAssert(::epsEqual(sensorpos.z, 0.0));

	// Test ray direction back form sensorpos, through the lens center.
	dir = cam.lensExitDir(sensorpos, cam.getPos());
	testAssert(epsEqual(dir, indir * -1.0));


	// 'Sample' an exit direction
	sensorpos = cam.sensorPosForImCoords(Vec2d(0.1f, 0.8f));
	Vec3d lenspos = cam.sampleLensPos(SamplePair(0.45654f, 0.8099f));
	v = cam.lensExitDir(sensorpos, lenspos); // get lens exit direction

	//cast ray back into camera and make sure it hits at same sensor pos
	Vec3d sensorpos2 = cam.sensorPosForLensIncidentRay(lenspos, v * -1.0, hitsensor);
	testAssert(hitsensor);
	testAssert(::epsEqual(sensorpos, sensorpos2));

	// Test ray incoming at an angle that will miss sensor.
	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), normalise(Vec3d(-1.2, -1, 0)), hitsensor);
	assert(!hitsensor);

	//------------------------------------------------------------------------
	// Do some tests with points on the focus plane.
	//------------------------------------------------------------------------
	const Vec3d focus = cam.getPos() + forwards * focus_distance + Vec3d(3.45645, 0, -4.2324);

	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), normalise(cam.getPos() - focus), hitsensor);
	assert(hitsensor);

	lenspos = cam.sampleLensPos(SamplePair(0.76846, 0.38467)); // get a 'random' lens pos
	assert(lenspos != cam.getPos());
	sensorpos2 = cam.sensorPosForLensIncidentRay(lenspos, normalise(lenspos - focus), hitsensor);
	assert(hitsensor);

	assert(epsEqual(sensorpos, sensorpos2));




	//intersect with plane
	//Plane plane(forwards, -10.0f);//10 = focal dist
	//Vec3d hitpos = plane.getRayIntersectPos(lenspos, v);

	//cast back to camera




	//Camera(const Vec3d& pos, const Vec3d& ws_updir, const Vec3d& forwards, 
	//	float lens_radius, float focal_length, float aspect_ratio, float angle_of_view);

	/*const float aov = 90.0f;
	const Vec3d forwards(0,1,0);
	Camera cam(Vec3(0,0,10), Vec3(0,0,1), forwards, 0.0f, 10.0f, 1.333f, aov);

	Vec3 v, target;
	v = cam.getRayUnitDirForNormedImageCoords(0.5f, 0.5f);
	assert(epsEqual(v, Vec3(0.0f, 1.0f, 0.0f)));

	v = cam.getRayUnitDirForNormedImageCoords(0.0f, 0.5f);
	target = normalise(Vec3(-1.0f, 1.0f, 0.0f));
	assert(epsEqual(v, target));
	assert(::epsEqual(::angleBetween(forwards, v), degreeToRad(45.0f)));

	v = cam.getRayUnitDirForNormedImageCoords(1.0f, 0.5f);
	target = normalise(Vec3(1.0f, 1.0f, 0.0f));
	assert(epsEqual(v, target));

	Vec2 im, imtarget;
	bool fell_on_image;
	im = cam.getNormedImagePointForRay(normalise(Vec3(0.f, -1.f, 0.f)), fell_on_image);
	assert(fell_on_image);
	imtarget = Vec2(0.5f, 0.5f);
	assert(epsEqual(im, imtarget));

	im = cam.getNormedImagePointForRay(normalise(Vec3(1.0f, -1.0f, 0.f)), fell_on_image);
	assert(fell_on_image);
	imtarget = Vec2(0.0f, 0.5f);
	assert(epsEqual(im, imtarget));

	im = cam.getNormedImagePointForRay(normalise(Vec3(1.1f, -1.0f, 0.f)), fell_on_image);
	assert(!fell_on_image);

	im = cam.getNormedImagePointForRay(normalise(Vec3(0.0f, 1.0f, 0.f)), fell_on_image);
	assert(!fell_on_image);*/
}





















