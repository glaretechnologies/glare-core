/*=====================================================================
camera.cpp
----------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004Code By Nicholas Chapman.
=====================================================================*/
#include "camera.h"


#include "../maths/vec2.h"
#include "../maths/matrix2.h"
#include "../raytracing/matutils.h"
#include "../indigo/SingleFreq.h"
#include "../utils/stringutils.h"
#include "../graphics/image.h"
#include "../indigo/globals.h"

Camera::Camera(const Vec3d& pos_, const Vec3d& ws_updir, const Vec3d& forwards_, 
		double lens_radius_, double focus_distance_, double aspect_ratio_, double sensor_width_, double lens_sensor_dist_, 
		const std::string& white_balance, double bloom_weight_, double bloom_radius_, bool autofocus_, 
		double polarising_filter_, double polarising_angle_,
		double glare_weight_, double glare_radius_, int glare_num_blades_,
		double exposure_duration_/*, float film_sensitivity_*/)
:	pos(pos_),
	ws_up(ws_updir),
	forwards(forwards_),
	lens_radius(lens_radius_),
	focal_length(0.0f),
	focus_distance(focus_distance_),
	aspect_ratio(aspect_ratio_),
	angle_of_view(0.0f),
	sensor_to_lens_dist(lens_sensor_dist_),
	bloom_weight(bloom_weight_),
	bloom_radius(bloom_radius_),
	autofocus(autofocus_),
	polarising_filter(polarising_filter_),
	polarising_angle(polarising_angle_),
	glare_weight(glare_weight_),
	glare_radius(glare_radius_),
	glare_num_blades(glare_num_blades_),
	exposure_duration(exposure_duration_)
	//film_sensitivity(film_sensitivity_)
{
//	containing_medium = NULL;

	up = ws_up;
	up.removeComponentInDir(forwards);
	up.normalise();

	right = ::crossProduct(forwards, up);

	sensor_width = sensor_width_;
	sensor_height = sensor_width / aspect_ratio;
	lens_center = pos;
	sensor_center = lens_center - forwards * sensor_to_lens_dist;
	sensor_botleft = sensor_center - up * sensor_height * 0.5 - right * sensor_width * 0.5;

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
}




Camera::~Camera()
{
	
}

double Camera::distUpOnSensorFromCenter(const Vec3d& pos) const
{
	return dot(pos - sensor_center, up);
}

double Camera::distRightOnSensorFromCenter(const Vec3d& pos) const
{
	return dot(pos - sensor_center, right);
}

double Camera::distUpOnLensFromCenter(const Vec3d& pos) const
{
	return dot(pos - lens_center, up);
}

double Camera::distRightOnLensFromCenter(const Vec3d& pos) const
{
	return dot(pos - lens_center, right);
}

const Vec3d Camera::sampleSensor(const Vec2d& samples) const
{
	return sensor_botleft + 
		right * samples.x * sensor_width + 
		up * samples.y * sensor_height;
}

double Camera::sensorPDF(const Vec3d& pos) const
{
	const double A = sensor_width * sensor_height;
	return 1.0 / A;
}

const Vec3d Camera::sampleLensPos(const Vec2d& samples) const
{
	const Vec2d discpos = MatUtils::sampleUnitDisc(samples) * (double)lens_radius;
	return lens_center + discpos.x * up + discpos.y * right;
}

double Camera::lensPosPDF(/*const Vec3d& sensorpos, */const Vec3d& lenspos) const
{
	assert(lens_radius > 0.0);
	const double A = (double)NICKMATHS_PI * (double)lens_radius * (double)lens_radius;
	return 1.0 / A;
}

double Camera::lensPosSolidAnglePDF(const Vec3d& sensorpos, const Vec3d& lenspos) const
{
	//NOTE: wtf is up with this func???

	const double pdf_A = lensPosPDF(lenspos);

	//NOTE: this is a very slow way of doing this!!!!
	const double costheta = fabs(dot(forwards, normalise(lenspos - sensorpos)));

	return MatUtils::areaToSolidAnglePDF((double)pdf_A, lenspos.getDist2(sensorpos), costheta);

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


	const double target_up_dist = -sensor_up * focus_distance / sensor_to_lens_dist;
	const double target_right_dist = -sensor_right * focus_distance / sensor_to_lens_dist;

	const Vec3d target_point = lens_center + forwards * focus_distance + 
		up * target_up_dist + 
		right * target_right_dist;

	return normalise(target_point - lenspos);

}
const Vec3d Camera::sensorPosForLensIncidentRay(const Vec3d& lenspos, const Vec3d& raydir, bool& hitsensor_out) const
{
	hitsensor_out = true;

	//follow ray back to focus_distance away to the target point
	double lens_up = distUpOnLensFromCenter(lenspos);
	double lens_right = distRightOnLensFromCenter(lenspos);
	double forwards_comp = -dot(raydir, forwards);

	if(forwards_comp <= 0.0f)
	{
		hitsensor_out = false;
		return Vec3d(0,0,0);
	}
	
	//compute the up and right components of target point
	double target_up = lens_up - dot(raydir, up) * focus_distance / forwards_comp;
	double target_right = lens_right - dot(raydir, right) * focus_distance / forwards_comp;

	//compute corresponding sensorpos
	double sensor_up = -target_up * sensor_to_lens_dist / focus_distance;
	double sensor_right = -target_right * sensor_to_lens_dist / focus_distance;

	return sensor_center + up * sensor_up + right * sensor_right;
}

	//const Vec3d sampleSensorExirDir(const Vec2& samples, const Vec3d& pos);
	//float sensorExitDirPDF(const Vec3d& pos, const Vec3d& exitdir);

const Vec2d Camera::imCoordsForSensorPos(const Vec3d& sensorpos) const
{
	const double sensor_up_dist = dot(sensorpos - sensor_botleft, up);
	const double sensor_right_dist = dot(sensorpos - sensor_botleft, right);
	
	return Vec2d(1.0f - (sensor_right_dist / sensor_width), sensor_up_dist / sensor_height);
}
const Vec3d Camera::sensorPosForImCoords(const Vec2d& imcoords) const
{
	return sensor_botleft + 
		up * imcoords.y * sensor_height + 
		right * (1.0f - imcoords.x) * sensor_width;
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


void Camera::convertFromXYZToSRGB(Image& image)
{
	assert(colour_space_converter);
	colour_space_converter->convertFromXYZToSRGB(image);
}


void Camera::setFocusDistance(double fd)
{
	this->focus_distance = fd;
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

int Camera::UVSetIndexForName(const std::string& uvset_name) const
{
	::fatalError("Camera::UVSetIndexForName");
	return 0;
}


void Camera::unitTest()
{
	
	


	/*Camera(const Vec3d& pos, const Vec3d& ws_updir, const Vec3d& forwards, 
		float lens_radius, float focus_distance, float aspect_ratio, float sensor_width, float lens_sensor_dist, 
		const std::string& white_balance, float bloom_weight, float bloom_radius, bool autofocus, float polarising_filter, float polarising_angle);
*/
	const float aov = 90.0f;
	const Vec3d forwards(0,1,0);
	Camera cam(
		Vec3d(0,0,0), //pos
		Vec3d(0,0,1), //up
		forwards, //forwards
		0.0f, //lens_radius
		10.0f, //focus distance
		1.333f, //aspect ration
		0.25f, //sensor_width
		0.25, //lens_sensor_dist
		"A",
		0.f, 0.f, false, false, 0.f,
		0.f, 0.f, 5, //glare
		1.f / 200.f //shutter_open_duration
		//800.f //film speed
		);



	Vec3d sensorpos = cam.sensorPosForImCoords(Vec2d(0.5f, 0.5f));
	assert(::epsEqual(sensorpos.x, 0.0));
	assert(::epsEqual(sensorpos.z, 0.0));
	//assert(::epsEqual(dot(forwards, cam.getPos() - sensorpos), 0.0f));

	Vec3d v = cam.lensExitDir(sensorpos, cam.getPos());
	assert(epsEqual(v, forwards));


	bool hitsensor;
	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), cam.getForwardsDir() * -1.0f, hitsensor);
	assert(hitsensor);
	assert(epsEqual(sensorpos, cam.getSensorCenter()));
	Vec2d imcoords = cam.imCoordsForSensorPos(sensorpos);
	assert(epsEqual(imcoords, Vec2d(0.5f, 0.5f)));

	//conversion to sensorpos and back
	imcoords = Vec2d(0.1f, 0.1f);
	sensorpos = cam.sensorPosForImCoords(imcoords);
	assert(epsEqual(cam.imCoordsForSensorPos(sensorpos), imcoords));



	//near bottom left of sensor as seen facing in backwards dir
	sensorpos = cam.sensorPosForImCoords(Vec2d(0.1f, 0.1f));
	//so ray should go a) up and b)left
	Vec3d dir = cam.lensExitDir(sensorpos, cam.getPos());
	assert(dot(dir, cam.getCurrentUpDir()) > 0.0f);//goes up
	assert(dot(dir, cam.getRightDir()) < 0.0f);//goes left

	//hello

	Vec3d indir = normalise(Vec3d(-1, -1, 0));//45 deg angle
	sensorpos = cam.sensorPosForLensIncidentRay(cam.getPos(), indir, hitsensor);
	assert(::epsEqual(sensorpos.x, -cam.sensor_to_lens_dist));
	assert(::epsEqual(sensorpos.y, -cam.sensor_to_lens_dist));
	assert(::epsEqual(sensorpos.z, 0.0));

	dir = cam.lensExitDir(sensorpos, cam.getPos());
	assert(dir.x > 0.0f);
	assert(dir.y > 0.0f);
	assert(epsEqual(dir, normalise(Vec3d(1,1,0))));


	//sample an exit direction
	sensorpos = cam.sensorPosForImCoords(Vec2d(0.1f, 0.8f));
	Vec3d lenspos = cam.sampleLensPos(Vec2d(0.45654f, 0.8099f));
	v = cam.lensExitDir(sensorpos, lenspos);

	//cast ray back into camera and make sure it hits at same sensor pos
	Vec3d sensorpos2 = cam.sensorPosForLensIncidentRay(lenspos, v*-1.0f, hitsensor);
	assert(hitsensor);
	assert(::epsEqual(sensorpos, sensorpos2));

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





















