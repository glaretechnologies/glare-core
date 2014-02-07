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


RaySphere::RaySphere(const Vec4f& centre_, double radius_)
:	centre(centre_)
{
	radius = (Real)radius_;
	radius_squared = (Real)(radius_ * radius_);
	recip_radius = (Real)(1 / radius_);

	aabbox.min_ = centre + Vec4f(-(float)radius_, -(float)radius_, -(float)radius_, 0.f);
	aabbox.max_ = centre + Vec4f((float)radius_, (float)radius_, (float)radius_, 0.f);
}


RaySphere::~RaySphere()
{
}


Geometry::DistType RaySphere::traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const
{
	// We are using a numerically robust ray-sphere intersection algorithm as described here: http://www.cg.tuwien.ac.at/courses/CG1/textblaetter/englisch/10%20Ray%20Tracing%20(engl).pdf

	const Vec4f raystart_to_centre = centre - ray.startPos();

	const Real u_dot_del_p = dot(raystart_to_centre, ray.unitDir());

	const Real discriminant = radius_squared - raystart_to_centre.getDist2(ray.unitDir() * u_dot_del_p);

	if(discriminant < 0)
		return -1; // No intersection.

	const Real sqrt_discriminant = std::sqrt(discriminant);

	const Real use_min_t = rayMinT(radius);

	const Real t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	if(t_0 >= use_min_t && t_0 <= max_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_0) - centre, recip_radius);
		hitinfo_out.sub_elem_index = 0;
		hitinfo_out.sub_elem_coords = uvs;
		return t_0;
	}

	const Real t_1 = u_dot_del_p + sqrt_discriminant;
	if(t_1 >= use_min_t && t_1 <= max_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_1) - centre, recip_radius);
		hitinfo_out.sub_elem_index = 0;
		hitinfo_out.sub_elem_coords = uvs;
		return t_1;
	}

	return -1;
}


const RaySphere::Vec3Type RaySphere::getGeometricNormal(const HitInfo& hitinfo) const
{
	return GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
}


void RaySphere::getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_os_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_os_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	pos_os_out = centre + N_g_os_out * this->radius;
	pos_os_rel_error_out = std::numeric_limits<Real>::epsilon();
}


void RaySphere::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out, Real& curvature_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	N_s_os_out = N_g_os_out;
	mat_index_out = 0;
	pos_os_out = centre + N_g_os_out * this->radius;
	pos_os_rel_error_out = std::numeric_limits<Real>::epsilon();
	curvature_out = -recip_radius; // Mean curvature for a sphere is just the negative reciprocal radius of the sphere.
}


void RaySphere::getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);

	const Vec4f raystart_to_centre = centre - ray.startPos();

	const Real u_dot_del_p = dot(raystart_to_centre, ray.unitDir());

	const Real discriminant = radius_squared - raystart_to_centre.getDist2(ray.unitDir() * u_dot_del_p);

	if(discriminant < 0)
		return; // No intersection.

	const Real sqrt_discriminant = std::sqrt(discriminant);

	const Real use_min_t = rayMinT(radius);

	const Real t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	if(t_0 >= use_min_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_0) - centre, recip_radius);

		hitinfos_out.push_back(DistanceHitInfo(
			0, // sub elem index
			uvs,
			t_0 // dist
		));
	}

	const Real t_1 = u_dot_del_p + sqrt_discriminant;
	if(t_1 >= use_min_t)
	{
		const TexCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_1) - centre, recip_radius);
		
		hitinfos_out.push_back(DistanceHitInfo(
			0, // sub elem index
			uvs,
			t_1 // dist
		));
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


void RaySphere::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const
{
	//assert(to_parent == Matrix4f::identity());
	surface_areas_out.resize(1);
	surface_areas_out[0] = 4 * NICKMATHS_PIf * radius * radius;
}


void RaySphere::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, float sub_elem_area_ws, Real& p_out, unsigned int& mat_index_out) const
{
	assert(sub_elem_index == 0);
	const Vec4f n = GeometrySampling::uniformlySampleSphere(samples);
	normal_out.set(n.x[0], n.x[1], n.x[2], 0.0f);
	assert(normal_out.isUnitLength());
	pos_out = n * (float)radius;
	pos_out.x[3] = 1.0f;

	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords = GeometrySampling::sphericalCoordsForDir(n, (Vec3RealType)this->recip_radius);
	p_out = 1 / sub_elem_area_ws;
	mat_index_out = 0;
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


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"


void RaySphere::test()
{
	conPrint("RaySphere::test()");

	ThreadContext thread_context;

	// Test traceRay()
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray(
			Vec4f(-1,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f // min_t
		);
		
		HitInfo hitinfo;
		double d = sphere.traceRay(ray, 1000.0, thread_context, 
			hitinfo);

		testAssert(::epsEqual(d, 0.5));
		testAssert(hitinfo.sub_elem_index == 0);
	}

	// Test traceRay() in reverse direction
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray2(
			Vec4f(1,0,0,1),
			Vec4f(-1,0,0,0),
			1.0e-5f // min_t
		);

		HitInfo hitinfo;
		double d = sphere.traceRay(ray2, 1000.0, thread_context, hitinfo);
		testAssert(::epsEqual(d, 0.5));
		testAssert(hitinfo.sub_elem_index == 0);

		// Test with max dist = 0.1
		d = sphere.traceRay(ray2, 0.1, thread_context, hitinfo);
		testAssert(d < 0.0);
	}


	// Test getAllHits()
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray(
			Vec4f(-1,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f // min_t
		);
		std::vector<DistanceHitInfo> hitinfos;
		sphere.getAllHits(ray, thread_context, hitinfos);

		testAssert(hitinfos.size() == 2);

		testAssert(epsEqual(hitinfos[0].dist, 0.5f));
		testAssert(epsEqual(hitinfos[1].dist, 1.5f));

		testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[0]), Vec3Type(-1,0,0,0)));
		testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[1]), Vec3Type(1,0,0,0)));

		Vec3Type pos, N_g;
		Vec3RealType pos_error;
		sphere.getPosAndGeomNormal(hitinfos[0], pos, pos_error, N_g);

		testAssert(epsEqual(N_g, Vec3Type(-1,0,0,0)));

		sphere.getPosAndGeomNormal(hitinfos[1], pos, pos_error, N_g);

		testAssert(epsEqual(N_g, Vec3Type(1,0,0,0)));
	}


	// Try tracing from inside sphere
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray3(
			Vec4f(0.25f,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f // min_t
		);

		HitInfo hitinfo;
		double d = sphere.traceRay(ray3, 1000.0, thread_context, hitinfo);
		testAssert(::epsEqual(d, 0.25));

		d = sphere.traceRay(ray3, 0.24, thread_context, hitinfo);
		testAssert(d < 0.0);
	}

	// Try getAllHits() from inside sphere
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray3(
			Vec4f(0.25f,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f // min_t
		);
		std::vector<DistanceHitInfo> hitinfos;
		sphere.getAllHits(ray3, thread_context, hitinfos);

		testAssert(hitinfos.size() == 1);
		testAssert(epsEqual(hitinfos[0].dist, 0.25f));
	}
}


#endif // BUILD_TESTS
