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


class RaySphere : public Geometry
{
public:
	RaySphere(const Vec3d& pos_, double radius_);
	virtual ~RaySphere();

	
	//intersectable interface
	virtual double traceRay(const Ray& ray, double max_t, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const { return aabbox; }

	virtual void getAllHits(const Ray& ray, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceFullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::ObjectTreePerThreadData& context, const Object* object) const;

	virtual const Vec3d getShadingNormal(const FullHitInfo& hitinfo) const;
	virtual const Vec3d getGeometricNormal(const FullHitInfo& hitinfo) const;
	virtual const Vec2d getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const;

	//returns true if could construct a suitable basis
	virtual bool getTangents(const FullHitInfo& hitinfo, unsigned int texcoord_set, Vec3d& tangent_out, Vec3d& bitangent_out) const;

	virtual void emitterInit();
	virtual const Vec3d sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out, HitInfo& hitinfo_out) const;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const;
	virtual double surfaceArea(const Matrix3d& to_parent) const;

	virtual int UVSetIndexForName(const std::string& uvset_name) const { return 0; }

	static void test();

	virtual const std::string getName() const { return "RaySphere"; }

	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings) {} // throws GeometryExcep

private:
	Vec3d centerpos;

	double radius;

	//stuff below is precomputed for efficiency
	double radius_squared;
	double recip_radius;
	js::AABBox aabbox;
};





#endif //__RAYSPHERE_H__
