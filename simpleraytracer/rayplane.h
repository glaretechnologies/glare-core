/*===================================================================

  
  digital liberation front 2002
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman[/ Ono-Sendai]
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __RAYPLANE_H__
#define __RAYPLANE_H__

#include "geometry.h"
#include "../maths/plane.h"
#include "../maths/vec2.h"
#include "../physics/jscol_aabbox.h"

class RayPlane : public Geometry
{
public:
	RayPlane(const Plane& plane);
	~RayPlane(){}

	/// intersectable interface ///
	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	/// End intersectable interface ///

	virtual void getAllHits(const Ray& ray, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceFullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context, const Object* object) const;

	virtual const Vec3d getShadingNormal(const FullHitInfo& hitinfo) const { return plane.getNormal(); }
	virtual const Vec3d getGeometricNormal(const FullHitInfo& hitinfo) const { return plane.getNormal(); }
	virtual const Vec2d getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const;

	virtual void emitterInit();
	virtual const Vec3d sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										  HitInfo& hitinfo_out) const;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const;
	virtual double surfaceArea(const Matrix3d& to_parent) const;

	virtual int UVSetIndexForName(const std::string& uvset_name) const;

	virtual const std::string debugName() const { return "RayPlane"; }
private:
	Plane plane;
	Vec2d uvs;
	js::AABBox aabbox;
};


#endif //__RAYPLANE_H__
