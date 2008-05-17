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
#include "../physics/jscol_ObjectTree.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
namespace js{ class Triangle; }
namespace js{ class EdgeTri; }
class Material;

class RayMeshTriangle
{
public:
	RayMeshTriangle(){}
	RayMeshTriangle(unsigned int v0_, unsigned int v1_, unsigned int v2_, unsigned int matindex) : tri_mat_index(matindex)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
	}
	unsigned int vertex_indices[3];
	unsigned int tri_mat_index;
};

class RayMeshVertex
{
public:
	RayMeshVertex(){}
	RayMeshVertex(const Vec3f& pos_, const Vec3f& normal_) : pos(pos_), normal(normal_) { texcoords[0] = texcoords[1] = texcoords[2] = texcoords[3] = Vec2f(0.f, 0.f); }
	Vec3f pos;
	Vec3f normal;
	Vec2f texcoords[4];
};

class RayMeshExcep
{
public:
	RayMeshExcep(const std::string& s_) : s(s_) {}
	~RayMeshExcep(){}
	const std::string& what() const { return s; }
private:
	std::string s;
};

#define SUB_D_TREE 1


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
	RayMesh(const std::string& name, bool enable_normal_smoothing, unsigned int num_subdivisions = 0, double subdivide_pixel_threshold = 8.0);

	virtual ~RayMesh();


	////////////////////// Geometry interface ///////////////////
	virtual double traceRay(const Ray& ray, double max_t, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceFullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::ObjectTreePerThreadData& context, const Object* object) const;
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
	
	virtual void subdivideAndDisplace(const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, const std::vector<Material*>& materials);
	virtual void build(const std::string& indigo_base_dir_path, bool use_cached_trees);
	virtual const std::string getName() const { return name; }
	//////////////////////////////////////////////////////////



	////////// ModelLoadingStreamHandler interface /////////////
	virtual void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets);
	virtual void addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets);
	virtual void addTriangle(const unsigned int* vertex_indices, unsigned int material_index);
	virtual void addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index);
	virtual void addMaterialUsed(const std::string& material_name);
	////////////////////////////////////////////////////////////////
	bool isMaterialAlreadyUsed(const std::string& material_name);



	////////////////////// Stuff used by the kdtree/BIH ///////////////////////
	inline const Vec3f& triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const;
	inline const Vec3f& triNormal(unsigned int triindex) const;
	inline const unsigned int getNumTris() const { return (unsigned int)triangles.size(); }
	//inline const unsigned int getNumVerts() const { return num_vertices; }

	inline const unsigned int getNumVerts() const { return vertices.size(); }

	//inline const std::vector<unsigned int>& getTriMaterialIndices() const { return tri_mat_indices; }
	////////////////////////////////////////////////////////////////////////////

	
	
	//Debugging:

	const js::Tree* getTreeDebug() const { return tritree.get(); }

	void printTreeStats();
	void printTraceStats();



private:
	void doInitAsEmitter();

	const Vec3d computeTriGeometricNormal(const FullHitInfo& hitinfo) const;

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
	//unsigned int num_vertices;
	//std::vector<float> vertex_data;
	std::vector<RayMeshVertex> vertices;
	std::vector<RayMeshTriangle> triangles;

	unsigned int num_subdivisions;
	double subdivide_pixel_threshold;

	//------------------------------------------------------------------------
	//emitter stuff
	//------------------------------------------------------------------------
	bool done_init_as_emitter;
	double total_surface_area;
	std::vector<double> cdf;
};

const Vec3f& RayMesh::vertPos(unsigned int vertindex) const
{
	return vertices[vertindex].pos;
	//assert(vertindex < num_vertices);
	//return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}

const Vec3f& RayMesh::triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const
{
	
	const unsigned int vertindex = triangles[triindex].vertex_indices[vertindex_in_tri];
	
	return vertices[vertindex].pos;
	//assert(vertindex < num_vertices);
	//return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}


const Vec3f& RayMesh::triNormal(unsigned int triindex) const
{
	return tritree->triGeometricNormal(triindex);
}




/*unsigned int RayMesh::vertSize() const
{
	return num_texcoord_sets*2 + 6;
}

unsigned int RayMesh::vertOffset(unsigned int vertindex) const //in units of float
{
	assert(vertindex < num_vertices);
	return vertSize() * vertindex;
}*/
	
const Vec3f& RayMesh::vertNormal(unsigned int vertindex) const
{
	return vertices[vertindex].normal;
	//assert(vertindex < num_vertices);
	//return *((const Vec3f*)&vertex_data[vertOffset(vertindex) + 3]);
}

const Vec2f& RayMesh::vertTexCoord(unsigned int vertindex, unsigned int texcoord_set_index) const
{
	return vertices[vertindex].texcoords[texcoord_set_index];

	//assert(vertindex < num_vertices);
	//assert(texcoord_set_index < num_texcoord_sets);
	//return *((const Vec2f*)&vertex_data[vertOffset(vertindex) + 6 + texcoord_set_index*2]);
}

#endif //__RAYMESH_H_666_
