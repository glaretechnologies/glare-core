/*=====================================================================
raymesh.h
---------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RAYMESH_H_666_
#define __RAYMESH_H_666_



#include "geometry.h"
#include "../simpleraytracer/ModelLoadingStreamHandler.h"
#include "../physics/jscol_Tree.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
namespace js{ class Triangle; }
namespace js{ class EdgeTri; }

class SimpleIndexTri
{
public:
	unsigned int vertex_indices[3];
	unsigned int tri_mat_index;
};

/*=====================================================================
RayMesh
-------

=====================================================================*/
class RayMesh : public Geometry, public ModelLoadingStreamHandler
{
public:
	/*=====================================================================
	RayMesh
	-------
	
	=====================================================================*/
	RayMesh(const std::string& name, bool enable_normal_smoothing);

	virtual ~RayMesh();

	void build(const std::string& indigo_base_dir_path, bool use_cached_trees);

	////////////////////// Geometry interface ///////////////////
	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "RayMesh (name=" + name + ")"; }
	
	virtual const Vec3d getShadingNormal(const FullHitInfo& hitinfo) const;
	virtual const Vec3d getGeometricNormal(const FullHitInfo& hitinfo) const;
	const Vec2d getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual bool getTangents(const FullHitInfo& hitinfo, unsigned int texcoord_set, Vec3d& tangent_out, Vec3d& bitangent_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void emitterInit();
	virtual const Vec3d sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										  HitInfo& hitinfo_out) const;
	virtual double surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const;
	virtual double surfaceArea(const Matrix3d& to_parent) const;

	virtual int UVSetIndexForName(const std::string& uvset_name) const;
	//////////////////////////////////////////////////////////

	

	////////// ModelLoadingStreamHandler interface /////////////
	virtual void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets);
	virtual void addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets);
	virtual void addTriangle(const unsigned int* vertex_indices, unsigned int material_index);
	virtual void addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index);
	virtual void addMaterialUsed(const std::string& material_name);
	////////////////////////////////////////////////////////////////



	////////////////////// Stuff used by the kdtree/BIH ///////////////////////
	inline const Vec3f& triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const;
	inline const Vec3f& triNormal(unsigned int triindex) const;
	inline const unsigned int getNumTris() const { return (unsigned int)triangles.size(); }
	inline const unsigned int getNumVerts() const { return num_vertices; }
	//inline const std::vector<unsigned int>& getTriMaterialIndices() const { return tri_mat_indices; }
	////////////////////////////////////////////////////////////////////////////

	
	//Debugging:
	const js::Tree* getTreeDebug() const { return tritree.get(); }


	const std::string& getName() const { return name; }

	
	std::map<std::string, int> uvset_name_to_index;
	
	//Map from material name to the index of the material in the final per-object material array
	std::map<std::string, int> matname_to_index_map;
private:
	void doInitAsEmitter();

	std::string name;

	std::auto_ptr<js::Tree> tritree;

	bool enable_normal_smoothing;

	//------------------------------------------------------------------------
	//compact mesh stuff
	//------------------------------------------------------------------------
	/*
		vertex_data has the following format:

		float pos[3]
		float normal[3]
		float texcoords[2][num_texcoord_sets]

		repeated num_vertices times

		for a total size of num_vertices * (num_texcoord_sets*2 + 6) * sizeof(float)
	*/
	inline unsigned int vertSize() const;
	inline unsigned int vertOffset(unsigned int vertindex) const; //in units of floats
	inline const Vec3f& vertNormal(unsigned int vertindex) const;
	inline const Vec2f& vertTexCoord(unsigned int vertindex, unsigned int texcoord_set_index) const;
	inline const Vec3f& vertPos(unsigned int vertindex) const;

	unsigned int num_texcoord_sets;
	unsigned int num_vertices;
	std::vector<float> vertex_data;
	std::vector<SimpleIndexTri> triangles;

	//------------------------------------------------------------------------
	//emitter stuff
	//------------------------------------------------------------------------
	bool done_init_as_emitter;
	double total_surface_area;
	std::vector<double> cdf;
};

const Vec3f& RayMesh::vertPos(unsigned int vertindex) const
{
	assert(vertindex < num_vertices);
	return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}

const Vec3f& RayMesh::triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const
{
	//return tritree->triVertPos(triindex, vertindex);
	const unsigned int vertindex = triangles[triindex].vertex_indices[vertindex_in_tri];
	assert(vertindex < num_vertices);
	return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}


const Vec3f& RayMesh::triNormal(unsigned int triindex) const
{
	//return triangles[triindex].getNormal();
	return tritree->triGeometricNormal(triindex);
}



unsigned int RayMesh::vertSize() const
{
	return num_texcoord_sets*2 + 6;
}

unsigned int RayMesh::vertOffset(unsigned int vertindex) const //in units of float
{
	assert(vertindex < num_vertices);
	return vertSize() * vertindex;
}
	
const Vec3f& RayMesh::vertNormal(unsigned int vertindex) const
{
	assert(vertindex < num_vertices);
	return *((const Vec3f*)&vertex_data[vertOffset(vertindex) + 3]);
}

const Vec2f& RayMesh::vertTexCoord(unsigned int vertindex, unsigned int texcoord_set_index) const
{
	assert(vertindex < num_vertices);
	assert(texcoord_set_index < num_texcoord_sets);
	return *((const Vec2f*)&vertex_data[vertOffset(vertindex) + 6 + texcoord_set_index*2]);
}

#endif //__RAYMESH_H_666_
