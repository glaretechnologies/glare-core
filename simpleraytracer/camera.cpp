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
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../indigo/DiffractionFilter.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"



Camera::Camera(const Vec3d& pos_, const Vec3d& ws_updir, const Vec3d& forwards_, 
		double lens_radius_, double focus_distance_, double aspect_ratio_, double sensor_width_, double lens_sensor_dist_, 
		const std::string& white_balance, double bloom_weight_, double bloom_radius_, bool autofocus_, 
		bool polarising_filter_, double polarising_angle_,
		double glare_weight_, double glare_radius_, int glare_num_blades_,
		double exposure_duration_/*, float film_sensitivity_*/)
:	pos(pos_),
	ws_up(ws_updir),
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
	colour_space_converter(NULL),
	diffraction_filter(NULL),
	aperture_image(NULL)
	//film_sensitivity(film_sensitivity_)
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

	up = ws_up;
	up.removeComponentInDir(forwards);
	up.normalise();

	right = ::crossProduct(forwards, up);

	assert(forwards.isUnitLength());
	assert(ws_up.isUnitLength());
	assert(up.isUnitLength());
	assert(right.isUnitLength());

	sensor_width = sensor_width_;
	sensor_height = sensor_width / aspect_ratio;
	lens_center = pos;
	lens_botleft = pos - up * lens_radius - right * lens_radius;
	sensor_center = lens_center - forwards * sensor_to_lens_dist;
	sensor_botleft = sensor_center - up * sensor_height * 0.5 - right * sensor_width * 0.5;
	sensor_to_lens_dist_focus_dist_ratio = sensor_to_lens_dist / focus_distance;
	focus_dist_sensor_to_lens_dist_ratio = focus_distance / sensor_to_lens_dist;
	recip_sensor_width = 1.0 / sensor_width;
	recip_sensor_height = 1.0 / sensor_height;
	uniform_lens_pos_pdf = 1.0 / (NICKMATHS_PI * lens_radius * lens_radius);
	uniform_sensor_pos_pdf = 1.0 / (sensor_width * sensor_height);

	try
	{
		const Vec3d whitepoint = ColourSpaceConverter::whitepoint(white_balance);
		colour_space_converter = new ColourSpaceConverter(whitepoint.x, whitepoint.y);
	}
	catch(ColourSpaceConverterExcep& e)
	{
		throw CameraExcep("ColourSpaceConverterExcep: " + e.what());
	}

	//------------------------------------------------------------------------
	//calculate polarising Vector
	//------------------------------------------------------------------------
	this->polarising_vec = getRightDir();
	
	const Vec2d components = Matrix2d::rotationMatrix(::degreeToRad(this->polarising_angle)) * Vec2d(1.0, 0.0);
	this->polarising_vec = getRightDir() * components.x + getCurrentUpDir() * components.y;
	assert(this->polarising_vec.isUnitLength());


	//TEMP
	//diffraction_filter = new DiffractionFilter(lens_radius, sensor_to_lens_dist);

	//TEMP
	/*Bitmap aperture_bitmap;
	PNGDecoder::decode("aperture.png", aperture_bitmap);

	aperture_image = new Array2d<float>(aperture_bitmap.getWidth(), aperture_bitmap.getHeight());

	for(int y=0; y<aperture_bitmap.getHeight(); ++y)
		for(int x=0; x<aperture_bitmap.getWidth(); ++x)
			aperture_image->elem(x, y) = (float)aperture_bitmap.getPixel(x, y)[0] / 255.0f;
*/
}


Camera::~Camera()
{
	delete diffraction_filter;
	delete colour_space_converter;
}


const Vec3d Camera::sampleSensor(const Vec2d& samples) const
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

const Vec3d Camera::sampleLensPos(const Vec2d& samples) const
{
	const Vec2d discpos = MatUtils::sampleUnitDisc(samples) * lens_radius;
	return lens_center + discpos.x * up + discpos.y * right;
}

double Camera::lensPosPDF(const Vec3d& lenspos) const
{
	if(aperture_image)
	{
		// Then the lens rectangle was sampled uniformly.
		// so pdf = 1 / 2d
		return 1.0 / (4.0 * lens_radius*lens_radius);
	}
	else
	{
		assert(lens_radius > 0.0);
		assert(epsEqual(uniform_lens_pos_pdf, 1.0 / (NICKMATHS_PI * lens_radius * lens_radius)));
		return uniform_lens_pos_pdf;
	}
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
	//The target point can be found by following the line from sensorpos, through the lens center,
	//and for a distance of focus_distance.

	const double sensor_up = distUpOnSensorFromCenter(sensorpos);
	const double sensor_right = distRightOnSensorFromCenter(sensorpos);

	const double target_up_dist = -sensor_up * focus_dist_sensor_to_lens_dist_ratio;
	const double target_right_dist = -sensor_right * focus_dist_sensor_to_lens_dist_ratio;

	const Vec3d target_point = lens_center + forwards * focus_distance + 
		up * target_up_dist + 
		right * target_right_dist;

	return normalise(target_point - lenspos);
}

double Camera::lensPosVisibility(const Vec3d& lenspos) const
{
	if(aperture_image)
	{
		const Vec2d normed_lenspoint = normalisedLensPosForWSPoint(lenspos);

		const int ap_image_x = (int)(normed_lenspoint.x * aperture_image->getWidth());
		if(ap_image_x < 0 || ap_image_x >= aperture_image->getWidth())
			return 0.0;

		const int ap_image_y = (int)((1.0 - normed_lenspoint.y) * aperture_image->getHeight()); // y increases downwards in aperture_image
		if(ap_image_y < 0 || ap_image_y >= aperture_image->getHeight())
			return 0.0;

		assert(Maths::inRange(aperture_image->elem(ap_image_x, ap_image_y), 0.f, 1.f));
		return (double)aperture_image->elem(ap_image_x, ap_image_y);
	}
	else
	{
		return 1.0;
	}
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
	const double sensor_up = -target_up * sensor_to_lens_dist_focus_dist_ratio;
	const double sensor_right = -target_right * sensor_to_lens_dist_focus_dist_ratio;

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










double Camera::traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	return -1.0f;//TEMP
}

js::AABBox tempzero(Vec3f(0,0,0), Vec3f(0,0,0));

const js::AABBox& Camera::getAABBoxWS() const
{
	return tempzero;
}


void Camera::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	return;
}
bool Camera::doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	return false;
}











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


void Camera::convertFromXYZToSRGB(Image& image) const
{
	assert(colour_space_converter);
	colour_space_converter->convertFromXYZToSRGB(image);
}

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

const Vec3d Camera::sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
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

const Vec2d Camera::getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const
{
	return Vec2d(0,0);
}

/*int Camera::UVSetIndexForName(const std::string& uvset_name) const
{
	::fatalError("Camera::UVSetIndexForName");
	return 0;
}*/


void Camera::unitTest()
{
	conPrint("Camera::unitTest()");

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
		"A",
		0.f, 
		1.f, // bloom radius
		false, // autofocus
		false, // polarising_filter
		0.f, //polarising_angle
		0.f, //glare_weight
		1.f, //glare_radius
		5, //glare
		1.f / 200.f //shutter_open_duration
		//800.f //film speed
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
	testAssert(dot(dir, cam.getCurrentUpDir()) > 0.0);//goes up
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
	Vec3d lenspos = cam.sampleLensPos(Vec2d(0.45654f, 0.8099f));
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

	lenspos = cam.sampleLensPos(Vec2d(0.76846, 0.38467)); // get a 'random' lens pos
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





















