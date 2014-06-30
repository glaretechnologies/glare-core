/*=====================================================================
camera.h
--------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "ray.h"
#include "geometry.h"
#include "../indigo/Spectral.h"
#include "../indigo/SampleTypes.h"
#include "../indigo/TransformPath.h"
#include "../maths/SSE.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../physics/jscol_aabbox.h"
#include "../utils/Reference.h"
#include <vector>
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

	enum CameraType
	{
		CameraType_ThinLens,
		CameraType_Orthographic,
		CameraType_Spherical
	};


	Camera(CameraType camera_type);
	virtual ~Camera();

	typedef float PDType;

	virtual void init(
		const Vec3d& cam_pos, const Vec3d& ws_updir, const Vec3d& forwards, 
		double lens_radius, double focus_distance, double sensor_width, double sensor_height, double lens_sensor_dist,
		double exposure_duration,
		Reference<Aperture>& aperture,
		double lens_shift_up_distance,
		double lens_shift_right_distance,
		bool build_alpha_splat_factor_data) = 0;


	struct SampleLensResults
	{
		Vec3Type forwards_dir_ws;
		Vec3Type sensorpos_ws;
		Vec3Type lenspos_ws;
		Vec3Type lens_exit_dir_ws;
		const std::vector<const Medium*>* containing_media;
		Real sensorpos_p;
		Real recip_lenspos_p;
		Real aperture_vis;
		Real F; // camera contribution
		Real exposure_duration;
		Real sensitivity_scale;
		//Real sensorpos_to_lenspos_dist;
		Real alpha_splat_weight;
	};


	virtual SampleLensResults sampleLens(
		const Vec2d& imagepos,
		const SamplePair& samples,
		double time,
		bool vignetting,
		Real recip_normed_image_rect_area,
		bool compute_alpha_weight
	) const = 0;


	virtual PDType sensitivityScale() const = 0;


	virtual PDType sensorPDF() const = 0; // used

	virtual void sampleLensPos(const SamplePair& samples, double time, Vec3Type& pos_os_out, Vec3Type& pos_ws_out) const = 0; // used
	virtual PDType lensPosPDF(const Vec3Type& lenspos_os) const = 0; // used
	virtual double lensPosVisibility(const Vec3Type& lenspos_os) const = 0; // only used in old code.

	virtual const Vec3Type lensExitDir(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const = 0; // only used in old code.
	virtual void sensorPosForLensIncidentRay(const Vec3Type& lenspos_ws, const Vec3Type& raydir, double time, bool& hitsensor_out, Vec3Type& sensorpos_os_out, Vec3Type& sensorpos_ws_out) const = 0;

	virtual const Vec2d imCoordsForSensorPos(const Vec3Type& sensorpos_os, double time) const = 0;
	virtual void sensorPosForImCoords(const Vec2d& imcoords, Vec3Type& pos_os_out) const = 0;


	virtual const Vec4f getForwardsDirF(double time) const = 0;

	virtual const Vec4f getPosWS(double time) const = 0;
	

	virtual Real getExposureDuration() const = 0;


	virtual const Vec3Type diffractRay(const SamplePair& samples, const Vec3Type& dir, const SpectralVector& wavelengths, float direction_sign, double time, SpectralVector& weights_out) const = 0;


	virtual void prepareForDiffractionFilter(int main_buffer_width, int main_buffer_height, int ssf_) = 0;
	virtual void buildDiffractionFilter() = 0;


	virtual const std::vector<Plane<Vec3RealType> >& getViewVolumeClippingPlanesCameraSpace() const = 0;

	

	virtual const TransformPath& getTransformPath() const = 0;


	virtual bool isAutoFocus() const = 0;
	virtual void setFocusDistance(double fd) = 0; // NOTE: non-const

	virtual double imageHeightAtDistanceOne() const = 0;

	// Used in IndigoDriver::traceRay() for picking.
	virtual void getRayForImagePos(const Vec2d& image_coordinates, double time, Vec4f& pos_ws_out, Vec4f& dir_ws_out) const = 0;



	/////////////// Non-virtual methods //////////////
	inline CameraType getCameraType() const { return camera_type; }

	void setContainingMedia(const std::vector<const Medium*>& media);
	inline const std::vector<const Medium*>& getContainingMedia() const { return containing_media; }

protected:
	std::vector<const Medium*> containing_media;
private:
	CameraType camera_type;
};
