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
#include "../utils/reference.h"
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
		CameraType_Orthographic
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
		double lens_shift_right_distance) = 0;


	virtual void sampleLens(
		const Vec2d& imagepos,
		const SamplePair& samples,
		double time,
		bool vignetting,
		Real recip_normed_image_rect_area,
		Vec3Type& sensorpos_ws_out,
		Vec3Type& lenspos_ws_out,
		Vec3Type& sensor_to_lens_pos_os_out, // Needed for bidir
		Vec3Type& lens_exit_dir_ws_out,
		Real& sensorpos_p_out,
		Real& lenspos_p_out,
		Real& aperture_vis_out,
		Real& F_out // camera contribution
	) const = 0;


	virtual PDType sensitivityScale() const = 0;


	virtual PDType sensorPDF() const = 0; // used

	virtual void sampleLensPos(const SamplePair& samples, double time, Vec3Type& pos_os_out, Vec3Type& pos_ws_out) const = 0; // used
	virtual PDType lensPosPDF(const Vec3Type& lenspos_os) const = 0; // used
	virtual PDType lensPosSolidAnglePDF(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const = 0; // only used in generic path tracer.  TODO: remove.
	virtual double lensPosVisibility(const Vec3Type& lenspos_os) const = 0; // only used in old code.

	virtual const Vec3Type lensExitDir(const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, double time) const = 0; // only used in old code.
	virtual void sensorPosForLensIncidentRay(const Vec3Type& lenspos_ws, const Vec3Type& raydir, double time, bool& hitsensor_out, Vec3Type& sensorpos_os_out, Vec3Type& sensorpos_ws_out) const = 0;

	virtual const Vec2d imCoordsForSensorPos(const Vec3Type& sensorpos_os, double time) const = 0;
	virtual void sensorPosForImCoords(const Vec2d& imcoords, Vec3Type& pos_os_out) const = 0;

	virtual const Vec3Type worldToCameraObjectSpace(const Vec3Type& pos_ws, double time) const = 0;
	virtual PDType lensPosPDFForWSPos(const Vec3Type& lenspos_ws, double time) const = 0; // only used in old code.


	virtual const Vec4f getForwardsDirF(double time) const = 0;

	virtual const Vec4f getPosWS(double time) const = 0;
	

	virtual Real getExposureDuration() const = 0;


	virtual const Vec3Type diffractRay(const SamplePair& samples, const Vec3Type& dir, const SpectralVector& wavelengths, float direction_sign, double time, SpectralVector& weights_out) const = 0;


	virtual void prepareForDiffractionFilter(int main_buffer_width, int main_buffer_height, int ssf_) = 0;
	virtual void buildDiffractionFilter() = 0;


	virtual const std::vector<Plane<Vec3RealType> >& getViewVolumeClippingPlanesCameraSpace() const = 0;

	virtual void setContainingMedia(const std::vector<const Medium*>& media) = 0;
	virtual const std::vector<const Medium*>& getContainingMedia() const = 0;

	virtual const TransformPath& getTransformPath() const = 0;


	virtual bool isAutoFocus() const = 0;
	virtual void setFocusDistance(double fd) = 0; // NOTE: non-const

	virtual double imageHeightAtDistanceOne() const = 0;

	virtual Real cameraContribution(Real T, Real A_vis, const Vec3Type& sensorpos_os, const Vec3Type& lenspos_os, Real sensor_pd_A, Real lens_pd_A, bool vignetting) const = 0;


	inline CameraType getCameraType() const { return camera_type; }
private:
	CameraType camera_type;
};
