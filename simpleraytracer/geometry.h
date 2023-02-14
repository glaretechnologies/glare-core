/*=====================================================================
geometry.h
----------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Wed Apr 14 21:19:37 2004
=====================================================================*/
#pragma once


#include "hitinfo.h"
#include "../indigo/UVCoordEvaluator.h"
#include "../indigo/SampleTypes.h"
#include "../physics/jscol_aabbox.h"
#include "../maths/vec2.h"
#include "../maths/plane.h"
#include "../utils/RefCounted.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/ArrayRef.h"
#include <vector>
class Ray;
class World;
class HitInfo;
class FullHitInfo;
class DistanceHitInfo;
class Object;
class PrintOutput;
class Matrix4f;
class Material;
class ShouldCancelCallback;
class TransformPath;
struct WorldParams;
namespace glare { class TaskManager; }

namespace EmbreeGlare {
	struct RTCSceneTy;
	struct RTCDeviceTy;
}

using namespace EmbreeGlare;


/*=====================================================================
Geometry
--------
Interface that represents the shape of an object
=====================================================================*/
class Geometry : public ThreadSafeRefCounted, public UVCoordEvaluator
{
public:
	enum GeometryType
	{
		GeometryType_DisplacedQuad,
		GeometryType_RectangleGeom,
		GeometryType_Camera,
		GeometryType_RayMesh,
		GeometryType_RaySphere
	};

	Geometry(GeometryType geom_type_, bool sub_elements_curved_) : object_usage_count(0), geom_type(geom_type_), sub_elements_curved(sub_elements_curved_) {}
	virtual ~Geometry(){}


	typedef Vec4f Pos3Type;
	typedef float Vec3RealType;
	typedef float Real;
	typedef double DistType;
	typedef Vec4f Vec3Type;
	

	virtual const std::string getName() const = 0;

	virtual const js::AABBox getAABBox() const = 0;

	virtual const js::AABBox getTightAABBoxWS(const TransformPath& transform_path) const { return getAABBox(); }

	virtual DistType traceRay(const Ray& ray, HitInfo& hitinfo_out) const = 0;
	
	// Returns a vector orthogonal to the surface, with length equal to one over the probability density of sampling the given point on the sub-element.
	// This is equal to the area in object space of the surface sub-element for uniformly sampled sub-elements (e.g. triangles).
	virtual const Vec3Type getGeometricNormalAndMatIndex(const HitInfo& hitinfo, unsigned int& mat_index_out) const = 0;

	// Returns the coordinates (u, v) for the given uv-set, given the intrinsic coordinates (alpha, beta) in hitinfo.
	virtual const UVCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const = 0;
	virtual const UVCoordsType getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& duv_dalphabeta_out) const = 0;
	virtual unsigned int getNumUVCoordSets() const = 0;
	

	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_abs_error_out, Vec3Type& N_g_out) const = 0;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_abs_error_out, Vec2f& uv0_out) const = 0;
	
	// Get the partial derivatives of the surface position relative to u and v: dp/du and dp/dv
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const = 0;

	virtual void getIntrinsicCoordsPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out/*, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out*/) const = 0;
	
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
	virtual bool subdivideAndDisplace(glare::TaskManager& task_manager, const ArrayRef<Reference<Material> >& materials, /*const Object& object, */const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
		const std::vector<Planef>& camera_clip_planes_os, const std::vector<Planef>& section_planes_os, const WorldParams& world_params, PrintOutput& print_output, bool verbose,
		ShouldCancelCallback* should_cancel_callback
		) = 0; // throws glare::Exception

#ifndef GEOMETRY_NO_TREE_BUILD_SUPPORT
	struct BuildOptions
	{
		BuildOptions() : build_small_bvh(false), compute_is_planar(true), embree_device(NULL) {}
		bool build_small_bvh;
		bool compute_is_planar; // If true, computes planar and planar_normal in RayMesh::build()
		RTCDeviceTy* embree_device; // Used in EmbreeAccel::build()
	};
	virtual void build(const BuildOptions& options, ShouldCancelCallback& should_cancel_callback, PrintOutput& print_output, bool verbose, glare::TaskManager& task_manager) = 0; // throws glare::Exception
#endif

	// Gets the built embree scene
	virtual RTCSceneTy* getEmbreeScene() { return NULL; }

	virtual Real meanCurvature(const HitInfo& hitinfo) const = 0;

	virtual bool isPlanar(Vec4f& normal_out) const = 0; // Does the whole geometry lie on a single plane?

	void incrementObjectUsageCount() { object_usage_count++; }
	void decrementObjectUsageCount() { object_usage_count--; }
	unsigned int getObjectUsageCount() const { return object_usage_count; }

	inline bool areSubElementsCurved() const { return sub_elements_curved; } // For testing for self intersections.  Can a ray launched from a sub-element hit the same sub-element at a decent distance?

	inline GeometryType getType() const { return geom_type; }
	inline bool isCamera() const { return geom_type == GeometryType_Camera; }


	std::vector<bool> mat_group_opaque; // For each material group of the mesh, are all materials from all instances fully opaque (not delta transmission)?

private:
	unsigned int object_usage_count; // Number of objects that use this geometry.
	GeometryType geom_type;
	bool sub_elements_curved;
};


typedef Reference<Geometry> GeometryRef;
