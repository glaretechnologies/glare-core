/*=====================================================================
geometry.h
----------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Wed Apr 14 21:19:37 2004
=====================================================================*/
#pragma once


#include "../indigo/UVCoordEvaluator.h"
#include "../indigo/SampleTypes.h"
#include "../raytracing/hitinfo.h"
#include "../physics/jscol_aabbox.h"
#include "../maths/vec2.h"
#include "../maths/plane.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <vector>
class Ray;
class World;
class HitInfo;
class FullHitInfo;
class DistanceHitInfo;
class Object;
class RendererSettings;
class ThreadContext;
class PrintOutput;
class Matrix4f;
namespace Indigo { class TaskManager; }


class GeometryExcep
{
public:
	GeometryExcep(const std::string& text_) : text(text_) {}
	~GeometryExcep(){}

	const std::string& what() const { return text; }
private:
	std::string text;
};


/*=====================================================================
Geometry
--------
Interface that represents the shape of an object
=====================================================================*/
SSE_CLASS_ALIGN Geometry : public RefCounted, public UVCoordEvaluator
{
public:
	Geometry(bool sub_elements_curved_) : object_usage_count(0), sub_elements_curved(sub_elements_curved_) {}
	virtual ~Geometry(){}


	typedef Vec4f Pos3Type;
	typedef float Vec3RealType;
	typedef float Real;
	typedef double DistType;
	typedef Vec4f Vec3Type;
	

	virtual const std::string getName() const = 0;

	virtual const js::AABBox getAABBoxWS() const = 0;

	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const = 0;
	
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;

	// Returns a vector orthogonal to the surface, with length equal to one over the probability density of sampling the given point on the sub-element.
	// This is equal to the area in object space of the surface sub-element for uniformly sampled sub-elements (e.g. triangles).
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const = 0;

	// Returns the coordinates (u, v) for the given uv-set, given the intrinsic coordinates (alpha, beta) in hitinfo.
	virtual const UVCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const = 0;
	virtual unsigned int getNumUVCoordSets() const = 0;
	

	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const = 0;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out, Vec2f& uv0_out) const = 0;
	
	// Get the partial derivatives of the surface position relative to u and v: dp/du and dp/dv
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const = 0;
	
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const { return 0; }

	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const = 0;
	
	// Sample the surface of the given sub-element.
	// normal_os_out will be set to a vector orthogonal to the surface, with length equal to one over the probability density of sampling the given point on the sub-element.
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_os_out, Vec3Type& normal_os_out, HitInfo& hitinfo_out, 
		unsigned int& mat_index_out, Vec2f& uv0_out) const = 0;

	
	class SampleResults
	{
	public:
		Pos3Type pos;
		Vec3Type N_g;
		HitInfo hitinfo;
		Real pd;
	};
	virtual void sampleSurface(const SamplePair& samples, SampleResults& results_out) const { assert(0); }

	// Returns true if possibly clipped by section planes, false otherwise.
	virtual bool subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes_os, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose
		) = 0; // throws GeometryExcep

	virtual void build(const std::string& cache_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) = 0; // throws GeometryExcep

	virtual Vec3RealType getBoundingRadius() const = 0;

	virtual Real meanCurvature(const HitInfo& hitinfo) const = 0;

	void incrementObjectUsageCount() { object_usage_count++; }
	unsigned int getObjectUsageCount() const { return object_usage_count; }

	inline bool areSubElementsCurved() const { return sub_elements_curved; } // For testing for self intersections.  Can a ray launched from a sub-element hit the same sub-element at a decent distance?

private:
	unsigned int object_usage_count; // Number of objects that use this geometry.
	bool sub_elements_curved;
};


typedef Reference<Geometry> GeometryRef;
