/*=====================================================================
raysphere.h
-----------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include "geometry.h"
#include "../maths/vec3.h"
#include "../physics/jscol_aabbox.h"


class RaySphere : public Geometry
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	RaySphere(const Vec4f& centre, double radius_);
	virtual ~RaySphere();

	////////////////////// Geometry interface ///////////////////
	virtual DistType traceRay(const Ray& ray, ThreadContext& thread_context, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual const js::AABBox getAABBox() const;
	
	virtual const Vec3Type getGeometricNormalAndMatIndex(const HitInfo& hitinfo, unsigned int& mat_index_out) const;
	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_abs_error_out, Vec3Type& N_g_out) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_abs_error_out, Vec2f& uv0_out) const;
	virtual const UVCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual const UVCoordsType getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& duv_dalphabeta_out) const;
	virtual unsigned int getNumUVCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const;
	virtual void getIntrinsicCoordsPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, 
		unsigned int& mat_index_out, Vec2f& uv0_out) const;

	virtual bool subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const ArrayRef<Reference<Material> >& materials,/*const Object& object, */const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Planef>& camera_clip_planes, const std::vector<Planef>& section_planes_os, PrintOutput& print_output, bool verbose, ShouldCancelCallback* should_cancel_callback);
	virtual void build(const BuildOptions& options, ShouldCancelCallback& should_cancel_callback, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager);
	virtual const std::string getName() const;
	virtual Real meanCurvature(const HitInfo& hitinfo) const;
	virtual bool isPlanar(Vec4f& normal_out) const { return false; }
	//////////////////////////////////////////////////////////

	static Real rayMinT(Real radius) { return 0.0001f/*0.0003f*//*0.00005f*/ * radius; }

	static void test();

private:
	js::AABBox aabbox;
	Vec4f centre;
	Real radius;
	// Stuff below is precomputed for efficiency.
	Real radius_squared;
	Real recip_radius;
	Real area; // sampling prob. density - 1 / area = 1 / (4*pi*r^2)
};
