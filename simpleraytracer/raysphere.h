/*===================================================================

  
  digital liberation front 2002
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|        \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman[/ Ono-Sendai]
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __RAYSPHERE_H__
#define __RAYSPHERE_H__

#include "geometry.h"
#include "../maths/vec3.h"
#include "../physics/jscol_aabbox.h"


SSE_CLASS_ALIGN RaySphere : public Geometry
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	RaySphere(double radius_);
	virtual ~RaySphere();

	
	////////////////////// Geometry interface ///////////////////
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;
	virtual const js::AABBox& getAABBoxWS() const;
	
	//virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out) const;
	const TexCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumUVCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const;
	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const;

	virtual bool subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose);
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	virtual bool areSubElementsCurved() const;
	virtual Vec3RealType getBoundingRadius() const;
	//virtual const Vec3Type positionForHitInfo(const HitInfo& hitinfo) const;
	//////////////////////////////////////////////////////////

	static double rayMinT(double radius) { return 0.0001f/*0.0003f*//*0.00005f*/ * radius; }

	static void test();

private:
	js::AABBox aabbox;
	double radius;

	//stuff below is precomputed for efficiency
	double radius_squared;
	double recip_radius;
};





#endif //__RAYSPHERE_H__
