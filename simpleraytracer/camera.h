/*=====================================================================
camera.h
--------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CAMERA_H_666_
#define __CAMERA_H_666_


#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/matrix3.h"
#include "ray.h"
#include "../physics/jscol_aabbox.h"
#include <vector>
#include "geometry.h"
#include "../graphics/image.h" //TEMP for diffraction
#include "../indigo/Spectral.h"
#include "../indigo/SampleTypes.h"
#include <memory>
#include <string>
class ColourSpaceConverter;
class HitInfo;
class FullHitInfo;
class Image;
class Medium;
class DiffractionFilter;
class Distribution2;
class Aperture;
class MTwister;

class CameraExcep
{
public:
	CameraExcep(const std::string& text_) : text(text_) {}
	~CameraExcep(){}

	const std::string& what() const { return text; }
private:
	std::string text;
};

/*=====================================================================
Camera
------

=====================================================================*/
class Camera : public Geometry
{
public:
	/*=====================================================================
	Camera
	------
	
	=====================================================================*/
	Camera(const Vec3d& pos, const Vec3d& ws_updir, const Vec3d& forwards, 
		double lens_radius, double focus_distance, double aspect_ratio, double sensor_width, double lens_sensor_dist, 
		//const std::string& white_balance, 
		double bloom_weight, double bloom_radius, bool autofocus, bool polarising_filter, 
		double polarising_angle,
		double glare_weight, double glare_radius, int glare_num_blades,
		double exposure_duration/*, double film_sensitivity*/,
		Aperture* aperture,
		const std::string& base_indigo_path,
		double lens_shift_up_distance,
		double lens_shift_right_distance
		);

	virtual ~Camera();


	const Vec3d sampleSensor(const SamplePair& samples) const;
	double sensorPDF(const Vec3d& pos) const;

	const Vec3d sampleLensPos(const SamplePair& samples/*, const Vec3d& sensorpos*/) const;
	double lensPosPDF(/*const Vec3d& sensorpos,*/ const Vec3d& lenspos) const;
	double lensPosSolidAnglePDF(const Vec3d& sensorpos, const Vec3d& lenspos) const;
	double lensPosVisibility(const Vec3d& lenspos) const;

	const Vec3d lensExitDir(const Vec3d& sensorpos, const Vec3d& lenspos) const;
	const Vec3d sensorPosForLensIncidentRay(const Vec3d& lenspos, const Vec3d& raydir, bool& hitsensor_out) const;

	const Vec2d imCoordsForSensorPos(const Vec3d& senserpos) const;
	const Vec3d sensorPosForImCoords(const Vec2d& imcoords) const;

	const Vec3d& getSensorCenter() const { return sensor_center; }


	////////////////////// Geometry interface ///////////////////
	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const;
	virtual const js::AABBox& getAABBoxWS() const;
	//virtual const std::string debugName() const;
	
	virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	const TexCoordsType getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumTexCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const;
	virtual void getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix3<Vec3RealType>& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const;

	virtual void subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one,
		const std::vector<Plane<double> >& camera_clip_planes);
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	//////////////////////////////////////////////////////////
	/*
	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string getName() const { return "Camera"; }
	
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const;

	virtual void getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const;
	virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const { return toVec3f(forwards); }
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const { return toVec3f(forwards); }
	virtual const TexCoordsType getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumTexCoordSets() const { return 0; }

	virtual void subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, 
		const std::vector<Plane<double> >& camera_clip_planes){}
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings) {} // throws GeometryExcep

	virtual void getSubElementSurfaceAreas(const Matrix3d& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Vec3d& pos_out, Vec3d& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Vec3d& pos, double sub_elem_area_ws) const;*/


	//void lookAt(const Vec3d& target);
	//void setForwardsDir(const Vec3d& forwards);
	//void setPos(const Vec3d& newpos) { pos = newpos; }

	inline const Vec3d& getUpDir() const { return up; }
	inline const Vec3d& getRightDir() const { return right; }
	inline const Vec3d& getForwardsDir() const { return forwards; }

	inline const Vec3d& getPos() const { return pos; }

	//const Vec2d getNormedImagePointForRay(const Vec3d& unitray, bool& fell_on_image_out) const;

	const Vec3d getRayUnitDirForImageCoords(double x, double y, double width, double height) const;
	const Vec3d getRayUnitDirForNormedImageCoords(double x, double y) const;

	double getLensRadius() const { return lens_radius; }

	void sampleRay(const Vec3d& target_dir, const SamplePair& samples, Vec3d& origin_out, Vec3d& unitdir_out,
		double wvlen) const;

	//assuming the image plane is sampled uniformly
	double getExitRaySolidAnglePDF(const Vec3d& dir) const;

	static void unitTest();

	//void convertFromXYZToSRGB(Image& image) const;

	double bloomRadius() const { return bloom_radius; }
	double bloomWeight() const { return bloom_weight; }
	double glareRadius() const { return glare_radius; }
	double glareWeight() const { return glare_weight; }
	int glareNumBlades() const { return glare_num_blades; }

	bool isAutoFocus() const { return autofocus; }
	// NOTE: non-const
	void setFocusDistance(double fd);


	// NOTE: non-const
	virtual void emitterInit();
	virtual const Vec3d sampleSurface(const SamplePair& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										  HitInfo& hitinfo_out) const;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const;
	virtual double surfaceArea(const Matrix3d& to_parent) const;


	bool polarisingFilter() const { return polarising_filter; }
	const Vec3d polarisingVec() const { return polarising_vec; }

	double getExposureDuration() const { return exposure_duration; }
	//double getFilmSensitivity() const { return film_sensitivity; }

	//virtual int UVSetIndexForName(const std::string& uvset_name) const;

	const Vec3d diffractRay(const SamplePair& samples, const Vec3d& dir, const SpectralVector& wavelengths, double direction_sign, SpectralVector& weights_out) const;

	static void applyDiffractionFilterToImage(const Image& cam_diffraction_filter_image, Image& image);
	void applyDiffractionFilterToImage(Image& image) const;

	const Image* getDiffractionFilterImage() const { return diffraction_filter_image.get(); }

	

	void prepareForDiffractionFilter(const std::string& base_indigo_path, int main_buffer_width, int main_buffer_height);
	void buildDiffractionFilter(/*const std::string& base_indigo_path*/) const;
	void buildDiffractionFilterImage(/*int main_buffer_width, int main_buffer_height, MTwister& rng, const std::string& base_indigo_path*/) const;


	double sensorHeight() const { return sensor_height; }
	double sensorLensDist() const { return sensor_to_lens_dist; }

	//double getHorizontalAngleOfView() const; // including to left and right, in radians
	//double getVerticalAngleOfView() const; // including to up and down, in radians

	//const Vec3d getLensCenterPos() const;
	//const Vec3d getSensorBottomMiddle() const;
	//cons

	void getViewVolumeClippingPlanes(std::vector<Plane<double> >& planes_out) const;
	

	std::vector<const Medium*> containing_media;
private:

	inline double distUpOnSensorFromCenter(const Vec3d& pos) const;
	inline double distRightOnSensorFromCenter(const Vec3d& pos) const;
	inline double distUpOnLensFromCenter(const Vec3d& pos) const;
	inline double distRightOnLensFromCenter(const Vec3d& pos) const;

	// Where x=0 is left, x=1 is on right of lens, y=0 is bottom, y=1 is top of lens.
	inline const Vec2d normalisedLensPosForWSPoint(const Vec3d& pos) const;

	//Array2d<float>* aperture_image;
	//Distribution2* aperture_image;

	mutable std::auto_ptr<DiffractionFilter> diffraction_filter; // Distribution for direct during-render sampling
	Aperture* aperture;
	mutable std::auto_ptr<Image> diffraction_filter_image; // Image for post-process convolution

	Vec3d pos;
	//Vec3d ws_up;
	Vec3d up;
	Vec3d right;
	Vec3d forwards;

	double lens_radius;
	double focal_length;
	double aspect_ratio;
	//double angle_of_view;

	/*double width;
	double width_2;
	double height;
	double height_2;*/

	double focus_distance;
	Vec3d sensor_center;
	Vec3d sensor_botleft;
	double sensor_width;
	double sensor_height;
	Vec3d lens_center;
	Vec3d lens_botleft;
	double recip_sensor_width;
	double recip_sensor_height;
	double sensor_to_lens_dist;

	double sensor_to_lens_dist_focus_dist_ratio;
	double focus_dist_sensor_to_lens_dist_ratio;
	//double uniform_lens_pos_pdf;
	double uniform_sensor_pos_pdf;

	double recip_unoccluded_aperture_area;
	

	double bloom_weight;
	double bloom_radius;
	double glare_weight;
	double glare_radius;
	int glare_num_blades;

	bool autofocus;

	bool polarising_filter;
	double polarising_angle;
	Vec3d polarising_vec;

	//ColourSpaceConverter* colour_space_converter;

	double exposure_duration; // aka shutter speed
	//double film_sensitivity; // aka ISO film sppeed

	double lens_shift_up_distance;
	double lens_shift_right_distance;

	// Stuff for Lazy calculation of diffraction filter
	std::string base_indigo_path;
	int main_buffer_width;
	int main_buffer_height;
};


double Camera::distUpOnSensorFromCenter(const Vec3d& x) const
{
	return dot(x - sensor_center, up);
}

double Camera::distRightOnSensorFromCenter(const Vec3d& x) const
{
	return dot(x - sensor_center, right);
}

double Camera::distUpOnLensFromCenter(const Vec3d& x) const
{
	return dot(x - lens_center, up);
}

double Camera::distRightOnLensFromCenter(const Vec3d& x) const
{
	return dot(x - lens_center, right);
}

const Vec2d Camera::normalisedLensPosForWSPoint(const Vec3d& x) const
{
	return Vec2d(dot(right, x) - dot(right, lens_botleft), dot(up, x) - dot(up, lens_botleft)) / (2.0 * lens_radius);
	//return Vec2d(dot(right, pos) - dot(right, lens_botleft), dot(up, pos) - dot(up, lens_botleft)) / (2.0 * lens_radius);
}



#endif //__CAMERA_H_666_




