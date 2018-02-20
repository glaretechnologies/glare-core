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
#include "../utils/Numeric.h"


RaySphere::RaySphere(const Vec4f& centre_, double radius_)
:	Geometry(
		true, // sub-elements curved
		false // is camera
	),
	centre(centre_)
{
	radius = (Real)radius_;
	radius_squared = (Real)(radius_ * radius_);
	recip_radius = (Real)(1 / radius_);

	aabbox.min_ = centre + Vec4f(-(float)radius_, -(float)radius_, -(float)radius_, 0.f);
	aabbox.max_ = centre + Vec4f((float)radius_, (float)radius_, (float)radius_, 0.f);

	area = (Real)(4 * Maths::pi<double>() * radius_*radius_);
}


RaySphere::~RaySphere()
{
}


Geometry::DistType RaySphere::traceRay(const Ray& ray, ThreadContext& thread_context, HitInfo& hitinfo_out) const
{
	// We are using a numerically robust ray-sphere intersection algorithm as described here: http://www.cg.tuwien.ac.at/courses/CG1/textblaetter/englisch/10%20Ray%20Tracing%20(engl).pdf

	const Vec4f raystart_to_centre = centre - ray.startPos();

	const Real u_dot_del_p = dot(raystart_to_centre, ray.unitDir());

	const Real discriminant = radius_squared - raystart_to_centre.getDist2(ray.unitDir() * u_dot_del_p);

	if(discriminant < 0)
		return -1; // No intersection.

	const Real sqrt_discriminant = std::sqrt(discriminant);

	//const Real use_min_t = rayMinT(radius);
	const Real use_min_t = this->radius * std::numeric_limits<float>::epsilon() * 50; //ray.minT(); // rayMinT(radius);

	const Real t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	if(t_0 >= use_min_t && t_0 <= ray.maxT())
	{
		const UVCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_0) - centre, recip_radius);
		hitinfo_out.sub_elem_index = 0;
		hitinfo_out.sub_elem_coords = uvs;
		return t_0;
	}

	const Real t_1 = u_dot_del_p + sqrt_discriminant;
	if(t_1 >= use_min_t && t_1 <= ray.maxT())
	{
		const UVCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_1) - centre, recip_radius);
		hitinfo_out.sub_elem_index = 0;
		hitinfo_out.sub_elem_coords = uvs;
		return t_1;
	}

	return -1;
}


const RaySphere::Vec3Type RaySphere::getGeometricNormalAndMatIndex(const HitInfo& hitinfo, unsigned int& mat_index_out) const
{
	mat_index_out = 0;
	return GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y) * area;
}


void RaySphere::getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_os_out, Vec3RealType& pos_os_abs_error_out, Vec3Type& N_g_os_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	pos_os_out = centre + N_g_os_out * this->radius;
	pos_os_abs_error_out = std::numeric_limits<Real>::epsilon() * 5 * Numeric::L1Norm(pos_os_out);
}


void RaySphere::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_abs_error_out, Vec2f& uv0_out) const
{
	N_g_os_out = GeometrySampling::dirForSphericalCoords(hitinfo.sub_elem_coords.x, hitinfo.sub_elem_coords.y);
	N_s_os_out = N_g_os_out;
	mat_index_out = 0;
	pos_os_out = centre + N_g_os_out * this->radius;
	pos_os_abs_error_out = std::numeric_limits<Real>::epsilon() * 5 * Numeric::L1Norm(pos_os_out);
	uv0_out = hitinfo.sub_elem_coords;
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
		const UVCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_0) - centre, recip_radius);

		hitinfos_out.push_back(DistanceHitInfo(
			0, // sub elem index
			uvs,
			t_0 // dist
		));
	}

	const Real t_1 = u_dot_del_p + sqrt_discriminant;
	if(t_1 >= use_min_t)
	{
		const UVCoordsType uvs = GeometrySampling::sphericalCoordsForDir(ray.pointf(t_1) - centre, recip_radius);
		
		hitinfos_out.push_back(DistanceHitInfo(
			0, // sub elem index
			uvs,
			t_1 // dist
		));
	}
}


const RaySphere::UVCoordsType RaySphere::getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	return hitinfo.sub_elem_coords;
}


const RaySphere::UVCoordsType RaySphere::getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& duv_dalphabeta_out) const
{
	// (alpha, beta) -> (u, v) is the identity mapping
	duv_dalphabeta_out = Matrix2f::identity();

	return hitinfo.sub_elem_coords;
}


void RaySphere::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const
{
	const Vec3RealType phi = hitinfo.sub_elem_coords.x;
	const Vec3RealType theta = hitinfo.sub_elem_coords.y;

	//(dx/du, dy/du, dz/du)
	dp_du_out = Vec3Type(-sin(phi)*sin(theta), cos(phi)*sin(theta), 0.0f, 0.0f) * radius;

	//(dx/dv, dy/dv, dz/dv)
	dp_dv_out = Vec3Type(cos(phi)*cos(theta), sin(phi)*cos(theta), -sin(theta), 0.0f) * radius;
}


void RaySphere::getIntrinsicCoordsPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out) const
{
	const Vec3RealType phi = hitinfo.sub_elem_coords.x;
	const Vec3RealType theta = hitinfo.sub_elem_coords.y;

	//(dx/du, dy/du, dz/du)
	dp_dalpha_out = Vec3Type(-sin(phi)*sin(theta), cos(phi)*sin(theta), 0.0f, 0.0f) * radius;

	//(dx/dv, dy/dv, dz/dv)
	dp_dbeta_out = Vec3Type(cos(phi)*cos(theta), sin(phi)*cos(theta), -sin(theta), 0.0f) * radius;
}


const js::AABBox RaySphere::getAABBox() const { return aabbox; }


void RaySphere::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const
{
	//assert(to_parent == Matrix4f::identity());
	surface_areas_out.resize(1);
	surface_areas_out[0] = 4 * NICKMATHS_PIf * radius * radius;
}


void RaySphere::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, 
								 unsigned int& mat_index_out, Vec2f& uv0_out) const
{
	assert(sub_elem_index == 0);
	const float phi = samples.x * Maths::get2Pi<float>();
	const float z = 1 - samples.y * 2;
	const float r = sqrt(1 - z*z);
	const Vec4f n(cos(phi) * r, sin(phi) * r, z, 0.0f);

	assert(n.isUnitLength());

	normal_out = n * area;
	
	pos_out = centre + n * radius;

	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords = Vec2f(phi, std::acos(z));

	mat_index_out = 0;
	uv0_out = hitinfo_out.sub_elem_coords;
}


const std::string RaySphere::getName() const { return "RaySphere"; }


bool RaySphere::subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const ArrayRef<Reference<Material> >& materials,/*const Object& object, */const Matrix4f& object_to_camera, /*const CoordFramed& camera_coordframe_os, */ double pixel_height_at_dist_one,
	const std::vector<Plane<Vec3RealType> >& camera_clip_planes, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose,
	ShouldCancelCallback* should_cancel_callback)
{
	return false; // Spheres can't be clipped currently
}


void RaySphere::build(const std::string& indigo_base_dir_path, const BuildOptions& options, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) {} // throws GeometryExcep


unsigned int RaySphere::getMaterialIndexForTri(unsigned int tri_index) const { return 0; }


unsigned int RaySphere::getNumUVCoordSets() const { return 1; }


RaySphere::Real RaySphere::meanCurvature(const HitInfo& hitinfo) const
{
	return -recip_radius; // Mean curvature for a sphere is just the negative reciprocal radius of the sphere.
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"


void RaySphere::test()
{
	conPrint("RaySphere::test()");

	ThreadContext thread_context;

	// Test getGeometricNormalAndMatIndex()
	{
		const float radius = 0.5f;
		RaySphere sphere(Vec4f(0,0,0,1), radius);
		unsigned int mat_index;
		testEpsEqual(sphere.getGeometricNormalAndMatIndex(HitInfo(0, Vec2f(0, 0)), mat_index), Vec4f(0,0,4*Maths::pi<float>()*radius*radius,0)); // phi=0, theta=0 => straight up

		testEpsEqual(sphere.getGeometricNormalAndMatIndex(HitInfo(0, Vec2f(0, Maths::pi_2<float>())), mat_index), Vec4f(4*Maths::pi<float>()*radius*radius,0,0,0)); // phi=0, theta=pi/2
		testAssert(mat_index == 0);
	}


	// Test getPartialDerivs()
	{
		const float radius = 1.f;
		RaySphere sphere(Vec4f(0,0,0,1), radius);

		Vec4f dp_du, dp_dv;
		sphere.getPartialDerivs(HitInfo(0, Vec2f(0, Maths::pi_2<float>())), dp_du, dp_dv);
		testEpsEqual(dp_du, Vec4f(0,1,0,0));
		testEpsEqual(dp_dv, Vec4f(0,0,-1,0));
	}
	{
		const float radius = 0.5f;
		RaySphere sphere(Vec4f(0,0,0,1), radius);

		Vec4f dp_du, dp_dv;
		sphere.getPartialDerivs(HitInfo(0, Vec2f(0, Maths::pi_2<float>())), dp_du, dp_dv);
		testEpsEqual(dp_du, Vec4f(0,radius,0,0));
		testEpsEqual(dp_dv, Vec4f(0,0,-radius,0));
	}

	// Test sampleSubElement
	{
		const float radius = 0.5f;
		RaySphere sphere(Vec4f(1,1,1,1), radius);

		Vec4f pos_os, n_os;
		HitInfo hitinfo;
		unsigned int mat_index;
		Vec2f uv_0;

		sphere.sampleSubElement(0, SamplePair(0, 0.5f), pos_os, n_os, hitinfo, mat_index, uv_0); // Should map to phi=0, theta=pi/2
		testEpsEqual(pos_os, Vec4f(1,1,1,1) + Vec4f(radius,0,0,0));
		testEpsEqual(n_os, Vec4f(4*Maths::pi<float>()*radius*radius,0,0,0)); // length of normal should be equal to pdf.
		testEqual(mat_index, 0u);
		testEpsEqual(uv_0, Vec2f(0, Maths::pi_2<float>()));

		sphere.sampleSubElement(0, SamplePair(0, 0.0f), pos_os, n_os, hitinfo, mat_index, uv_0); // Should map to phi=0, theta=0
		testEpsEqual(pos_os, Vec4f(1,1,1,1) + Vec4f(0,0,radius,0));
		testEpsEqual(n_os, Vec4f(0,0,4*Maths::pi<float>()*radius*radius,0)); // length of normal should be equal to pdf.
		testEqual(mat_index, 0u);
		testEpsEqual(uv_0, Vec2f(0, 0));

		const float nearly_one = 0.99999994f;
		testAssert(nearly_one < 1);
		sphere.sampleSubElement(0, SamplePair(0, nearly_one), pos_os, n_os, hitinfo, mat_index, uv_0); // Should map to phi=0, theta=pi
		testAssert(epsEqual(pos_os, Vec4f(1,1,1,1) + Vec4f(0,0,-radius,0), 1.0e-3f));
		testAssert(epsEqual(n_os, Vec4f(0,0,-4*Maths::pi<float>()*radius*radius,0), 2e-3f)); // length of normal should be equal to pdf.
		testEqual(mat_index, 0u);
		testAssert(epsEqualWithEps(uv_0, Vec2f(0, Maths::pi<float>()), 1.0e-3f));
	}

	// Test traceRay()
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray(
			Vec4f(-1,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f, // min_t
			std::numeric_limits<float>::infinity() // max_t
		);
		
		HitInfo hitinfo;
		double d = sphere.traceRay(ray, thread_context, 
			hitinfo);

		testAssert(::epsEqual(d, 0.5));
		testAssert(hitinfo.sub_elem_index == 0);
	}

	// Test traceRay() in reverse direction
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		Ray ray2(
			Vec4f(1,0,0,1),
			Vec4f(-1,0,0,0),
			1.0e-5f, // min_t
			std::numeric_limits<float>::infinity() // max_t
		);

		HitInfo hitinfo;
		double d = sphere.traceRay(ray2, thread_context, hitinfo);
		testAssert(::epsEqual(d, 0.5));
		testAssert(hitinfo.sub_elem_index == 0);

		// Test with max dist = 0.1
		ray2.setMaxT(0.1);
		d = sphere.traceRay(ray2, thread_context, hitinfo);
		testAssert(d < 0.0);
	}


	// Test getAllHits()
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray(
			Vec4f(-1,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f, // min_t
			std::numeric_limits<float>::infinity() // max_t
		);
		std::vector<DistanceHitInfo> hitinfos;
		sphere.getAllHits(ray, thread_context, hitinfos);

		testAssert(hitinfos.size() == 2);

		testAssert(epsEqual(hitinfos[0].dist, 0.5f));
		testAssert(epsEqual(hitinfos[1].dist, 1.5f));

		//testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[0]), Vec3Type(-1,0,0,0)));
		//testAssert(epsEqual(sphere.getGeometricNormal(hitinfos[1]), Vec3Type(1,0,0,0)));

		Vec3Type pos, N_g;
		Vec3RealType pos_error;
		sphere.getPosAndGeomNormal(hitinfos[0], pos, pos_error, N_g);

		testAssert(epsEqual(N_g, Vec3Type(-1,0,0,0)));

		sphere.getPosAndGeomNormal(hitinfos[1], pos, pos_error, N_g);

		//testAssert(epsEqual(N_g, Vec3Type(1,0,0,0)));
	}


	// Try tracing from inside sphere
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		Ray ray3(
			Vec4f(0.25f,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f, // min_t
			std::numeric_limits<float>::infinity() // max_t
		);

		HitInfo hitinfo;
		double d = sphere.traceRay(ray3, thread_context, hitinfo);
		testAssert(::epsEqual(d, 0.25));

		ray3.setMaxT(0.24f);
		d = sphere.traceRay(ray3, thread_context, hitinfo);
		testAssert(d < 0.0);
	}

	// Try getAllHits() from inside sphere
	{
		RaySphere sphere(Vec4f(0,0,0,1), 0.5);

		const Ray ray3(
			Vec4f(0.25f,0,0,1),
			Vec4f(1,0,0,0),
			1.0e-5f, // min_t
			std::numeric_limits<float>::infinity() // max_t
		);
		std::vector<DistanceHitInfo> hitinfos;
		sphere.getAllHits(ray3, thread_context, hitinfos);

		testAssert(hitinfos.size() == 1);
		testAssert(epsEqual(hitinfos[0].dist, 0.25f));
	}
}


#endif // BUILD_TESTS
