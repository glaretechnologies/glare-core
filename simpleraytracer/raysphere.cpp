/*=====================================================================
raysphere.cpp
-------------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#include "raysphere.h"


#include "ray.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/DistanceHitInfo.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/ThreadContext.h"
#include "../maths/GeometrySampling.h"
#include "../indigo/object.h"


RaySphere::RaySphere(double radius_)
{
	radius = (Real)radius_;
	radius_squared = (Real)(radius_ * radius_);
	recip_radius = (Real)(1 / radius_);

	aabbox.min_ = Vec4f(-(float)radius_, -(float)radius_, -(float)radius_, 1.f);
	aabbox.max_ = Vec4f((float)radius_, (float)radius_, (float)radius_, 1.f);
}


RaySphere::~RaySphere()
{
}


Geometry::DistType RaySphere::traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, HitInfo& hitinfo_out) const
{
	// We are using a numerically robust ray-sphere intersection algorithm as described here: http://www.cg.tuwien.ac.at/courses/CG1/textblaetter/englisch/10%20Ray%20Tracing%20(engl).pdf
	// Sphere origin is at (0,0,0,1).

	const Vec4f raystart_to_centre = Vec4f(0,0,0,1) - ray.startPos();

	const Real u_dot_del_p = dot(raystart_to_centre, ray.unitDir());

	const Real discriminant = radius_squared - raystart_to_centre.getDist2(ray.unitDir() * u_dot_del_p);

	if(discriminant < 0)
		return -1; // No intersection.

	const Real sqrt_discriminant = std::sqrt(discriminant);

	const Real use_min_t = rayMinT(radius);

	const Real t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	if(t_0 >= use_min_t && t_0 <= max_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_0) - Vec4f(0,0,0,1), recip_radius);
		if(!object || object->isNonNullAtHit(thread_context, ray, t_0, 0, uvs.x, uvs.y))
		{
			hitinfo_out.sub_elem_index = 0;
			hitinfo_out.sub_elem_coords = uvs;
			return t_0;
		}
	}

	const Real t_1 = u_dot_del_p + sqrt_discriminant;
	if(t_1 >= use_min_t && t_1 <= max_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_1) - Vec4f(0,0,0,1.f), recip_radius);
		if(!object || object->isNonNullAtHit(thread_context, ray, t_1, 0, uvs.x, uvs.y))
		{
			hitinfo_out.sub_elem_index = 0;
			hitinfo_out.sub_elem_coords = uvs;
			return t_1;
		}
	}

	return -1;
}


bool RaySphere::doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const
{
	HitInfo hitinfo;
	const double hitdist = traceRay(ray, raylength, thread_context, 
		object, hitinfo);

	return hitdist >= rayMinT(radius) && hitdist < raylength;
}


const RaySphere::Vec3Type RaySphere::getGeometricNormal(const HitInfo& hitinfo) const
{
	return GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
}


void RaySphere::getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_os_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_os_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	pos_os_out = Vec3Type(0,0,0,1) + N_g_os_out * this->radius;
	pos_os_rel_error_out = std::numeric_limits<Real>::epsilon();
}


void RaySphere::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out, Real& curvature_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	N_s_os_out = N_g_os_out;
	mat_index_out = 0;
	pos_os_out = Vec3Type(0,0,0,1) + N_g_os_out * this->radius;
	pos_os_rel_error_out = std::numeric_limits<Real>::epsilon();
	curvature_out = -recip_radius; // Mean curvature for a sphere is just the negative reciprocal radius of the sphere.
}


//TODO: improve this to use the numerically robust intersection algorithm
void RaySphere::getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);

	const Vec3d raystarttosphere = toVec3d(ray.startPos()) * -1.0f;

	const double dist_to_rayclosest = dot(raystarttosphere, toVec3d(ray.unitDir()));

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

	const double use_min_t = rayMinT(radius);

	if(dist_to_rayclosest + a >= use_min_t/*ray.minT()*/)
	{
		const Vec3d hitpos = toVec3d(ray.pointf(dist_to_rayclosest + a));
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir<Vec3RealType>(toVec3f(hitpos), (Vec3RealType)recip_radius);

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

	if(dist_to_rayclosest - a >= use_min_t/*ray.minT()*/)
	{
		const Vec3d hitpos = toVec3d(ray.pointf(dist_to_rayclosest - a));
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir<Vec3RealType>(toVec3f(hitpos), (Vec3RealType)recip_radius);

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


const RaySphere::TexCoordsType RaySphere::getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	return hitinfo.sub_elem_coords;
}


void RaySphere::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const
{
	const Vec3RealType theta = hitinfo.sub_elem_coords.x;
	const Vec3RealType phi = hitinfo.sub_elem_coords.y;

	//(dx/du, dy/du, dz/du)
	dp_du_out = Vec3Type(-sin(theta)*sin(phi), cos(theta)*sin(phi), 0.0f, 0.0f) * ((Vec3RealType)NICKMATHS_2PI * (Vec3RealType)radius);

	//(dx/dv, dy/dv, dz/dv)
	dp_dv_out = Vec3Type(-cos(theta)*cos(phi), -sin(theta)*cos(phi), sin(phi), 0.0f) * ((Vec3RealType)NICKMATHS_PI * (Vec3RealType)radius);

	//TEMP HACK:
	dNs_du_out = dNs_dv_out = Vec3Type(0.0);
}


void RaySphere::getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, 
								   TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const
{
	// (alpha, beta) -> (u, v) is the identity mapping
	du_dalpha_out = dv_dbeta_out = 1;

	du_dbeta_out = dv_dalpha_out = 0;
}


const js::AABBox& RaySphere::getAABBoxWS() const { return aabbox; }


void RaySphere::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const
{
	assert(to_parent == Matrix4f::identity());
	surface_areas_out.resize(1);
	surface_areas_out[0] = 4.0 * NICKMATHS_PI * radius * radius;
}


void RaySphere::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	assert(sub_elem_index == 0);
	const Vec4f n = GeometrySampling::uniformlySampleSphere(samples);
	normal_out.set(n.x[0], n.x[1], n.x[2], 0.0f);
	assert(normal_out.isUnitLength());
	pos_out = n * (float)radius;
	pos_out.x[3] = 1.0f;

	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords = GeometrySampling::sphericalCoordsForDir(n, (Vec3RealType)this->recip_radius);
}


double RaySphere::subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const
{
	return 1 / sub_elem_area_ws;
}


const std::string RaySphere::getName() const { return "RaySphere"; }


bool RaySphere::subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, /*const CoordFramed& camera_coordframe_os, */ double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose)
{
	return false; // Spheres can't be clipped currently
}


void RaySphere::build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) {} // throws GeometryExcep


unsigned int RaySphere::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


unsigned int RaySphere::getNumUVCoordSets() const { return 1; }


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include <algorithm>


void RaySphere::test()
{
	conPrint("RaySphere::test()");

	const SSE_ALIGN Ray ray(
		Vec4f(-1,0,0,1),
		Vec4f(1,0,0,0),
		1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
		, Vec4f(1,0,0,0) // launch normal
#endif
		);

	RaySphere sphere(/*Vec3d(0,0,1), */0.5);

	//js::ObjectTreePerThreadData context;//(true);
	ThreadContext thread_context;

	//------------------------------------------------------------------------
	//test traceRay()
	//------------------------------------------------------------------------
	HitInfo hitinfo;
	double d = sphere.traceRay(ray, 1000.0, thread_context, 
		NULL, hitinfo);

	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.sub_elem_index == 0);
	//testAssert(hitinfo.hittricoords




	//------------------------------------------------------------------------
	//test traceRay() in reverse direction
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray2(
		Vec4f(1,0,0,1),
		Vec4f(-1,0,0,0),
		1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
		, Vec4f(-1,0,0,0) // launch normal
#endif
		);
	d = sphere.traceRay(ray2, 1000.0, thread_context, 
		NULL, hitinfo);
	testAssert(::epsEqual(d, 0.5));
	testAssert(hitinfo.sub_elem_index == 0);

	d = sphere.traceRay(ray2, 0.1, thread_context, 
		NULL, hitinfo);
	//ignoring this for now: testAssert(d < 0.0);


	//------------------------------------------------------------------------
	//test getAllHits()
	//------------------------------------------------------------------------
#ifndef COMPILER_GCC
	std::vector<DistanceHitInfo> hitinfos;
	sphere.getAllHits(ray, thread_context, 
		//context, 
		NULL, hitinfos);
	std::sort(hitinfos.begin(), hitinfos.end(), distanceHitInfoComparisonPred);

	testAssert(hitinfos.size() == 2);

	testAssert(epsEqual(hitinfos[0].dist, 0.5f));
	//testAssert(epsEqual(hitinfos[0].geometric_normal, Vec3d(-1,0,0)));
	//testAssert(epsEqual(hitinfos[0].hitpos, Vec3d(-0.5, 0, 1)));

	testAssert(epsEqual(hitinfos[1].dist, 1.5f));
	//testAssert(epsEqual(hitinfos[1].geometric_normal, Vec3d(1,0,0)));
	//testAssert(epsEqual(hitinfos[1].hitpos, Vec3d(0.5, 0, 1)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[0]), Vec3Type(-1,0,0,0)));
	//testAssert(epsEqual(sphere.getShadingNormal(hitinfos[0]), Vec3Type(-1,0,0,0)));

	testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[1]), Vec3Type(1,0,0,0)));
	//testAssert(epsEqual(sphere.getShadingNormal(hitinfos[1]), Vec3Type(1,0,0,0)));

	Vec3Type pos, N_g;
	Vec3RealType pos_error;
	sphere.getPosAndGeomNormal(hitinfos[0], pos, pos_error, N_g);

	testAssert(epsEqual(N_g, Vec3Type(-1,0,0,0)));


	sphere.getPosAndGeomNormal(hitinfos[1], pos, pos_error, N_g);

	testAssert(epsEqual(N_g, Vec3Type(1,0,0,0)));


	//------------------------------------------------------------------------
	//test doesFiniteRayHit()
	//------------------------------------------------------------------------
	testAssert(!sphere.doesFiniteRayHit(ray, 0.49f, thread_context, NULL, std::numeric_limits<unsigned int>::max()));
	testAssert(sphere.doesFiniteRayHit(ray, 0.51f, thread_context, NULL, std::numeric_limits<unsigned int>::max()));

	//------------------------------------------------------------------------
	//try tracing from inside sphere
	//------------------------------------------------------------------------
	const SSE_ALIGN Ray ray3(
		Vec4f(0.25,0,0,1),
		Vec4f(1,0,0,0),
		1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
		, Vec4f(1,0,0,0) // launch normal
#endif
		);
	d = sphere.traceRay(ray3, 1000.0, thread_context, NULL, hitinfo);
	testAssert(::epsEqual(d, 0.25));

	d = sphere.traceRay(ray3, 0.24, thread_context, NULL, hitinfo);
	//NOTE: ignoring this for now.  testAssert(d < 0.0);

	//------------------------------------------------------------------------
	//try getAllHits() from inside sphere
	//------------------------------------------------------------------------
	sphere.getAllHits(ray3, thread_context/*, context*/, NULL, hitinfos);

	testAssert(hitinfos.size() == 1);
	testAssert(epsEqual(hitinfos[0].dist, 0.25f));
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
#endif


bool RaySphere::isEnvSphereGeometry() const
{
	return false;
}


bool RaySphere::areSubElementsCurved() const
{
	return true;
}


RaySphere::Vec3RealType RaySphere::getBoundingRadius() const
{
	return radius;
}
