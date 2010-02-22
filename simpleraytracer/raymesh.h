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
class RendererSettings;
class PrintOutput;


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
	unsigned int uv_indices[3];
	unsigned int tri_mat_index;
	Vec3f geom_normal;
};


class RayMeshVertex
{
public:
	RayMeshVertex(){}
	RayMeshVertex(const Vec3f& pos_, const Vec3f& normal_) : 
		pos(pos_), 
		normal(normal_) 
	{}

	Vec3f pos;
	Vec3f normal;
};


/*=====================================================================
RayMesh
-------

=====================================================================*/
SSE_CLASS_ALIGN RayMesh : public Geometry, public ModelLoadingStreamHandler
{
public:
	/*=====================================================================
	RayMesh
	-------
	
	=====================================================================*/
	RayMesh(const std::string& name, bool enable_normal_smoothing, 
		unsigned int max_num_subdivisions = 0, 
		double subdivide_pixel_threshold = 0.0, 
		bool subdivision_smoothing = true, 
		double subdivide_curvature_threshold = 0.0,
		bool merge_vertices_with_same_pos_and_normal = false,
		bool wrap_u = false,
		bool wrap_v = false,
		bool view_dependent_subdivision = false,
		double displacement_error_threshold = 0.0
		);

	virtual ~RayMesh();


	////////////////////// Geometry interface ///////////////////
	virtual Real traceRay(const Ray& ray, Real max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;
	virtual const js::AABBox& getAABBoxWS() const;
	//virtual const std::string debugName() const;
	
	virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out) const;
	const TexCoordsType getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumTexCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const;
	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const;

	virtual void subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, PrintOutput& print_output, bool verbose);
	virtual void build(const std::string& appdata_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	virtual bool areSubElementsCurved() const;
	virtual Vec3RealType getBoundingRadius() const;
	virtual const Vec3Type positionForHitInfo(const HitInfo& hitinfo) const;
	//////////////////////////////////////////////////////////



	////////// ModelLoadingStreamHandler interface /////////////
	virtual void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets);
	//virtual void addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets);
	//virtual void addTriangle(const unsigned int* vertex_indices, unsigned int material_index);
	virtual void addVertex(const Vec3f& pos);
	virtual void addVertex(const Vec3f& pos, const Vec3f& normal);
	virtual void addUVs(const std::vector<Vec2f>& uvs);
	virtual void addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);

	virtual void addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index);
	virtual void addMaterialUsed(const std::string& material_name);
	virtual void endOfModel();
	////////////////////////////////////////////////////////////////
	void addTriangleUnchecked(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);



	////////////////////// Stuff used by the kdtree/BIH ///////////////////////
	INDIGO_STRONG_INLINE const Vec3f& triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const;
	//INDIGO_STRONG_INLINE const Vec3f triNormal(unsigned int triindex) const;
	INDIGO_STRONG_INLINE const unsigned int getNumTris() const { return (unsigned int)triangles.size(); }
	//inline const unsigned int getNumVerts() const { return num_vertices; }

	INDIGO_STRONG_INLINE const unsigned int getNumVerts() const { return (unsigned int)vertices.size(); }

	//inline const std::vector<unsigned int>& getTriMaterialIndices() const { return tri_mat_indices; }
	////////////////////////////////////////////////////////////////////////////

	INDIGO_STRONG_INLINE unsigned int getNumUVGroups() const { return num_uv_groups; }
	
	//Debugging:

	const js::Tree* getTreeDebug() const { return tritree; }

	void printTreeStats();
	void printTraceStats();


	const std::vector<RayMeshVertex>& getVertices() const { return vertices; }
	const std::vector<RayMeshTriangle>& getTriangles() const { return triangles; }
	const std::vector<Vec2f>& getUVs() const { return uvs; }
	
	const unsigned int numUVSets() const { return num_uvs_per_group; }

private:
	void computeShadingNormals(PrintOutput& print_output, bool verbose);
	void mergeVerticesWithSamePosAndNormal(PrintOutput& print_output, bool verbose);
	void doInitAsEmitter();

	//inline unsigned int vertSize() const;
	//inline unsigned int vertOffset(unsigned int vertindex) const; //in units of floats
	inline const Vec3f& vertNormal(unsigned int vertindex) const;
	//inline const Vec2f& vertTexCoord(unsigned int vertindex, unsigned int texcoord_set_index) const;
	inline const Vec3f& vertPos(unsigned int vertindex) const;


	std::string name;

	//std::auto_ptr<js::Tree> tritree;
	js::Tree* tritree;

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

	

	
	//unsigned int num_texcoord_sets;
	//unsigned int num_vertices;
	//std::vector<float> vertex_data;
	std::vector<RayMeshVertex> vertices;
	std::vector<RayMeshTriangle> triangles;

	//std::vector<Vec3f> triangle_geom_normals;
	
	//unsigned int num_uvs;
	
	unsigned int num_uvs_per_group; // 0 - 4
	unsigned int num_uv_groups; // will be roughly equal to number of vertice
	std::vector<Vec2f> uvs; // will have num_uv_groups * num_uvs_per_group elements

	unsigned int max_num_subdivisions;
	double subdivide_pixel_threshold;
	double subdivide_curvature_threshold;
	bool subdivision_smoothing;

	bool subdivide_and_displace_done;
	bool vertex_shading_normals_provided;

	bool merge_vertices_with_same_pos_and_normal;
	bool view_dependent_subdivision;
	double displacement_error_threshold;

	bool wrap_u;
	bool wrap_v;
};


const Vec3f& RayMesh::vertPos(unsigned int vertindex) const
{
	return vertices[vertindex].pos;
	//assert(vertindex < num_vertices);
	//return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}


const Vec3f& RayMesh::triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const
{
	
	//const unsigned int vertindex = triangles[triindex].vertex_indices[vertindex_in_tri];
	
	return vertices[triangles[triindex].vertex_indices[vertindex_in_tri]].pos;
	//assert(vertindex < num_vertices);
	//return *((const Vec3f*)&vertex_data[vertOffset(vertindex)]);
}


/*const Vec3f RayMesh::triNormal(unsigned int triindex) const
{
	return tritree->triGeometricNormal(triindex);
}*/




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

/*const Vec2f& RayMesh::vertTexCoord(unsigned int vertindex, unsigned int texcoord_set_index) const
{
	return vertices[vertindex].texcoords[texcoord_set_index];

	//assert(vertindex < num_vertices);
	//assert(texcoord_set_index < num_texcoord_sets);
	//return *((const Vec2f*)&vertex_data[vertOffset(vertindex) + 6 + texcoord_set_index*2]);
}*/


#endif //__RAYMESH_H_666_

