/*=====================================================================
camera.h
--------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CAMERA_H_666_
#define __CAMERA_H_666_


#include "../maths/SSE.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../maths/matrix3.h"
#include "ray.h"
#include "../physics/jscol_aabbox.h"
#include <vector>
#include "geometry.h"
#include "../indigo/Spectral.h"
#include "../indigo/SampleTypes.h"
#include "../indigo/TransformPath.h"
#include "../utils/reference.h"
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
class FFTPlan;


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
SSE_CLASS_ALIGN Camera : public Geometry
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	/*=====================================================================
	Camera
	------
	
	=====================================================================*/
	Camera(
		const js::Vector<TransformKeyFrame, 16>& frames,
		const Vec3d& ws_updir, const Vec3d& forwards, 
		double lens_radius, double focus_distance, double sensor_width, double sensor_height, double lens_sensor_dist, 
		double bloom_weight, double bloom_radius, bool autofocus, bool polarising_filter, 
		double polarising_angle,
		double glare_weight, double glare_radius, int glare_num_blades,
		double exposure_duration,
		Reference<Aperture>& aperture,
		const std::string& appdata_path,
		double lens_shift_up_distance,
		double lens_shift_right_distance,
		bool write_aperture_preview
		);

	virtual ~Camera();

	typedef float PDType;
	//typedef Vec4f VecType;


	void init(
		const Vec3d& cam_pos, const Vec3d& ws_updir, const Vec3d& forwards, 
		double lens_radius, double focus_distance, double sensor_width, double sensor_height, double lens_sensor_dist,
		double exposure_duration,
		Reference<Aperture>& aperture,
		double lens_shift_up_distance,
		double lens_shift_right_distance);


	//const Vec3d sampleSensor(const SamplePair& samples, double time) const;
	PDType sensorPDF() const;

	void sampleLensPos(const SamplePair& samples, double time, Vec3Type& pos_os_out, Vec3Type& pos_ws_out) const;
	PDType lensPosPDF(const Vec3Type& lenspos_os) const;
	PDType lensPosSolidAnglePDF(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const;
	double lensPosVisibility(const Vec3Type& lenspos_os) const;

	const Vec3Type lensExitDir(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const;
	void sensorPosForLensIncidentRay(const Vec3Type& lenspos_ws, const Vec3Type& raydir, double time, bool& hitsensor_out, Vec3Type& sensorpos_os_out, Vec3Type& sensorpos_ws_out) const;

	// float pdScalingFactor(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os) const;

	const Vec2d imCoordsForSensorPos(const Vec3Type& sensorpos_os, double time) const;
	void sensorPosForImCoords(const Vec2d& imcoords, Vec3Type& pos_os_out) const;

	inline const Vec3Type worldToCameraObjectSpace(const Vec3Type& pos_ws, double time) const { return transform_path.vecToLocal(pos_ws, time); }
	inline PDType lensPosPDFForWSPos(const Vec3Type& lenspos_ws, double time) const { return lensPosPDF(worldToCameraObjectSpace(lenspos_ws, time)); }



	//const Vec3d& getSensorCenter(double time) const;// { return sensor_center; }


	////////////////////// Geometry interface ///////////////////
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;
	virtual const js::AABBox& getAABBoxWS() const;
	//virtual const std::string debugName() const;
	
	virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out) const;
	const TexCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumUVCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const;
	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const;

	virtual void subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, PrintOutput& print_output, bool verbose);
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	virtual bool areSubElementsCurved() const;
	virtual Vec3RealType getBoundingRadius() const;
	virtual const Vec3Type positionForHitInfo(const HitInfo& hitinfo) const;
	//////////////////////////////////////////////////////////


	void setPosForwardsWS(Vec3d cam_pos, Vec3d cam_dir);

	const Vec3d getUpDir(double time) const;
	const Vec3d getRightDir(double time) const;
	const Vec3d getForwardsDir(double time) const;

	const Vec4f getForwardsDirF(double time) const;

	const Vec3d getPosWS(double time) const;
	//const Vec3d getPos(double time) const;

	//const Vec2d getNormedImagePointForRay(const Vec3d& unitray, bool& fell_on_image_out) const;

	//const Vec3d getRayUnitDirForImageCoords(double x, double y, double width, double height) const;
	//const Vec3d getRayUnitDirForNormedImageCoords(double x, double y) const;

	//double getLensRadius() const { return lens_radius; }

	//void sampleRay(const Vec3d& target_dir, const SamplePair& samples, Vec3d& origin_out, Vec3d& unitdir_out,
	//	double wvlen) const;

	//assuming the image plane is sampled uniformly
	//double getExitRaySolidAnglePDF(const Vec3d& dir) const;

	static void unitTest();

	bool isAutoFocus() const { return autofocus; }
	
	// NOTE: non-const
	void setFocusDistance(double fd);


	// NOTE: non-const
	virtual void emitterInit();
	virtual const Vec3d sampleSurface(const SamplePair& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										  HitInfo& hitinfo_out) const;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const;
	virtual double surfaceArea(const Matrix3d& to_parent) const;
	

	//bool polarisingFilter() const { return polarising_filter; }
	//const Vec3d polarisingVec() const { return polarising_vec; }

	double getExposureDuration() const { return exposure_duration; }

	const Vec3d diffractRay(const SamplePair& samples, const Vec3d& dir, const SpectralVector& wavelengths, double direction_sign, double time, SpectralVector& weights_out) const;


	void prepareForDiffractionFilter(int main_buffer_width, int main_buffer_height, int ssf_);
	void buildDiffractionFilter();


	double sensorWidth() const { return sensor_width; }
	double sensorHeight() const { return sensor_height; }
	double sensorLensDist() const { return sensor_to_lens_dist; }
	double lensRadius() const { return lens_radius; }
	double lensWidth() const { return lens_width; }
	double focusDistance() const { return focus_distance; }

	double focalLength() const;
	double fStop() const;

	const Vec4f sensorCenter() const { return Vec4f((float)sensor_center.x, (float)sensor_center.y, (float)sensor_center.z, 1.f); }
	const Vec4f lensCenter() const { return Vec4f((float)lens_center.x, (float)lens_center.y, (float)lens_center.z, 1.f); }

	//double getHorizontalAngleOfView() const; // including to left and right, in radians
	//double getVerticalAngleOfView() const; // including to up and down, in radians

	//const Vec3d getLensCenterPos() const;
	//const Vec3d getSensorBottomMiddle() const;

	//void getViewVolumeClippingPlanesCameraSpace(std::vector<Plane<Vec3RealType> >& planes_out) const;
	const std::vector<Plane<Vec3RealType> >& getViewVolumeClippingPlanesCameraSpace() const { return clipping_planes_camera_space; }
	

	TransformPath transform_path; // SSE Aligned
	js::AABBox bbox_ws; // SSE Aligned

	std::vector<const Medium*> containing_media;

	// READ ONLY
	double focus_dist_sensor_to_lens_dist_ratio;
	double uniform_sensor_pos_pdf;
	double recip_unoccluded_aperture_area;
	double lens_shift_up_distance;
	double lens_shift_right_distance;
	double focus_distance;



private:
	inline double distUpOnSensorFromCenter(const Vec3d& pos) const;
	inline double distRightOnSensorFromCenter(const Vec3d& pos) const;
	inline double distUpOnLensFromCenter(const Vec3d& pos) const;
	inline double distRightOnLensFromCenter(const Vec3d& pos) const;

	// Where x=0 is left, x=1 is on right of lens, y=0 is bottom, y=1 is top of lens.
	inline const Vec2d normalisedLensPosForWSPoint(const Vec3d& pos, double time) const;
	inline const Vec2d normalisedLensPosForOSPoint(const Vec3d& pos, double time) const;

	void makeClippingPlanesCameraSpace();

	std::vector<Plane<Vec3RealType> > clipping_planes_camera_space;


	std::auto_ptr<DiffractionFilter> diffraction_filter; // Distribution for direct during-render sampling
	Reference<Aperture> aperture;

	double lens_radius;
	double lens_width;
	double focal_length;
	double recip_lens_width;

	Vec3d sensor_center;
	Vec3d sensor_botleft;
	double sensor_width;
	double sensor_height;
	Vec3d lens_center;
	double recip_sensor_width;
	double recip_sensor_height;
	double sensor_to_lens_dist;

	double sensor_to_lens_dist_focus_dist_ratio;

	

	double bloom_weight;
	double bloom_radius;
	double glare_weight;
	double glare_radius;
	int glare_num_blades;

	bool autofocus;

	bool polarising_filter;
	double polarising_angle;
	Vec3d polarising_vec;

	double exposure_duration; // aka shutter speed

	// Stuff for Lazy calculation of diffraction filter
	std::string appdata_path;
	int ssf;
	int main_buffer_width;
	int main_buffer_height;

	bool write_aperture_preview;
};


#endif //__CAMERA_H_666_
