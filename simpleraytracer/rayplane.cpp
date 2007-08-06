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
#include "rayplane.h"

#include "ray.h"
#include "../raytracing/RayBundle.h"
#include "../indigo/FullHitInfo.h"
#include "../raytracing/hitinfo.h"


RayPlane::RayPlane(const Plane& plane_)
:	plane(plane_)
{
	//try and create a tightly fitting AABB
	
	const float big = 1e10f;
	//TEMP
	/*
	if(plane.getNormal() == Vec3(0,0,1) || plane.getNormal() == Vec3(0,0,-1))
		aabbox = js::AABBox(Vec3(-big, -big, plane.getDist()), Vec3(big, big, plane.getDist()));
	else if(plane.getNormal() == Vec3(0,1,0) || plane.getNormal() == Vec3(0,-1,0))
		aabbox = js::AABBox(Vec3(-big, plane.getDist(), -big), Vec3(big, plane.getDist(), big));
	else if(plane.getNormal() == Vec3(1,0,0) || plane.getNormal() == Vec3(-1,0,0))
		aabbox = js::AABBox(Vec3(plane.getDist(), -big, -big), Vec3(plane.getDist(), big, big));
	else*/
		aabbox = js::AABBox(Vec3f(-big, -big, -big), Vec3f(big, big, big));
}


const js::AABBox& RayPlane::getAABBoxWS() const
{
	return aabbox;
}

double RayPlane::traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	hitinfo_out.hittriindex = 0;
	//NOTE: deal with div by 0?

	return (ray.startPos().dot(plane.getNormal()) - plane.getDist()) / 
			-dot(plane.getNormal(), ray.unitDir());
}

bool RayPlane::doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	HitInfo hitinfo;
	const double hitdist = traceRay(ray, raylength, context, hitinfo);
	
	return hitdist >= 0.0f && hitdist < raylength;
}

/*void RayPlane::traceBundle(const RayBundle& raybundle, std::vector<FullHitInfo>& hitinfo_out)
{
	const float numerator = raybundle.origin.dot(plane.getNormal()) - plane.getDist();

	for(int i=0; i<4; ++i)
	{
		const Vec3 rayunitdir(raybundle.dirs[i], raybundle.dirs[i+4], raybundle.dirs[i+8]);

		hitinfo_out[i].dist = numerator / -dot(plane.getNormal(), rayunitdir);//raybundle.unitdirs[i]);
		hitinfo_out[i].hittri_index = 0;
	}
}*/

void RayPlane::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	HitInfo hitinfo;
	const double dist = traceRay(ray, 1.0e20f, context, hitinfo);
	if(dist >= 0.f)
	{
		//TEMP: this is more or less a total hack, need to fill out all info
		hitinfos_out.resize(1);
		hitinfos_out.back().dist = dist;
		hitinfos_out.back().hitpos = ray.startPos() + ray.unitDir() * dist;
	}


	//::fatalError("RayPlane::getAllHits: not implemented.");
	//hitinfos_out.push_back(FullHitInfo());
}


/*const Vec2 RayPlane::getPrimaryTexCoords(unsigned int tri_index, float tri_u, float tri_v, 
		const Vec3& ob_intersect_pos) const
{
	return Vec2(ob_intersect_pos.x * 0.2f, ob_intersect_pos.y * 0.2f);
}*/

const Vec2d RayPlane::getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const
{
	//NOTE: this is only gonna work well for x-y planes
	return Vec2d(hitinfo.hitpos.x, hitinfo.hitpos.y);
}


void RayPlane::emitterInit()
{
	::fatalError("Planes may not be emitters.");
}

const Vec3d RayPlane::sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										 HitInfo& hitinfo_out) const
{
	assert(0);
	return Vec3d(0.f, 0.f, 0.f);
}
double RayPlane::surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const
{
	assert(0);
	return 0.f;
}
double RayPlane::surfaceArea(const Matrix3d& to_parent) const
{
	fatalError("RayPlane::surfaceArea()");
	return 0.f;
}

//void RayPlane::getHitInfo(const Vec3& hitpos, Vec2& uvs_out/*, int& hitindex_out*/) const
//{
//	uvs_out.x = hitpos.x * 0.2f;
//	uvs_out.y = hitpos.y * 0.2f;
//	/*hitindex_out = 0;*/
//}




int RayPlane::UVSetIndexForName(const std::string& uvset_name) const
{
	return 0;
}