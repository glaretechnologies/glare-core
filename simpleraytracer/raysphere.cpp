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
#include "raysphere.h"

#include "ray.h"
#include "../indigo/FullHitInfo.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/TestUtils.h"
#include "../physics/jscol_TriTreePerThreadData.h"
#include <algorithm>

RaySphere::RaySphere(const Vec3d& pos_, double radius_)
{
	centerpos = pos_;
	radius = radius_;
	radius_squared = radius_ * radius_;
	recip_radius = 1.0 / radius;

	aabbox = js::AABBox(
		toVec3f(centerpos - Vec3d(radius, radius, radius)), 
		toVec3f(centerpos + Vec3d(radius, radius, radius))
		);
}

RaySphere::~RaySphere()
{
}

	//returns neg num if object not hit by the ray

//NOTE: ignoring max_t for now.
double RaySphere::traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.0, 0.0);

	const Vec3d center_to_raystart = ray.startPos() - centerpos;

//http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter1.htm
	/*const double B = 2.0 * (
		ray.unitdir.x * (ray.startpos.x - centerpos.x) + 
		ray.unitdir.y * (ray.startpos.y - centerpos.y) + 
		ray.unitdir.z * (ray.startpos.z - centerpos.z)
		);*/

	const double B = 2.0 * dot(center_to_raystart, ray.unitDir());

	const double C = center_to_raystart.length2() - radius_squared;

	const double discriminant = B*B - 4.0*C;

	if(discriminant < 0.0)
		return -1.0;//no intersection

	const double sqrt_discriminant = sqrt(discriminant);
	const double t0 = (-B - sqrt_discriminant) * 0.5;
	if(t0 > 0.0)
		return t0;
	else
		return (-B + sqrt_discriminant) * 0.5;

	
	/*const Vec3d raystarttosphere = this->centerpos - ray.startpos;

	const double dist_to_rayclosest = dotProduct(raystarttosphere, ray.unitdir);


	//-----------------------------------------------------------------
	//check if center of sphere lies behind ray startpos (in dir of ray)
	//-----------------------------------------------------------------
//	if(dist_to_rayclosest < 0.0f)//will think of rays as having infinite length || q_closest > ray.length)
//		return -666.0f;


	const double sph_cen_to_ray_closest_len2 = raystarttosphere.length2() - 
		dist_to_rayclosest*dist_to_rayclosest;

	//-----------------------------------------------------------------
	//ray has missed sphere?
	//-----------------------------------------------------------------
	if(sph_cen_to_ray_closest_len2 > this->radius_squared)
		return -666.0;


	//ray has hit sphere...
	
	
	//return dist_to_rayclosest - sqrt(this->radius_squared - sph_cen_to_ray_closest_len2);
	const double a = sqrt(this->radius_squared - sph_cen_to_ray_closest_len2);
	double dist = dist_to_rayclosest - a;

	if(dist < 0.0f)
		return dist_to_rayclosest + a;
	else
		return dist;*/
}

bool RaySphere::doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	HitInfo hitinfo;
	const double hitdist = traceRay(ray, raylength, context, hitinfo);
	
	return hitdist >= 0.0f && hitdist < raylength;
}

/*void RaySphere::traceBundle(const RayBundle& raybundle, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfo_out)
{

}*/


/*float RaySphere::getDistanceUntilExits(const Ray& ray, HitInfo& hitinfo_out) const
{
	const Vec3 raystarttosphere = this->centerpos - ray.startpos;

	const float dist_to_rayclosest = dotProduct(raystarttosphere, ray.unitdir);

	//-----------------------------------------------------------------
	//check if center of sphere lies behind ray startpos (in dir of ray)
	//-----------------------------------------------------------------
	//if(dist_to_rayclosest < 0.0f)//will think of rays as having infinite length || q_closest > ray.length)
	//	return -666.0f;

	const float sph_cen_to_ray_closest_len2 = raystarttosphere.length2() - 
		dist_to_rayclosest*dist_to_rayclosest;

	//-----------------------------------------------------------------
	//ray has missed sphere?
	//-----------------------------------------------------------------
//	if(sph_cen_to_ray_closest_len2 > this->radius_squared)
//		return -666.0f;

	//ray has hit sphere...
	return dist_to_rayclosest + sqrt(this->radius_squared - sph_cen_to_ray_closest_len2);
}*/



const Vec3d RaySphere::getShadingNormal(const FullHitInfo& hitinfo) const 
{ 
	//assert(::epsEqual(point.getDist(centerpos), this->radius, 0.01f));

	//return (point - centerpos) * recip_radius;

	return getGeometricNormal(hitinfo);
}

const Vec3d RaySphere::getGeometricNormal(const FullHitInfo& hitinfo) const 
{ 
	//Vec3 normal = hitinfo.hitpos - centerpos;
	//normal.normalise();
	//return normal;
	return normalise(hitinfo.hitpos - centerpos);
}

//TODO: test
void RaySphere::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);

	const Vec3d raystarttosphere = centerpos - ray.startPos();

	const double dist_to_rayclosest = dotProduct(raystarttosphere, ray.unitDir());

	const double sph_cen_to_ray_closest_len2 = raystarttosphere.length2() - 
		dist_to_rayclosest*dist_to_rayclosest;

	//-----------------------------------------------------------------
	//ray has missed sphere?
	//-----------------------------------------------------------------
	if(sph_cen_to_ray_closest_len2 > this->radius_squared)
		return;

	//ray has hit sphere...
	
	
	//return dist_to_rayclosest - sqrt(this->radius_squared - sph_cen_to_ray_closest_len2);
	const double a = sqrt(this->radius_squared - sph_cen_to_ray_closest_len2);
	
	if(dist_to_rayclosest + a > 0.0)
	{
		hitinfos_out.push_back(FullHitInfo());
		hitinfos_out.back().dist = dist_to_rayclosest + a;
		hitinfos_out.back().hitpos = ray.startPos();
		hitinfos_out.back().hitpos.addMult(ray.unitDir(), hitinfos_out.back().dist);
	}

	if(dist_to_rayclosest - a > 0.0)
	{
		hitinfos_out.push_back(FullHitInfo());
		hitinfos_out.back().dist = dist_to_rayclosest - a;
		hitinfos_out.back().hitpos = ray.startPos();
		hitinfos_out.back().hitpos.addMult(ray.unitDir(), hitinfos_out.back().dist);
	}

}

const Vec2d RaySphere::getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const
{
	assert(hitinfo.geometric_normal.isUnitLength());

	//use the normal
	//float theta = asin(hitinfo.geometric_normal.y);
	const double theta = atan2(hitinfo.geometric_normal.y, hitinfo.geometric_normal.x);
	//if(hitinfo.geometric_normal.x < 0.0f)
	//	theta = NICKMATHS_PI - theta;
	const double phi = acos(hitinfo.geometric_normal.z);
	return Vec2d(theta * NICKMATHS_RECIP_2PI, phi * -NICKMATHS_RECIP_PI);
}



//returns true if could construct a suitable basis
bool RaySphere::getTangents(const FullHitInfo& hitinfo, unsigned int texcoords_set, Vec3d& tangent_out, Vec3d& bitangent_out) const
{
	//const Vec3 p = (hitinfo.hitpos - this->centerpos) * recip_radius;//normalised

	//const float r2_minus_z2 = radius_squared - p.z*p.z;

	//tangent_out = Vec3(-p.y, p.x, 0.0f) * r2_minus_z2 * NICKMATHS_RECIP_2PI;

	//bitangent_out.set(0.0f, 0.0f, NICKMATHS_RECIP_PI / sqrt(1.0f - p.z*p.z));

		//tangent_out = Vec3(-p.y, p.x, 0.0f) * r2_minus_z2 * NICKMATHS_RECIP_2PI;

	//bitangent_out.set(0.0f, 0.0f, NICKMATHS_RECIP_PI / sqrt(1.0f - p.z*p.z));

	const double theta = atan2(hitinfo.original_geometric_normal.y, hitinfo.original_geometric_normal.x);
	const double phi = acos(hitinfo.original_geometric_normal.z);


	//(dx/du, dy/du, dz/du)
	//tangent_out = Vec3(-sin(theta)*sin(phi), cos(theta)*sin(phi), 0.0f);

	tangent_out = Vec3d(-sin(theta)*sin(phi), cos(theta)*sin(phi), 0.0f) * NICKMATHS_2PI * radius;

	//(dx/dv, dy/dv, dz/dv)
	//bitangent_out = Vec3(cos(theta)*cos(phi), sin(theta)*cos(phi), -sin(phi));
	bitangent_out = Vec3d(-cos(theta)*cos(phi), -sin(theta)*cos(phi), sin(phi)) * NICKMATHS_PI * radius;

	return true;
}

void RaySphere::emitterInit()
{
	::fatalError("Spheres may not be emitters.");
}
const Vec3d RaySphere::sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										 HitInfo& hitinfo_out) const
{
	assert(0);
	return Vec3d(0.f, 0.f, 0.f);
}
double RaySphere::surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const
{
	::fatalError("RaySphere::surfacePDF");
	return 0.f;
}
double RaySphere::surfaceArea(const Matrix3d& to_parent) const
{
	::fatalError("RaySphere::surfaceArea");
	return 4.0 * NICKMATHS_PI * radius*radius;
}











void RaySphere::test()
{
	conPrint("RaySphere::test()");

	const SSE_ALIGN Ray ray(Vec3d(-1,0,1), Vec3d(1,0,0));

	RaySphere sphere(Vec3d(0,0,1), 0.5);

	js::TriTreePerThreadData tree_context;

	//------------------------------------------------------------------------
	//test traceRay()
	//------------------------------------------------------------------------
	HitInfo hitinfo;
	double d = sphere.traceRay(ray, 1000.0, tree_context, hitinfo);

	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.hittriindex == 0);
	//testAssert(hitinfo.hittricoords

	


	//------------------------------------------------------------------------
	//test traceRay() in reverse direction
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray2(Vec3d(1,0,1), Vec3d(-1,0,0));
	d = sphere.traceRay(ray2, 1000.0, tree_context, hitinfo);
	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.hittriindex == 0);

	d = sphere.traceRay(ray2, 0.1, tree_context, hitinfo);
	//ignoring this for now: testAssert(d < 0.0);


	//------------------------------------------------------------------------
	//test getAllHits()
	//------------------------------------------------------------------------
#ifndef COMPILER_GCC
	std::vector<FullHitInfo> hitinfos;
	sphere.getAllHits(ray, tree_context, hitinfos);
	std::sort(hitinfos.begin(), hitinfos.end(), FullHitInfoComparisonPred);

	testAssert(hitinfos.size() == 2);

	testAssert(epsEqual(hitinfos[0].dist, 0.5));
	//testAssert(epsEqual(hitinfos[0].geometric_normal, Vec3d(-1,0,0)));
	testAssert(epsEqual(hitinfos[0].hitpos, Vec3d(-0.5, 0, 1)));

	testAssert(epsEqual(hitinfos[1].dist, 1.5));
	//testAssert(epsEqual(hitinfos[1].geometric_normal, Vec3d(1,0,0)));
	testAssert(epsEqual(hitinfos[1].hitpos, Vec3d(0.5, 0, 1)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[0]), Vec3d(-1,0,0)));
	testAssert(epsEqual(sphere.getShadingNormal(hitinfos[0]), Vec3d(-1,0,0)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[1]), Vec3d(1,0,0)));
	testAssert(epsEqual(sphere.getShadingNormal(hitinfos[1]), Vec3d(1,0,0)));


	//------------------------------------------------------------------------
	//test doesFiniteRayHit()
	//------------------------------------------------------------------------
	testAssert(!sphere.doesFiniteRayHit(ray, 0.49, tree_context));
	testAssert(sphere.doesFiniteRayHit(ray, 0.51, tree_context));

	//------------------------------------------------------------------------
	//try tracing from inside sphere
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray3(Vec3d(0.25,0,1), Vec3d(1,0,0));
	d = sphere.traceRay(ray3, 1000.0, tree_context, hitinfo);
	testAssert(::epsEqual(d, 0.25));

	d = sphere.traceRay(ray3, 0.24, tree_context, hitinfo);
	//NOTE: ignoring this for now.  testAssert(d < 0.0);

	//------------------------------------------------------------------------
	//try getAllHits() from inside sphere
	//------------------------------------------------------------------------
	sphere.getAllHits(ray3, tree_context, hitinfos);

	testAssert(hitinfos.size() == 1);
	testAssert(epsEqual(hitinfos[0].dist, 0.25));
	testAssert(epsEqual(hitinfos[0].hitpos, Vec3d(0.5, 0, 1)));


#endif

	//TODO: test normal stuff

	//------------------------------------------------------------------------
	//test emitter  stuff
	//------------------------------------------------------------------------
	/*sphere.emitterInit();

	//4*pi*r^2 
	//4*pi*(0.5)^2
	//pi
	testAssert(epsEqual(sphere.surfaceArea(), NICKMATHS_PI));
	testAssert(epsEqual(sphere.surfacePDF(), 1.0 / NICKMATHS_PI));*/

}


