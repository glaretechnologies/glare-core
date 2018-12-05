/*=====================================================================
camera.cpp
----------
File created by ClassTemplate on Sun Nov 14 04:06:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "camera.h"


Camera::Camera(CameraType camera_type_)
:	Geometry(
		false, // sub-elements curved
		true // is camera
	),
	camera_type(camera_type_)
{}


Camera::~Camera()
{}


void Camera::setContainingMedia(const std::vector<const Medium*>& media_)
{
	containing_media = media_;
}


void Camera::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const
{
	assert(0);
}


void Camera::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, 
										  /*float recip_sub_elem_area_ws, Real& p_out, */unsigned int& mat_index_out, Vec2f& uv0_out) const
{
	assert(0);
}


void Camera::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_abs_error_out, Vec2f& uv0_out) const
{
	assert(0);
}


void Camera::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const
{
	assert(0);
}


void Camera::getIntrinsicCoordsPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out) const
{
	assert(0);
}


const Camera::UVCoordsType Camera::getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoords_set, 
										Matrix2f& duv_dalphabeta_out
									   ) const
{
	assert(0);
	return UVCoordsType(0.f);
}


unsigned int Camera::getNumUVCoordSets() const { return 0; }


unsigned int Camera::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


bool Camera::subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const ArrayRef<Reference<Material> >& materials, const Matrix4f& object_to_camera, /*const CoordFramed& camera_coordframe_os, */ double pixel_height_at_dist_one,
		const std::vector<Planef>& camera_clip_planes, const std::vector<Planef>& section_planes_os, PrintOutput& print_output, bool verbose,
		ShouldCancelCallback* should_cancel_callback)
{
	return false; // camera can't be clipped
}


void Camera::build(const std::string& indigo_base_dir_path, const BuildOptions& options, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) {} // throws GeometryExcep
