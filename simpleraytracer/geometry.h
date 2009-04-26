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
#include "../physics/jscol_ObjectTreePerThreadData.h"
#include "../utils/refcounted.h"
#include "../indigo/TexCoordEvaluator.h"
#include "../indigo/SampleTypes.h"
#include <vector>
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
	/*=====================================================================
	Geometry
	--------
	
	=====================================================================*/
	Geometry(){}
	virtual ~Geometry(){}


	typedef Vec4f Pos3Type;
	typedef float Vec3RealType;
	typedef float Real;
	typedef Vec4f Vec3Type; //Vec3<Vec3RealType> Vec3Type;
	

	/// intersectable interface ///
	virtual Real traceRay(const Ray& ray, Real max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const = 0;
	virtual const std::string getName() const = 0;
	/// End intersectable interface ///

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;

	virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const = 0;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const = 0;
	virtual const TexCoordsType getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const = 0;
	virtual unsigned int getNumTexCoordSets() const = 0;
	
	// Get the partial derivatives of the surface position relative to the 'intrinsic parameters' u and v.
	// Also gets the partial derivatives of the shading normal relative to the 'intrinsic parameters' u and v.
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const = 0;
	virtual void getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const = 0;
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

	// Get the probability density of sampling the given point on the surface of the given sub-element, with respect to the world space area measure.
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const = 0;
	//virtual double subElementSamplingPDF(unsigned int sub_elem_index) const = 0;

	virtual bool isEnvSphereGeometry() const = 0;

	virtual void subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, 
		const std::vector<Plane<double> >& camera_clip_planes, PrintOutput& print_output
		) = 0; // throws GeometryExcep
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings, PrintOutput& print_output) = 0; // throws GeometryExcep

	//virtual int UVSetIndexForName(const std::string& uvset_name) const = 0;

	virtual Vec3RealType getBoundingRadius() const = 0;



	const std::map<std::string, unsigned int>& getMaterialNameToIndexMap() const { return matname_to_index_map; }
	const std::map<std::string, unsigned int>& getUVSetNameToIndexMap() const { return uvset_name_to_index; }
protected:
	// Map from material name to the index of the material in the final per-object material array.
	// Note that this could also be a vector.
	std::map<std::string, unsigned int> matname_to_index_map;

	std::map<std::string, unsigned int> uvset_name_to_index;
private:
};


#endif //__GEOMETRY_H_666_
