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
//#include "../graphics/MitchellNetravali.h"
#include "../indigo/RendererSettings.h"
#include "../indigo/Distribution2.h"
#include "../indigo/Aperture.h"
#include "../indigo/CircularAperture.h"
#include "../utils/fileutils.h"
//#include "../utils/MTwister.h"
#include "../indigo/IndigoImage.h"
#include "../graphics/imformatdecoder.h"
#include "../indigo/SpectralVector.h"
#include "../indigo/TransformPath.h"
#include "../utils/timer.h"
#include "../indigo/PrintOutput.h"
//#include "../graphics/FFTPlan.h"
#include "../maths/GeometrySampling.h"


static const Vec4f FORWARDS_OS(0.0f, 1.0f, 0.0f, 0.0f); // Forwards in local camera (object) space.
static const Vec4f UP_OS(0.0f, 0.0f, 1.0f, 0.0f);
static const Vec4f RIGHT_OS(1.0f, 0.0f, 0.0f, 0.0f);


Camera::Camera(
	const js::Vector<TransformKeyFrame, 16>& frames,
	const Vec3d& ws_updir, const Vec3d& forwards,
	double lens_radius_, double focus_distance_, double sensor_width_, double sensor_height_, double lens_sensor_dist_,
	//const std::string& white_balance,
	double bloom_weight_, double bloom_radius_, bool autofocus_,
	bool polarising_filter_, double polarising_angle_,
	double glare_weight_, double glare_radius_, int glare_num_blades_,
	double exposure_duration_,
	Aperture* aperture_,
	const std::string& appdata_path_,
	double lens_shift_up_distance_,
	double lens_shift_right_distance_,
	bool write_aperture_preview_)
:	autofocus(autofocus_),
	polarising_filter(polarising_filter_),
	polarising_angle(polarising_angle_),
	glare_weight(glare_weight_),
	glare_radius(glare_radius_),
	glare_num_blades(glare_num_blades_),
	aperture(NULL),
	diffraction_filter(NULL),
	appdata_path(appdata_path_),
	write_aperture_preview(write_aperture_preview_)
{
//	plan = new FFTPlan();

	init(Vec3d(0,0,0), ws_updir, forwards,
		lens_radius_, focus_distance_, sensor_width_, sensor_height_, lens_sensor_dist_,
		exposure_duration_,
		aperture_,
		lens_shift_up_distance_, lens_shift_right_distance_);


	// Override default camera position with passed keyframes
	try
	{
		Vec3d up = ws_updir;
		up.removeComponentInDir(forwards);
		up.normalise();
		Vec3d right = ::crossProduct(forwards, up);

		assert(forwards.isUnitLength());
		assert(ws_updir.isUnitLength());
		assert(up.isUnitLength());
		assert(right.isUnitLength());

		Matrix3f child_to_world(toVec3f(right), toVec3f(forwards), toVec3f(up));

		transform_path.init(child_to_world, frames);
	}
	catch(TransformPathExcep& e)
	{
		throw CameraExcep(e.what());
	}

	const Vec4f min_os((float)(lens_center.x - lens_radius), 0.0f, (float)(lens_center.z - lens_radius), 1.0f);
	const Vec4f max_os((float)(lens_center.x + lens_radius), 0.0f, (float)(lens_center.z + lens_radius), 1.0f);

	js::AABBox aabb_os(min_os, max_os);
	bbox_ws = transform_path.worldSpaceAABB(aabb_os, this->getBoundingRadius());

	
	//------------------------------------------------------------------------
	//calculate polarising Vector
	//------------------------------------------------------------------------
	/*this->polarising_vec = getRightDir();

	const Vec2d components = Matrix2d::rotationMatrix(::degreeToRad(this->polarising_angle)) * Vec2d(1.0, 0.0);
	this->polarising_vec = getRightDir() * components.x + getUpDir() * components.y;
	assert(this->polarising_vec.isUnitLength());*/


	// Save aperture preview to disk
	if(write_aperture_preview)
	{
		Array2d<float> ap_vis;
		aperture->getVisibilityMap(ap_vis);

		Image aperture_preview_image(ap_vis.getWidth(), ap_vis.getHeight());

		for(unsigned int y=0; y<ap_vis.getHeight(); ++y)
			for(unsigned int x=0; x<ap_vis.getWidth(); ++x)
				aperture_preview_image.setPixel(x, y, Colour3f((float)ap_vis.elem(x, y)));

		std::map<std::string, std::string> metadata;
		try
		{
			//aperture_preview_image.saveToPng(FileUtils::join(base_indigo_path, "aperture_preview.png"), metadata, 0);
			Bitmap ldr_image((unsigned int)aperture_preview_image.getWidth(), (unsigned int)aperture_preview_image.getHeight(), 3, NULL);
			aperture_preview_image.copyRegionToBitmap(ldr_image, 0, 0, (unsigned int)aperture_preview_image.getWidth(), (unsigned int)aperture_preview_image.getHeight());
			PNGDecoder::write(ldr_image, metadata, FileUtils::join(appdata_path, "aperture_preview.png"));
		}
		catch(ImageExcep& e)
		{
			conPrint("ImageExcep: " + e.what());
		}
	}
}


Camera::~Camera()
{
//	delete plan;
	delete aperture;
}


void Camera::init(
	const Vec3d& cam_pos, const Vec3d& ws_updir, const Vec3d& forwards, 
	double lens_radius_, double focus_distance_, double sensor_width_, double sensor_height_, double lens_sensor_dist_,
	double exposure_duration_,
	Aperture* aperture_,
	double lens_shift_up_distance_,
	double lens_shift_right_distance_)
{
	lens_radius					= lens_radius_;
	focus_distance				= focus_distance_;
	sensor_to_lens_dist			= lens_sensor_dist_;
	exposure_duration			= exposure_duration_;
	lens_shift_up_distance		= lens_shift_up_distance_;
	lens_shift_right_distance	= lens_shift_right_distance_;

	// If we are given a new aperture then delete the old and reassign current
	if(aperture_ != NULL)
	{
		delete aperture;
		aperture = aperture_;
		assert(aperture);
	}

	if(lens_radius <= 0.0)
		throw CameraExcep("lens_radius must be > 0.0");
	if(focus_distance <= 0.0)
		throw CameraExcep("focus_distance must be > 0.0");
	if(sensor_to_lens_dist <= 0.0)
		throw CameraExcep("sensor_to_lens_dist must be > 0.0");
	if(exposure_duration <= 0.0)
		throw CameraExcep("exposure_duration must be > 0.0");

	lens_width = lens_radius * 2.0;
	recip_lens_width = 1.0 / lens_width;

	Vec3d up = ws_updir;
	up.removeComponentInDir(forwards);
	up.normalise();

	Vec3d right = ::crossProduct(forwards, up);
	assert(forwards.isUnitLength());
	assert(ws_updir.isUnitLength());
	assert(up.isUnitLength());
	assert(right.isUnitLength());

	try
	{
		// TEMP HACK kills existing keyframes

		Matrix3f child_to_world(toVec3f(right), toVec3f(forwards), toVec3f(up));
		js::Vector<TransformKeyFrame, 16> frames;
		frames.push_back(TransformKeyFrame(0, Vec4f(cam_pos.x, cam_pos.y, cam_pos.z, 1), Quatf::identity()));

		transform_path.init(child_to_world, frames);
	}
	catch(TransformPathExcep& e)
	{
		throw CameraExcep(e.what());
	}

	sensor_width  = sensor_width_;
	sensor_height = sensor_height_;
	recip_sensor_width  = 1.0 / sensor_width_;
	recip_sensor_height = 1.0 / sensor_height_;

	lens_center = Vec3d(lens_shift_right_distance, 0.0f, lens_shift_up_distance);

	sensor_center  = Vec3d(0.0f, -sensor_to_lens_dist, 0.0f);
	sensor_botleft = Vec3d(sensor_width * -0.5, -sensor_to_lens_dist, sensor_height * -0.5);

	sensor_to_lens_dist_focus_dist_ratio = sensor_to_lens_dist / focus_distance;
	focus_dist_sensor_to_lens_dist_ratio = focus_distance / sensor_to_lens_dist;

	uniform_sensor_pos_pdf = 1.0 / (sensor_width * sensor_height);

	recip_unoccluded_aperture_area = 1.0 / (4.0 * lens_radius * lens_radius);


	const Vec4f min_os((float)(lens_center.x - lens_radius), 0.0f, (float)(lens_center.z - lens_radius), 1.0f);
	const Vec4f max_os((float)(lens_center.x + lens_radius), 0.0f, (float)(lens_center.z + lens_radius), 1.0f);

	js::AABBox aabb_os(min_os, max_os);
	bbox_ws = transform_path.worldSpaceAABB(aabb_os, this->getBoundingRadius());

	makeClippingPlanesCameraSpace();
}


void Camera::prepareForDiffractionFilter(int main_buffer_width_, int main_buffer_height_, int ssf_)
{
	main_buffer_width = main_buffer_width_;
	main_buffer_height = main_buffer_height_;
	ssf = ssf_;
}


void Camera::buildDiffractionFilter()
{
	diffraction_filter = std::auto_ptr<DiffractionFilter>(new DiffractionFilter(
		lens_radius,
		*aperture,
		appdata_path,
		this->write_aperture_preview
	));
}


/*void Camera::buildDiffractionFilterImage(PrintOutput& print_output)
{
	this->diffraction_filter_image = std::auto_ptr<Image>(doBuildDiffractionFilterImage(
		this->diffraction_filter->getDiffractionFilter(),
		*this->diffraction_filter,
		this->main_buffer_width,
		this->main_buffer_height,
		this->sensor_width,
		this->sensor_height,
		this->sensor_to_lens_dist,
		this->write_aperture_preview,
		this->appdata_path,
		this->ssf,
		print_output
	));
}*/

#if 0
Image* Camera::doBuildDiffractionFilterImage(const Array2d<float>& filter_data, const DiffractionFilter& diffraction_filter, int main_buffer_width, int main_buffer_height,
											 double sensor_width, double sensor_height, double sensor_to_lens_dist, bool write_aperture_preview, const std::string& appdata_path, 
											 int ssf, PrintOutput& print_output)
{
	assert(main_buffer_width > 0 && main_buffer_height > 0);

	print_output.print("Creating diffraction filter image...");
	Timer timer;

	MTwister rng(1);

	const int im_w = 512 * ssf + 1;

	Image* diffraction_filter_image = new Image(
		im_w, //TEMP NEW 513, //diffraction_filter->getDiffractionFilter().getWidth(),
		im_w //513 //diffraction_filter->getDiffractionFilter().getHeight()
		);

	const int sensor_x_res = main_buffer_width;
	const int sensor_y_res = main_buffer_height;
	// pixel
	
	MitchellNetravali<double> mn(
		0.6, // B
		0.2 // C
		);

	// max_x_angular_deviation = max pixel deviation divided by physical size of a pixel
	//NOTE: using small angle approximation here (even if not valid) because that's what the diffraction filter code will be doing as well
	const double sensor_pixel_width = sensor_width / (double)sensor_x_res;
	const double sensor_pixel_height = sensor_height / (double)sensor_y_res;
	// m / pixel                     = m                 / pixel

	// For each pixel in the XYZ diffraction map
	for(unsigned int y=0; y<diffraction_filter_image->getHeight(); ++y)
		for(unsigned int x=0; x<diffraction_filter_image->getWidth(); ++x)
		{
			// Get normalised coordinates of this pixel
			const Vec2d normed_coords(
				(double)x / (double)diffraction_filter_image->getWidth(),
				(double)y / (double)diffraction_filter_image->getHeight()
				);
			// dimensionless = pixel / pixel

			// Get distance from center pixel of filter
			const int d_pixel_x = (int)x - (int)diffraction_filter_image->getWidth() / 2;
			const int d_pixel_y = (int)y - (int)diffraction_filter_image->getHeight() / 2;
			// pixel            = pixel  - pixel / 2

			// Get physical distance this corresponds to on sensor
			const double sensor_dist_x = (double)d_pixel_x * sensor_pixel_width;
			const double sensor_dist_y = (double)d_pixel_y * sensor_pixel_height;
			// m                       = pixel             * (m/pixel)

			// What angular deviation does this correspond to?
			const double x_angle = sensor_dist_x / sensor_to_lens_dist;
			const double y_angle = sensor_dist_y / sensor_to_lens_dist;
			// radians           = m             / m


			// For each of a few wavelengths chosen over the visible range...
			Vec3d sum(0.0, 0.0, 0.0);
			const int NUM_WAVELENGTH_SAMPLES = 20;
			for(int i=0; i<NUM_WAVELENGTH_SAMPLES; ++i)
			{
				// Sample a wavelength
				const double wvlen_nm = MIN_WAVELENGTH + (((double)i + rng.unitRandom()) / (double)NUM_WAVELENGTH_SAMPLES) * WAVELENGTH_SPAN;
				assert(Maths::inRange<double>(wvlen_nm, MIN_WAVELENGTH, MAX_WAVELENGTH));
				const double wvlen_m = wvlen_nm * 1.0e-9;

				// Get the number of times bigger the dest features are than the source features (in pixels),

				const double max_src_angular_deviations = diffraction_filter.getXYScale(wvlen_m);
				// radians

				const Vec2d src_normed_coords = Vec2d(0.5, 0.5) + Vec2d(x_angle, y_angle) / max_src_angular_deviations;
				// dimensionless              =                   radians                 / radians

				// Src pixel width (if diffraction pattern was projected onto sensor)
				const double src_pixel_width = (max_src_angular_deviations * sensor_to_lens_dist) / ((double)filter_data.getWidth() * 0.5);
				// m/pixel                   = radians                 * m                          / pixels

				const double src_pixels_per_dest_pixel_x = sensor_pixel_width / src_pixel_width;
				const double src_pixels_per_dest_pixel_y = sensor_pixel_height / src_pixel_width;
				// dimensionless                         = m/pixel             / m/pixel

				// Convert to pixel coords.
				// So src_pixel_coords is the point on the source image corresponding to (x, y) on the dest image
				const Vec2d src_pixel_coords(
					src_normed_coords.x * (double)filter_data.getWidth(),
					src_normed_coords.y * (double)filter_data.getHeight());

				// Find the center pixel of the sampling filter in the source image.
				const int center_sx = Maths::floorToInt(src_pixel_coords.x);
				const int center_sy = Maths::floorToInt(src_pixel_coords.y);

				const Vec3d xyz_col = toVec3d(SingleFreq::getXYZ_CIE_2DegForWavelen((float)wvlen_nm));

				/*if(false) // TEMP NEW dest_pixels_per_src_pixel_x > 1.0)
				{
					// Just point sample
					if(center_sx > 0 && center_sx < (int)filter_data.getWidth() &&
						center_sy > 0 && center_sy < (int)filter_data.getHeight())
					{
						const double diff_value = filter_data.elem(center_sx, center_sy);
						sum += xyz_col * diff_value;
					}
				}
				else
				{*/
					const double mn_scale_x = myMax(src_pixels_per_dest_pixel_x, 1.0);
					const double mn_scale_y = myMax(src_pixels_per_dest_pixel_y, 1.0);
					const double recip_mn_scale_x = 1.0 / mn_scale_x;
					const double recip_mn_scale_y = 1.0 / mn_scale_y;

					// Using MN filter radius, in units of source image pixels.
					const double MN_radius = 2.0;
					const double filter_rad_xf = mn_scale_x * MN_radius; //myMax(2.0 * src_pixels_per_dest_pixel_x, 2.0);
					const double filter_rad_yf = mn_scale_y * MN_radius; //myMax(2.0 * src_pixels_per_dest_pixel_y, 2.0);
					const int filter_rad_x = (int)ceil(filter_rad_xf);
					const int filter_rad_y = (int)ceil(filter_rad_yf);

					// Get the min and max bounds of the filter support on the source image.
					const int min_sx = myMax(0, center_sx - filter_rad_x);
					const int min_sy = myMax(0, center_sy - filter_rad_y);
					const int max_sx = myMin((int)filter_data.getWidth(), center_sx + filter_rad_x + 1);
					const int max_sy = myMin((int)filter_data.getHeight(), center_sy + filter_rad_y + 1);

					// For each pixel in source image in filter support...
					for(int sy=min_sy; sy<max_sy; ++sy)
					{
						const double dy = fabs(((double)sy - src_pixel_coords.y) * recip_mn_scale_y); // / rad_y);
						assert(dy <= 3);
						const double filter_y = mn.eval(dy);

						for(int sx=min_sx; sx<max_sx; ++sx)
						{
							const double dx = fabs(((double)sx - src_pixel_coords.x) * recip_mn_scale_x); // / rad_x);
							assert(dx <= 3);
							const double filter_x = mn.eval(dx);

							const double filter_val = filter_x * filter_y;

							const double diff_value = filter_data.elem(sx, sy);

							sum += xyz_col * diff_value * filter_val;
						}
					}
				//}
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

	print_output.print("\tDone.  (Elapsed: " + toString(timer.elapsed()) + " s)");

	// save diffraction image-------------------------------------------------------
	if(write_aperture_preview)
	{
		//TEMP:
		/*for(int y=diffraction_filter_image->getWidth()/2 - 4; y<diffraction_filter_image->getWidth()/2 + 4; ++y)
		{
			conPrint("");
			for(int x=diffraction_filter_image->getWidth()/2 - 4; x<diffraction_filter_image->getWidth()/2 + 4; ++x)
			{
				conPrintStr(::doubleToStringScientific(diffraction_filter_image->getPixel(x, y).g) + "\t");
			}
		}*/

		try
		{
			for(int i=1; i<100000; i*=10)
			{
				Image save_image = *diffraction_filter_image;

				save_image.scale(1000 * i / save_image.maxLuminance());
				save_image.posClamp();

				Bitmap ldr_image(save_image.getWidth(), save_image.getHeight(), 3, NULL);
				save_image.copyRegionToBitmap(ldr_image, 0, 0, save_image.getWidth(), save_image.getHeight());
				std::map<std::string, std::string> dummy_metadata;
				PNGDecoder::write(ldr_image, dummy_metadata, FileUtils::join(appdata_path, "XYZ_diffraction_preview_" + toString(i) + ".png"));
			}

		}
		catch(ImageExcep& e)
		{
			print_output.print("ImageExcep: " + e.what());
		}
		catch(ImFormatExcep& e)
		{
			print_output.print("ImageExcep: " + e.what());
		}
	}
	//---------------------------------------------------------------------------------------

	return diffraction_filter_image;
}
#endif


/*      k
        ^          j
        |        ^
        |       /
        |      /
        |     /
________|____/__
|       |   /   |
|       |  /    | lens_width
|       | /     |
|       |/      |
|       -----------------> i
|               |
|               |
|_______________|
      lens_width
*/


/*const Vec3d Camera::sampleSensor(const SamplePair& samples, double time) const
{
	return transform_path.pointToWorld(
			Vec3d(
				sensor_width * (samples.x - 0.5f),
				-sensor_to_lens_dist,
				sensor_height * (samples.y - 0.5f)
			),
			time
		);

	//sensor_botleft +
	//right * samples.x * sensor_width +
	//up * samples.y * sensor_height;
}*/


Camera::PDType Camera::sensorPDF() const
{
	assert(epsEqual(uniform_sensor_pos_pdf, 1.0 / (sensor_width * sensor_height)));
	return uniform_sensor_pos_pdf;
}


void Camera::sampleLensPos(const SamplePair& samples, double time, Vec3Type& pos_os_out, Vec3Type& pos_ws_out) const
{
	assert(aperture->sampleAperture(samples).inHalfClosedInterval(0.0, 1.0));

	const Vec2f normed_lenspos = aperture->sampleAperture(samples);

	pos_os_out.set(
		lens_center.x + (normed_lenspos.x - 0.5f) * lens_width,
		lens_center.y,
		lens_center.z + (normed_lenspos.y - 0.5f) * lens_width,
		1.0f
	);

	pos_ws_out = transform_path.vecToWorld(pos_os_out, time);


	//return lens_center +
	//		up * (2.0 * normed_lenspos.y - 1.0) * lens_radius +
	//		right * (2.0 * normed_lenspos.x - 1.0) * lens_radius;
}


Camera::PDType Camera::lensPosPDF(const Vec3Type& lenspos_os/*const Vec3d& lenspos, double time*/) const
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


	//const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos, time);
	//assert(normed_lenspoint.inHalfClosedInterval(0.0, 1.0));

	//assert(aperture->pdf(normed_lenspoint) > 0.0);
	//assert(isFinite(aperture->pdf(normed_lenspoint)));

	const Vec2f normed_lenspoint(
		((lenspos_os[0] - lens_center.x) * recip_lens_width) + 0.5f,
		((lenspos_os[2] - lens_center.z) * recip_lens_width) + 0.5f
		);

	if(normed_lenspoint.x < 0.0f || normed_lenspoint.x >= 1.0f || normed_lenspoint.y < 0.0f || normed_lenspoint.y >= 1.0f)
		return 0.0;

	//return aperture->pdf(normed_lenspoint) / (4.0 * lens_radius * lens_radius); // TEMP TODO: precompute divide
	assert(epsEqual(recip_unoccluded_aperture_area, 1.0 / (4.0 * lens_radius * lens_radius)));
	return aperture->pdf(normed_lenspoint) * recip_unoccluded_aperture_area;
}


/*
	p_sa = p_A * ||lens - sensor||^2 / cos(theta)
	= p_A * ||lens - sensor||^2 / (forwards, (lens - sensor) / ||lens - sensor||)
	= p_A * ||lens - sensor||^2 / [ (forwards, (lens - sensor)) / ||lens - sensor|| ]
	= p_A * ||lens - sensor||^3 / (forwards, (lens - sensor))
*/
Camera::PDType Camera::lensPosSolidAnglePDF(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const
{
	const SSE_ALIGN Vec3Type sensor_to_lens = lenspos_os - sensorpos_os;

	assert(sensor_to_lens[1] > 0.0);

	const Real d2 = sensor_to_lens.length2();

	return lensPosPDF(lenspos_os) * d2 * std::sqrt(d2) / sensor_to_lens[1];

	/*const double pdf_A = lensPosPDF(lenspos, time);

	//NOTE: this is a very slow way of doing this!!!!
	const double costheta = fabs(dot(getForwardsDir(time), normalise(lenspos - sensorpos)));

	return MatUtils::areaToSolidAnglePDF(pdf_A, lenspos.getDist2(sensorpos), costheta);*/
}


const Camera::Vec3Type Camera::lensExitDir(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const
{
	// The target point can be found by following the line from sensorpos, through the lens center,
	// and for a distance of focus_distance.

	const double sensor_up = sensorpos_os[2] - sensor_center[2]; // Distance up from sensor center
	const double sensor_right = sensorpos_os[0] - sensor_center[0]; // Distance right from sensor center

	// TEMP HACK NEW: spherical abberation
	/*const float lens_r2 = lenspos_os.x[0]*lenspos_os.x[0] + lenspos_os.x[2]*lenspos_os.x[2];
	const float lens_r4 = lens_r2 * lens_r2;
	const float ab_scale = 1.0e7f;//10.0e-6f;
	//const float use_focus_dist_sensor_to_lens_dist_ratio = focus_dist_sensor_to_lens_dist_ratio * (1.0 - lens_r4*ab_scale);

	// TEMP NEW: Fish-eye projection
	const float r2 = sensorpos_os.x[0]*sensorpos_os.x[0] + sensorpos_os.x[2]*sensorpos_os.x[2];
	const float r = sqrt(r2);

	//const float theta = asin(2.5 * r / this->sensor_to_lens_dist);
	//const float theta = 4 * r / this->sensor_to_lens_dist;
	const float theta = atan(r / this->sensor_to_lens_dist);
	
	const float phi = atan2(-sensor_up, -sensor_right);
	
	const Vec4f dir(
		cos(phi) * sin(theta),
		cos(theta),
		sin(phi) * sin(theta),
		0
	);

	conPrint(::doubleToStringScientific(lens_r4));
	const Vec4f target_point_os = Vec4f(0,0,0,1) + dir * this->focus_distance * (1.0 - lens_r4*ab_scale);
		*/

	const double target_up_dist = (lens_shift_up_distance - sensor_up) * focus_dist_sensor_to_lens_dist_ratio;
	const double target_right_dist = (lens_shift_right_distance - sensor_right) * focus_dist_sensor_to_lens_dist_ratio;

	const Vec3Type target_point_os(
		lens_center.x + target_right_dist,
		focus_distance, // TEMP
		lens_center.z + target_up_dist,
		1.0f
	);

	// Resulting ray is a ray from the lens position, to the target position.
	return transform_path.vecToWorld(normalise(target_point_os - lenspos_os), time);

	



	/*const double sensor_up = distUpOnSensorFromCenter(sensorpos);
	const double sensor_right = distRightOnSensorFromCenter(sensorpos);

	const double target_up_dist = (lens_shift_up_distance - sensor_up) * focus_dist_sensor_to_lens_dist_ratio;
	const double target_right_dist = (lens_shift_right_distance - sensor_right) * focus_dist_sensor_to_lens_dist_ratio;

	const Vec3d target_point = lens_center + forwards * focus_distance +
		up * target_up_dist +
		right * target_right_dist;

	return normalise(target_point - lenspos);*/
}


double Camera::lensPosVisibility(const Vec3Type& lenspos_os) const
{
	//const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos, time);

	// Where x=0 is left, x=1 is on right of lens, y=0 is bottom, y=1 is top of lens.
	// TODO: precompute 1/lens_width
	const Vec2f normed_lenspoint(
		((lenspos_os[0] - lens_center.x) * recip_lens_width) + 0.5f,
		((lenspos_os[2] - lens_center.z) * recip_lens_width) + 0.5f
		);

	if(normed_lenspoint.x < 0.0 || normed_lenspoint.x >= 1.0 || normed_lenspoint.y < 0.0 || normed_lenspoint.y >= 1.0)
		return 0.0;

	return aperture->visibility(normed_lenspoint);
}


void Camera::sensorPosForLensIncidentRay(const Vec3Type& lenspos_ws, const Vec3Type& raydir_, double time, bool& hitsensor_out, Vec3Type& sensorpos_os_out, Vec3Type& sensorpos_ws_out) const
{
	assert(raydir_.isUnitLength());

	const SSE_ALIGN Vec3Type lenspos_os = transform_path.vecToLocal(lenspos_ws, time);
	const SSE_ALIGN Vec3Type raydir_os = transform_path.vecToLocal(raydir_, time);

	// Follow ray back to focus_distance away to the target point
	//const double lens_up = lenspos_os.z - lens_center.z; // distUpOnLensFromCenter(lenspos);
	//const double lens_right = lenspos_os.x - lens_center.x; // distRightOnLensFromCenter(lenspos);
	//const double forwards_comp = -raydir_os.y; //-dot(raydir, forwards);

	if(raydir_os[1] >= 0.0) // If ray is not heading into camera
	{
		hitsensor_out = false;
		return;
	}
	const double ray_dist_to_target_plane = focus_distance / (-raydir_os[1]);

	// Compute the up and right components of target point, relative to the lens center point.
	const double target_up = lenspos_os[2] - raydir_os[2] * ray_dist_to_target_plane - lens_center.z;
	const double target_right = lenspos_os[0] - raydir_os[0] * ray_dist_to_target_plane - lens_center.x;

	// Compute corresponding sensorpos
	const double sensor_up = -target_up * sensor_to_lens_dist_focus_dist_ratio + lens_shift_up_distance;
	const double sensor_right = -target_right * sensor_to_lens_dist_focus_dist_ratio + lens_shift_right_distance;

	/*if(fabs(sensor_up) > sensor_height * 0.5 || fabs(sensor_right) > sensor_width * 0.5)
	{
		hitsensor_out = false;
		return Vec3d(0,0,0);
	}*/

	sensorpos_os_out.set(
		sensor_center.x + sensor_right,
		sensor_center.y,
		sensor_center.z + sensor_up,
		1.0f);

	if(sensorpos_os_out[0] < sensor_center.x - sensor_width * 0.5 || sensorpos_os_out[0] >= sensor_center.x + sensor_width * 0.5 ||
		sensorpos_os_out[2] < sensor_center.z - sensor_height * 0.5 || sensorpos_os_out[2] >= sensor_center.z + sensor_height * 0.5)
	{
		hitsensor_out = false;
		return;
	}

	hitsensor_out = true;
	sensorpos_ws_out = transform_path.vecToWorld(sensorpos_os_out, time);

	//return sensor_center + up * sensor_up + right * sensor_right;
}


const Vec2d Camera::imCoordsForSensorPos(const Vec3Type& sensorpos_os, double time) const
{
	const double sensor_up_dist = sensorpos_os[2] - sensor_botleft.z; // dot(sensorpos - sensor_botleft, up);
	const double sensor_right_dist = sensorpos_os[0] - sensor_botleft.x; // dot(sensorpos - sensor_botleft, right);

	return Vec2d(1.0 - (sensor_right_dist * recip_sensor_width), sensor_up_dist * recip_sensor_height);
}


void Camera::sensorPosForImCoords(const Vec2d& imcoords, Vec3Type& pos_os_out) const
{
	pos_os_out.set(
		sensor_width * (0.5f - imcoords.x),
		-sensor_to_lens_dist,
		sensor_height * (imcoords.y - 0.5f),
		1.0f
	);
}


Geometry::DistType Camera::traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const
{
	return -1.0f; // TEMP
}


const js::AABBox& Camera::getAABBoxWS() const
{
	return bbox_ws;
}


void Camera::getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	return;
}


bool Camera::doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const
{
	return false;
}


const std::string Camera::getName() const { return "Camera"; }


const Camera::Vec3Type Camera::getShadingNormal(const HitInfo& hitinfo) const { return FORWARDS_OS; }


const Camera::Vec3Type Camera::getGeometricNormal(const HitInfo& hitinfo) const { return FORWARDS_OS; }


void Camera::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out) const
{
	N_g_os_out = FORWARDS_OS;
	N_s_os_out = FORWARDS_OS;
	mat_index_out = 0;
}


unsigned int Camera::getNumTexCoordSets() const { return 0; }


unsigned int Camera::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


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


void Camera::subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, /*const CoordFramed& camera_coordframe_os, */ double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, PrintOutput& print_output, bool verbose){}


void Camera::build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose) {} // throws GeometryExcep


const Vec3d Camera::diffractRay(const SamplePair& samples, const Vec3d& dir, const SpectralVector& wavelengths, double direction_sign, double time, SpectralVector& weights_out) const
{
	assert(diffraction_filter.get() != NULL);

	if(diffraction_filter.get() == NULL)
	{
		// conPrint("Camera::diffractRay(): ERROR: diffraction_filter is NULL");
		weights_out.set(1);
		return dir;
	}

	// Form a basis with k in direction of ray, using cam right as i
	const Vec3d k = dir;
	Vec3d i = getRightDir(time);
	i.removeComponentInDir(k);
	i.normalise();
	const Vec3d j = ::crossProduct(i, k);

	const Vec2d filterpos = diffraction_filter->sampleFilter(samples, wavelengths[PRIMARY_WAVELENGTH_INDEX]);

	assert(filterpos.x >= -0.5 && filterpos.x < 0.5);
	assert(filterpos.y >= -0.5 && filterpos.y < 0.5);



	diffraction_filter->evaluate(filterpos, wavelengths, weights_out);

	//const double pdf = weights_out.sum() / (double)TOTAL_NUM_WAVELENGTHS;
	//weights_out /= pdf;//camera.diffraction_filter->filterPDF(filterpos, wavelengths[PRIMARY_WAVELENGTH_INDEX]);
	weights_out *= (SpectralVector::Real)TOTAL_NUM_WAVELENGTHS / weights_out.sum();

	//const Vec2d offset = filterpos;

	const double z = sqrt(1.0 - filterpos.length2());

	// Generate scattered direction
	const Vec3d out = i * filterpos.x * direction_sign + j * filterpos.y + k * z;
	assert(out.isUnitLength());
	return out;
}


// Static + private
/*void Camera::applyDiffractionFilterToImage(const Image& cam_diffraction_filter_image, const Image& in, Image& out, FFTPlan& plan)
{
	try
	{
		ImageFilter::convolveImage(
			in, // in
			cam_diffraction_filter_image, //filter
			out, // result out
			plan
		);
	}
	catch(Indigo::Exception& e)
	{
		throw CameraExcep(e.what());
	}
}


void Camera::applyDiffractionFilterToImage(PrintOutput& print_output, const Image& in, Image& out)
{
	if(diffraction_filter_image.get() == NULL)
	{
		// Lazily construct diffraction filter + filter image
		buildDiffractionFilter();
		buildDiffractionFilterImage(print_output);
	}

	assert(this->diffraction_filter_image.get() != NULL);

	try
	{
		applyDiffractionFilterToImage(
			*this->diffraction_filter_image,
			in,
			out,
			*plan
			);
	}
	catch(CameraExcep& e)
	{
		print_output.print("Error while performing aperture diffraction: " + e.what());

		out = in;
	}
}*/


/*double Camera::getHorizontalAngleOfView() const // including to left and right, in radians
{
	return 2.0 * atan(sensor_width / (2.0 * sensor_to_lens_dist));
}


double Camera::getVerticalAngleOfView() const // including to up and down, in radians
{
	return 2.0 * atan(sensor_height / (2.0 * sensor_to_lens_dist));
}*/


const Vec3d Camera::getPosWS(double time) const
{
	const SSE_ALIGN Vec4f origin(0,0,0,1.0f);
	return toVec3d(transform_path.vecToWorld(origin, time));
}


void Camera::makeClippingPlanesCameraSpace()
{
	clipping_planes_camera_space.resize(0);

	const Vec3d up(UP_OS);
	const Vec3d right(RIGHT_OS);
	const Vec3d forwards(FORWARDS_OS);

	const Vec3d sensor_bottom = (sensor_center - up * sensor_height * 0.5);
	const Vec3d sensor_top = (sensor_center + up * sensor_height * 0.5);
	const Vec3d sensor_left = (sensor_center - right * sensor_width * 0.5);
	const Vec3d sensor_right = (sensor_center + right * sensor_width * 0.5);

	clipping_planes_camera_space.push_back(
		Plane<Vec3RealType>(
			toVec3f(lens_center + forwards * 0.01f), // NOTE: bit of a hack here: use a close near clipping plane
			toVec3f(forwards * -1.0f)
			)
		); // back of frustrum

	clipping_planes_camera_space.push_back(
		Plane<Vec3RealType>(
			toVec3f(lens_center),
			toVec3f(normalise(crossProduct(up, lens_center - sensor_right)))
			)
		); // left

	clipping_planes_camera_space.push_back(
		Plane<Vec3RealType>(
			toVec3f(lens_center),
			toVec3f(normalise(crossProduct(lens_center - sensor_left, up)))
			)
		); // right

	clipping_planes_camera_space.push_back(
		Plane<Vec3RealType>(
			toVec3f(lens_center),
			toVec3f(normalise(crossProduct(lens_center - sensor_top, right)))
			)
		); // bottom

	clipping_planes_camera_space.push_back(
		Plane<Vec3RealType>(
			toVec3f(lens_center),
			toVec3f(normalise(crossProduct(right, lens_center - sensor_bottom)))
			)
		); // top

	/*const double time = 0.0; // TEMP HACK not considering moving camera.

	planes_out.push_back(
		Plane<double>(
			lens_center + getForwardsDir(time) * 0.01, // NOTE: bit of a hack here: use a close near clipping plane
			getForwardsDir(time) * -1.0
			)
		); // back of frustrum


	// Compute some points on the camera sensor

	SSE_ALIGN Vec4f sensor_center_f;
	sensor_center.pointToVec4f(sensor_center_f);
	const Vec3d sensor_center_ws = transform_path.vecToWorld(sensor_center_f, time);


	const Vec3d sensor_bottom = sensor_center_ws - getUpDir(time) * sensor_height * 0.5;
	const Vec3d sensor_top = sensor_center_ws + getUpDir(time) * sensor_height * 0.5;
	const Vec3d sensor_left = sensor_center_ws - getRightDir(time) * sensor_width * 0.5;
	const Vec3d sensor_right = sensor_center_ws + getRightDir(time) * sensor_width * 0.5;

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(getUpDir(time), lens_center - sensor_right))
			)
		); // left

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(lens_center - sensor_left, getUpDir(time)))
			)
		); // right

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(lens_center - sensor_top, getRightDir(time)))
			)
		); // bottom

	planes_out.push_back(
		Plane<double>(
			lens_center,
			normalise(crossProduct(getRightDir(time), lens_center - sensor_bottom))
			)
		); // top*/
}


void Camera::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const
{
	assert(0);
}


void Camera::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	assert(0);
}


double Camera::subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const
{
	assert(0);
	return 1.0;
}


void Camera::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const
{
	assert(0);
}


void Camera::getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set,
									  TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const
{
	assert(0);
}


bool Camera::isEnvSphereGeometry() const
{
	return false;
}

bool Camera::areSubElementsCurved() const
{
	return false;
}


Camera::Vec3RealType Camera::getBoundingRadius() const
{
	const Vec3f min_os((float)(lens_center.x - lens_radius), 0.0f, (float)(lens_center.z - lens_radius));
	const Vec3f max_os((float)(lens_center.x + lens_radius), 0.0f, (float)(lens_center.z + lens_radius));

	Vec3RealType max_r2 = 0.0f;
	max_r2 = myMax(max_r2, Vec3f(min_os.x, min_os.y, min_os.z).length2());
	max_r2 = myMax(max_r2, Vec3f(min_os.x, min_os.y, max_os.z).length2());//1
	max_r2 = myMax(max_r2, Vec3f(min_os.x, max_os.y, min_os.z).length2());//2
	max_r2 = myMax(max_r2, Vec3f(min_os.x, max_os.y, max_os.z).length2());//3
	max_r2 = myMax(max_r2, Vec3f(max_os.x, min_os.y, min_os.z).length2());//4
	max_r2 = myMax(max_r2, Vec3f(max_os.x, min_os.y, max_os.z).length2());//5
	max_r2 = myMax(max_r2, Vec3f(max_os.x, max_os.y, min_os.z).length2());//6
	max_r2 = myMax(max_r2, Vec3f(max_os.x, max_os.y, max_os.z).length2());//7
	return std::sqrt(max_r2);
}


const Camera::Vec3Type Camera::positionForHitInfo(const HitInfo& hitinfo) const
{
	::fatalError("Camera::positionForHitInfo");
	return Vec3Type(0,0,0,1);
}


void Camera::setPosForwardsWS(Vec3d cam_pos, Vec3d cam_dir)
{
	Vec3d up(0, 0, 1); //UP_OS.x[0], UP_OS.x[1], UP_OS.x[2]);
	up.removeComponentInDir(cam_dir);
	up.normalise();

	Vec3d right = ::crossProduct(cam_dir, up);
	assert(cam_dir.isUnitLength());
	assert(up.isUnitLength());
	assert(right.isUnitLength());

	try
	{
		Matrix3f child_to_world(toVec3f(right), toVec3f(cam_dir), toVec3f(up));
		js::Vector<TransformKeyFrame, 16> frames;
		frames.push_back(TransformKeyFrame(0, Vec4f(cam_pos.x, cam_pos.y, cam_pos.z, 1), Quatf::identity()));

		transform_path.init(child_to_world, frames);
	}
	catch(TransformPathExcep& e)
	{
		throw CameraExcep(e.what());
	}

	const SSE_ALIGN Vec4f min_os((float)(lens_center.x - lens_radius), 0.0f, (float)(lens_center.z - lens_radius), 1.0f);
	const SSE_ALIGN Vec4f max_os((float)(lens_center.x + lens_radius), 0.0f, (float)(lens_center.z + lens_radius), 1.0f);

	SSE_ALIGN js::AABBox aabb_os(min_os, max_os);
	bbox_ws = transform_path.worldSpaceAABB(aabb_os, this->getBoundingRadius());

	makeClippingPlanesCameraSpace();
}


const Vec3d Camera::getUpDir(double time) const
{
	return toVec3d(transform_path.vecToWorld(UP_OS, time));
}


const Vec3d Camera::getRightDir(double time) const
{
	return toVec3d(transform_path.vecToWorld(RIGHT_OS, time));
}


const Vec3d Camera::getForwardsDir(double time) const
{
	return toVec3d(transform_path.vecToWorld(FORWARDS_OS, time));
}


const Vec4f Camera::getForwardsDirF(double time) const
{
	return transform_path.vecToWorld(FORWARDS_OS, time);
}


#if (BUILD_TESTS)
void Camera::unitTest()
{
	conPrint("Camera::unitTest()");

//	MTwister rng(1);
	const Vec3d forwards(0,1,0);
	const double lens_sensor_dist = 1.0;
	const double sensor_width = 1.0;
	const double aspect_ratio = 1.0;
	const double sensor_height = sensor_width / aspect_ratio;
	const double focus_distance = 10.0;
	Camera cam(
		//Vec3d(0,0,0), // pos
		js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, Vec4f(0,0,0,1), Quatf::identity())),
		//Matrix3f::identity(),
		Vec3d(0,0,1), // up
		forwards, // forwards
		0.25f, // lens_radius
		focus_distance, // focus distance
		sensor_width, // sensor_width
		sensor_height, // sensor_height
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
		0.25, // lens_shift_up_distance
		0.0, // lens_shift_right_distance
		false // write_aperture_preview
		);

	const SSE_ALIGN Vec4f sensor_center(0, -lens_sensor_dist, 0, 1.0f);


	//------------------------------------------------------------------------
	// test sensorPosForImCoords()
	//------------------------------------------------------------------------
	const double time = 0.0;
	// Image coords in middle of image.
	{
	SSE_ALIGN Vec4f sensorpos_os;
	cam.sensorPosForImCoords(Vec2d(0.5, 0.5), sensorpos_os);
	const Vec4f sensorpos_ws = cam.transform_path.vecToWorld(sensorpos_os, time);
	testAssert(epsEqual(sensor_center, sensorpos_os));
	testAssert(epsEqual(sensor_center, sensorpos_ws));
	}

	Vec3d lenspos_os(0,0,0);

	//Vec3f v = cam.lensExitDir(sensorpos_os, lenspos_os, time);
	//testAssert(epsEqual(v, toVec3f(forwards)));

	// Image coords at top left of image => bottom right of sensor
	{
	SSE_ALIGN Vec4f sensorpos_os;
	cam.sensorPosForImCoords(Vec2d(0.0, 0.0), sensorpos_os);
	const Vec4f sensorpos_ws = cam.transform_path.vecToWorld(sensorpos_os, time);
	testAssert(epsEqual(Vec3d(sensor_width/2.0, -lens_sensor_dist, -sensor_width/(aspect_ratio * 2.0)), toVec3d(sensorpos_os)));
	}

	// Test conversion of image coords to sensorpos and back
	{
	const Vec2d imcoords(0.1856, 0.145);
	SSE_ALIGN Vec4f sensorpos_os;
	cam.sensorPosForImCoords(imcoords, sensorpos_os);
	const Vec4f sensorpos_ws = cam.transform_path.vecToWorld(sensorpos_os, time);
	testAssert(epsEqual(cam.imCoordsForSensorPos(sensorpos_os, time), imcoords));
	}


	//------------------------------------------------------------------------
	// test sensorPosForLensIncidentRay()
	//------------------------------------------------------------------------
	// Test ray in middle of lens, in reverse camera dir.
	/*bool hitsensor;
	Vec3d lenspos_ws(0,0,0);
	cam.sensorPosForLensIncidentRay(lenspos_ws, toVec3f(forwards) * -1.0, time, hitsensor, sensorpos_os, sensorpos_ws);
	testAssert(hitsensor);
	testAssert(epsEqual(sensorpos_os, sensor_center));
	imcoords = cam.imCoordsForSensorPos(sensorpos_os, time);
	testAssert(epsEqual(imcoords, Vec2d(0.5, 0.5)));*/

	/*
	//near bottom left of sensor as seen facing in backwards dir
	cam.sensorPosForImCoords(Vec2d(0.1, 0.1), time, sensorpos_os, sensorpos_ws);
	//so ray should go a) up and b)left
	Vec3d dir = toVec3d(cam.lensExitDir(sensorpos_os, Vec3d(0,0,0), time));
	testAssert(dot(dir, cam.getUpDir(time)) > 0.0);//goes up
	testAssert(dot(dir, cam.getRightDir(time)) < 0.0);//goes left
*/
/*
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

*/
	{
	// Sample an exit direction
	Vec4f lenspos_os, lenspos_ws;
	Vec4f sensorpos_os;
	cam.sensorPosForImCoords(Vec2d(0.2f, 0.5f), sensorpos_os); // Get sensor position
	const Vec4f sensorpos_ws = cam.transform_path.vecToWorld(sensorpos_os, time);
	cam.sampleLensPos(SamplePair(0.1f, 0.4f), time, lenspos_os, lenspos_ws); // Get lens position
	const Vec4f lens_exit_dir = cam.lensExitDir(sensorpos_os, lenspos_os, time); // get lens exit direction

	// Cast ray back into camera and make sure it hits at same sensor pos
	bool hitsensor;
	Vec4f sensorpos_os2, sensorpos_ws2;
	cam.sensorPosForLensIncidentRay(lenspos_ws, lens_exit_dir * -1.0, time, hitsensor, sensorpos_os2, sensorpos_ws2);
	testAssert(hitsensor);
	testAssert(::epsEqual(sensorpos_os, sensorpos_os2));
	testAssert(::epsEqual(sensorpos_ws, sensorpos_ws2));
	}

/*
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


*/

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



	// Test aperture diffraction filter building.
	/*try
	{
		Array2d<float> vis_map(Aperture::getVisibilityMapWidth(), Aperture::getVisibilityMapWidth());
		vis_map.setAllElems(1.0f);

		CircularAperture ap(vis_map);

		DiffractionFilter filter(
			0.0001, // aperture radius
			ap,
			".", // preview save dir
			true // write preview
		);

		Array2d<double> filter_data(filter.getDiffractionFilter().getWidth(), filter.getDiffractionFilter().getHeight());
		
		
		//filter_data.setAllElems(0.0);
		//Bitmap bmp;
		//PNGDecoder::decode("../testscenes/diag_obstacle.png", bmp);

		//for(int y=0; y<myMin(filter_data.getHeight(), bmp.getHeight()); ++y)
		//	for(int x=0; x<myMin(filter_data.getWidth(), bmp.getWidth()); ++x)
		//	{
		//		filter_data.elem(x, y) = bmp.getPixel(x, y)[1] * (1.0 / 255.0);
		//	}

		filter_data = filter.getDiffractionFilter();

		Image* image = Camera::doBuildDiffractionFilterImage(
			filter_data,
			filter,
			1000, // main buffer width
			1000, // main buffer height
			0.036, // sensor width
			0.036, // sensor height
			0.05, // lens-sensor dist
			true, // write ap preview
			"." // appdata path
		);

		delete image;
	}
	catch(ImFormatExcep& e)
	{
		conPrint(e.what());
		testAssert(false);
	}

	exit(0);*/
}
#endif
