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
#include "../indigo/DistanceHitInfo.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/TestUtils.h"
#include "../physics/jscol_TriTreePerThreadData.h"
#include <algorithm>
#include "../indigo/ThreadContext.h"
#include "../raytracing/matutils.h"
#include "../indigo/object.h"


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


	uvset_name_to_index["default"] = 0;
	uvset_name_to_index["albedo"] = 0;
	uvset_name_to_index["bump"] = 0;
}


RaySphere::~RaySphere()
{
}


//returns neg num if object not hit by the ray
//NOTE: ignoring max_t for now.
double RaySphere::traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	hitinfo_out.sub_elem_index = 0;
	//hitinfo_out.sub_elem_coords.set(0.0, 0.0);

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
	{
		const double t0 = (-B - sqrt_discriminant) * 0.5; // t0 is the smaller of the two solutions
		if(t0 > 0.0)
		{
			const TexCoordsType uvs = MatUtils::sphericalCoordsForDir<Vec3RealType>(toVec3f(ray.point(t0) - centerpos), recip_radius);
			if(!object || object->isNonNullAtHit(thread_context, ray, t0, 0, uvs.x, uvs.y))
			{
				hitinfo_out.sub_elem_coords = uvs;
				return t0;
			}
		}
	}

	{
		const double t = (-B + sqrt_discriminant) * 0.5;
		if(t > 0.0)
		{
			const TexCoordsType uvs = MatUtils::sphericalCoordsForDir<Vec3RealType>(toVec3f(ray.point(t) - centerpos), recip_radius);
			if(!object || object->isNonNullAtHit(thread_context, ray, t, 0, uvs.x, uvs.y))
			{
				hitinfo_out.sub_elem_coords = uvs;
				return t;
			}
		}
	}

	return -1.0;

	
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


bool RaySphere::doesFiniteRayHit(const Ray& ray, double raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const
{
	HitInfo hitinfo;
	const double hitdist = traceRay(ray, raylength, thread_context, context, object, hitinfo);
	
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



const RaySphere::Vec3Type RaySphere::getShadingNormal(const HitInfo& hitinfo) const 
{ 
	//assert(::epsEqual(point.getDist(centerpos), this->radius, 0.01f));

	//return (point - centerpos) * recip_radius;

	return getGeometricNormal(hitinfo);
}


const RaySphere::Vec3Type RaySphere::getGeometricNormal(const HitInfo& hitinfo) const 
{ 
	//Vec3 normal = hitinfo.hitpos - centerpos;
	//normal.normalise();
	//return normal;
	//return normalise(hitinfo.hitpos - centerpos);

	return MatUtils::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
}


//TODO: test
void RaySphere::getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
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
		const Vec3d hitpos = ray.point(dist_to_rayclosest + a);
		const TexCoordsType uvs = MatUtils::sphericalCoordsForDir<Vec3RealType>(toVec3f(hitpos - centerpos), recip_radius);

		if(!object || object->isNonNullAtHit(thread_context, ray, dist_to_rayclosest + a, 0, uvs.x, uvs.y))
		{
			/*hitinfos_out.push_back(DistanceFullHitInfo());
			hitinfos_out.back().dist = dist_to_rayclosest + a;
			hitinfos_out.back().hitpos = hitpos;
			hitinfos_out.back().hitinfo.sub_elem_index = 0;
			hitinfos_out.back().hitinfo.sub_elem_coords = uvs;*/
			hitinfos_out.push_back(DistanceHitInfo(
				0,
				uvs,
				dist_to_rayclosest + a
				));
		}
	}

	if(dist_to_rayclosest - a > 0.0)
	{
		const Vec3d hitpos = ray.point(dist_to_rayclosest - a);
		const TexCoordsType uvs = MatUtils::sphericalCoordsForDir<Vec3RealType>(toVec3f(hitpos - centerpos), recip_radius);

		if(!object || object->isNonNullAtHit(thread_context, ray, dist_to_rayclosest - a, 0, uvs.x, uvs.y))
		{
			/*hitinfos_out.push_back(DistanceFullHitInfo());
			hitinfos_out.back().dist = dist_to_rayclosest - a;
			hitinfos_out.back().hitpos = hitpos;
			hitinfos_out.back().hitinfo.sub_elem_index = 0;
			hitinfos_out.back().hitinfo.sub_elem_coords = uvs;*/
			hitinfos_out.push_back(DistanceHitInfo(
				0,
				uvs,
				dist_to_rayclosest - a
				));
		}
	}
}


const RaySphere::TexCoordsType RaySphere::getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	return hitinfo.sub_elem_coords;

	/*assert(hitinfo.geometric_normal.isUnitLength());

	//use the normal
	//float theta = asin(hitinfo.geometric_normal.y);
	const double theta = atan2(hitinfo.geometric_normal.y, hitinfo.geometric_normal.x);
	//if(hitinfo.geometric_normal.x < 0.0f)
	//	theta = NICKMATHS_PI - theta;
	const double phi = acos(hitinfo.geometric_normal.z);
	return Vec2d(theta * NICKMATHS_RECIP_2PI, phi * -NICKMATHS_RECIP_PI);*/
}


void RaySphere::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const
{
	const Vec3RealType theta = hitinfo.sub_elem_coords.x;
	const Vec3RealType phi = hitinfo.sub_elem_coords.y;

	//const double theta = atan2(hitinfo.original_geometric_normal.y, hitinfo.original_geometric_normal.x);
	//const double phi = acos(hitinfo.original_geometric_normal.z);

	//(dx/du, dy/du, dz/du)
	dp_du_out = Vec3Type(-sin(theta)*sin(phi), cos(theta)*sin(phi), 0.0f) * NICKMATHS_2PI * radius;

	//(dx/dv, dy/dv, dz/dv)
	dp_dv_out = Vec3Type(-cos(theta)*cos(phi), -sin(theta)*cos(phi), sin(phi)) * NICKMATHS_PI * radius;

	//TEMP HACK:
	dNs_du_out = dNs_dv_out = Vec3Type(0.0);
}


void RaySphere::getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const
{
	// Tex coords must be the UVs for spheres.
	ds_du_out = dt_dv_out = (TexCoordsRealType)1.0;

	ds_dv_out = dt_du_out = 0.0;
}


//returns true if could construct a suitable basis
/*
bool RaySphere::getTangents(const HitInfo& hitinfo, unsigned int texcoords_set, Vec3d& tangent_out, Vec3d& bitangent_out) const
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
}*/

/*void RaySphere::emitterInit()
{
	::fatalError("Spheres may not be emitters.");
}
const Vec3d RaySphere::sampleSurface(const SamplePair& samples, const Vec3d& viewer_point, Vec3d& normal_out,
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
}*/



const js::AABBox& RaySphere::getAABBoxWS() const { return aabbox; }


void RaySphere::getSubElementSurfaceAreas(const Matrix3<Vec3RealType>& to_parent, std::vector<double>& surface_areas_out) const
{
	assert(0);
}


void RaySphere::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Vec3d& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	assert(0);
}


double RaySphere::subElementSamplingPDF(unsigned int sub_elem_index, const Vec3d& pos, double sub_elem_area_ws) const
{
	return 1.0 / sub_elem_area_ws;
}


//int RaySphere::UVSetIndexForName(const std::string& uvset_name) const { return 0; }

	
const std::string RaySphere::getName() const { return "RaySphere"; }


void RaySphere::subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, 
		const std::vector<Plane<double> >& camera_clip_planes){}


void RaySphere::build(const std::string& indigo_base_dir_path, const RendererSettings& settings) {} // throws GeometryExcep


unsigned int RaySphere::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


unsigned int RaySphere::getNumTexCoordSets() const { return 1; }


void RaySphere::test()
{
	conPrint("RaySphere::test()");

	const SSE_ALIGN Ray ray(Vec3d(-1,0,1), Vec3d(1,0,0));

	RaySphere sphere(Vec3d(0,0,1), 0.5);

	js::ObjectTreePerThreadData context(true);
	ThreadContext thread_context(1, 0);

	//------------------------------------------------------------------------
	//test traceRay()
	//------------------------------------------------------------------------
	HitInfo hitinfo;
	double d = sphere.traceRay(ray, 1000.0, thread_context, context, NULL, hitinfo);

	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.sub_elem_index == 0);
	//testAssert(hitinfo.hittricoords

	


	//------------------------------------------------------------------------
	//test traceRay() in reverse direction
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray2(Vec3d(1,0,1), Vec3d(-1,0,0));
	d = sphere.traceRay(ray2, 1000.0, thread_context, context, NULL, hitinfo);
	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.sub_elem_index == 0);

	d = sphere.traceRay(ray2, 0.1, thread_context, context, NULL, hitinfo);
	//ignoring this for now: testAssert(d < 0.0);


	//------------------------------------------------------------------------
	//test getAllHits()
	//------------------------------------------------------------------------
#ifndef COMPILER_GCC
	std::vector<DistanceHitInfo> hitinfos;
	sphere.getAllHits(ray, thread_context, context, NULL, hitinfos);
	std::sort(hitinfos.begin(), hitinfos.end(), distanceHitInfoComparisonPred);

	testAssert(hitinfos.size() == 2);

	testAssert(epsEqual(hitinfos[0].dist, 0.5));
	//testAssert(epsEqual(hitinfos[0].geometric_normal, Vec3d(-1,0,0)));
	//testAssert(epsEqual(hitinfos[0].hitpos, Vec3d(-0.5, 0, 1)));

	testAssert(epsEqual(hitinfos[1].dist, 1.5));
	//testAssert(epsEqual(hitinfos[1].geometric_normal, Vec3d(1,0,0)));
	//testAssert(epsEqual(hitinfos[1].hitpos, Vec3d(0.5, 0, 1)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[0]), Vec3Type(-1,0,0)));
	testAssert(epsEqual(sphere.getShadingNormal(hitinfos[0]), Vec3Type(-1,0,0)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[1]), Vec3Type(1,0,0)));
	testAssert(epsEqual(sphere.getShadingNormal(hitinfos[1]), Vec3Type(1,0,0)));


	//------------------------------------------------------------------------
	//test doesFiniteRayHit()
	//------------------------------------------------------------------------
	testAssert(!sphere.doesFiniteRayHit(ray, 0.49, thread_context, context, NULL));
	testAssert(sphere.doesFiniteRayHit(ray, 0.51, thread_context, context, NULL));

	//------------------------------------------------------------------------
	//try tracing from inside sphere
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray3(Vec3d(0.25,0,1), Vec3d(1,0,0));
	d = sphere.traceRay(ray3, 1000.0, thread_context, context, NULL, hitinfo);
	testAssert(::epsEqual(d, 0.25));

	d = sphere.traceRay(ray3, 0.24, thread_context, context, NULL, hitinfo);
	//NOTE: ignoring this for now.  testAssert(d < 0.0);

	//------------------------------------------------------------------------
	//try getAllHits() from inside sphere
	//------------------------------------------------------------------------
	sphere.getAllHits(ray3, thread_context, context, NULL, hitinfos);

	testAssert(hitinfos.size() == 1);
	testAssert(epsEqual(hitinfos[0].dist, 0.25));
	//testAssert(epsEqual(hitinfos[0].hitpos, Vec3d(0.5, 0, 1)));


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


