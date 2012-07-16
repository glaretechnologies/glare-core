/*=====================================================================
geometry.h
----------
File created by ClassTemplate on Wed Apr 14 21:19:37 2004
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __GEOMETRY_H_666_
#define __GEOMETRY_H_666_


#include "../maths/vec2.h"
#include "../maths/coordframe.h"
#include "../maths/plane.h"
#include "../physics/jscol_aabbox.h"
#include "../utils/refcounted.h"
#include "../indigo/TexCoordEvaluator.h"
#include "../indigo/SampleTypes.h"
#include "../raytracing/hitinfo.h"
#include <vector>
#include <map>
class Ray;
class RayBundle;
class World;
class PointTree;
class PhotonHit;
class HitInfo;
class FullHitInfo;
class DistanceHitInfo;
class PerThreadData;
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
interface that represents the shape of an object
=====================================================================*/
SSE_CLASS_ALIGN Geometry : /*public js::Intersectable, */public RefCounted, public TexCoordEvaluator
{
public:
	//INDIGO_ALIGNED_NEW_DELETE

	/*=====================================================================
	Geometry
	--------
	
	=====================================================================*/
	Geometry() : object_usage_count(0) {}
	virtual ~Geometry(){}


	typedef Vec4f Pos3Type;
	typedef float Vec3RealType;
	typedef float Real;
	typedef double DistType;
	typedef Vec4f Vec3Type; //Vec3<Vec3RealType> Vec3Type;
	

	/// intersectable interface ///
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const = 0;
	virtual const std::string getName() const = 0;
	/// End intersectable interface ///

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;

	//virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const = 0;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const = 0;

	// Returns the coordinates (u, v) for the given uv-set, given the intrinsic coordinates (alpha, beta) in hitinfo.
	virtual const TexCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const = 0;
	virtual unsigned int getNumUVCoordSets() const = 0;
	

	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const = 0;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out) const = 0;
	
	// Get the partial derivatives of the surface position relative to the 'intrinsic parameters' alpha and beta.
	// Also gets the partial derivatives of the shading normal relative to the 'intrinsic parameters' alpha and beta.
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const = 0;
	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const = 0;
	//returns true if could construct a suitable basis
	//virtual bool getTangents(const FullHitInfo& hitinfo, unsigned int texcoord_set, Vec3d& tangent_out, Vec3d& bitangent_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const { return 0; }

	//virtual void emitterInit() = 0;
	//virtual const Vec3d sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out, HitInfo& hitinfo_out) const = 0;
	//virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const = 0; // PDF with respect to surface area metric, in parent space
	//virtual double surfaceArea(const Matrix3d& to_parent) const = 0; //get surface area in parent space
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const = 0;
	//virtual unsigned int getNumSubElems() const = 0;
	// Sample the surface of the given sub-element.
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const = 0;

	
	class SampleResults
	{
	public:
		Pos3Type pos;
		Vec3Type N_g;
		HitInfo hitinfo;
		Real pd;
	};
	virtual void sampleSurface(const SamplePair& samples, SampleResults& results_out) const { assert(0); }


	// Get the probability density of sampling the given point on the surface of the given sub-element, with respect to the world space area measure.
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const = 0;
	//virtual double subElementSamplingPDF(unsigned int sub_elem_index) const = 0;

	virtual bool isEnvSphereGeometry() const = 0;
	virtual bool areSubElementsCurved() const = 0; // For testing for self intersections.  Can a ray launched from a sub-element hit the same sub-element at a decent distance?

	// Returns true if possibly clipped by section planes, false otherwise.
	virtual bool subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes_os, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose
		) = 0; // throws GeometryExcep
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) = 0; // throws GeometryExcep

	//virtual int UVSetIndexForName(const std::string& uvset_name) const = 0;

	virtual Vec3RealType getBoundingRadius() const = 0;

	//virtual const Vec3Type positionForHitInfo(const HitInfo& hitinfo, Real& pos_os_rel_error_out) const = 0;

	virtual Real positionForInstrinsicCoordsJacobian(unsigned int sub_elem_index) const { return 1; }

	void incrementObjectUsageCount() { object_usage_count++; }
	unsigned int getObjectUsageCount() const { return object_usage_count; }


	const std::map<std::string, unsigned int>& getMaterialNameToIndexMap() const { return matname_to_index_map; }
	//const std::map<std::string, unsigned int>& getUVSetNameToIndexMap() const { return uvset_name_to_index; }
protected:
	// Map from material name to the index of the material in the final per-object material array.
	// Note that this could also be a vector.
	std::map<std::string, unsigned int> matname_to_index_map;
	//std::map<std::string, unsigned int>& getUVSetNameToIndexMap() { return uvset_name_to_index; }

private:
	//std::map<std::string, unsigned int> uvset_name_to_index;
	unsigned int object_usage_count; // Number of objects that use this geometry.
};


#endif //__GEOMETRY_H_666_
