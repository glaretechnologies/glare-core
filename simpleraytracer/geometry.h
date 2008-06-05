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
#include <vector>
class Ray;
class RayBundle;
class World;
class PointTree;
class PhotonHit;
class HitInfo;
class FullHitInfo;
class DistanceFullHitInfo;
class PerThreadData;
class Object;
class RendererSettings;


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
class Geometry : /*public js::Intersectable, */public RefCounted
{
public:
	/*=====================================================================
	Geometry
	--------
	
	=====================================================================*/
	Geometry(){}
	virtual ~Geometry(){}


	/// intersectable interface ///
	virtual double traceRay(const Ray& ray, double max_t, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::ObjectTreePerThreadData& context, const Object* object) const = 0;
	virtual const std::string getName() const = 0;
	/// End intersectable interface ///

	virtual void getAllHits(const Ray& ray, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceFullHitInfo>& hitinfos_out) const = 0;

	virtual const Vec3d getShadingNormal(const FullHitInfo& hitinfo) const = 0;
	virtual const Vec3d getGeometricNormal(const FullHitInfo& hitinfo) const = 0;
	virtual const Vec2d getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const = 0;
	
	//returns true if could construct a suitable basis
	virtual bool getTangents(const FullHitInfo& hitinfo, unsigned int texcoord_set, Vec3d& tangent_out, Vec3d& bitangent_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const { return 0; }

	virtual void emitterInit() = 0;
	virtual const Vec3d sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out, HitInfo& hitinfo_out) const = 0;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const = 0; // PDF with respect to surface area metric, in parent space
	virtual double surfaceArea(const Matrix3d& to_parent) const = 0; //get surface area in parent space

	virtual void subdivideAndDisplace(const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, const std::vector<Material*>& materials, 
		const std::vector<Plane<double> >& camera_clip_planes
		){} // throws GeometryExcep
	virtual void build(const std::string& indigo_base_dir_path, const RendererSettings& settings) = 0; // throws GeometryExcep

	//virtual int UVSetIndexForName(const std::string& uvset_name) const = 0;


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
