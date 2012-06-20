/*=====================================================================
raymesh.h
---------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RAYMESH_H_666_
#define __RAYMESH_H_666_


#include "../utils/platform.h"
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
namespace Indigo{ class Mesh; }


class RayMeshTriangle
{
public:
	RayMeshTriangle() : tri_mat_index(0) {}
	RayMeshTriangle(unsigned int v0_, unsigned int v1_, unsigned int v2_, unsigned int matindex) : tri_mat_index(matindex << 1)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
	}
	uint32 vertex_indices[3];
	uint32 uv_indices[3];

	//Vec3f geom_normal;
	float inv_cross_magnitude;

	inline void setUseShadingNormals(bool b)
	{
		if(b)
			tri_mat_index |= 0x1;
		else
			tri_mat_index &= 0xFFFFFFFE;
	}
	inline uint32 getUseShadingNormals() const
	{
		return tri_mat_index & 0x1;
	}

	inline void setTriMatIndex(uint32 i)
	{ 
		// Clear upper 31 bits, and OR with new index.
		tri_mat_index = (tri_mat_index & 0x1) | (i << 1); 
	}
	inline uint32 getTriMatIndex() const { return tri_mat_index >> 1; }

private:
	uint32 tri_mat_index; // least significant bit is normal smoothing flag.
};


class RayMeshQuad
{
public:
	RayMeshQuad(){}
	RayMeshQuad(uint32_t v0_, uint32_t v1_, uint32_t v2_, uint32_t v3_, uint32_t mat_index_) : mat_index(mat_index_ << 1)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
		vertex_indices[3] = v3_;
	}
	uint32_t vertex_indices[4];
	uint32_t uv_indices[4];

	//Vec3f geom_normal;
	float inv_cross_magnitude;

	inline void setUseShadingNormals(bool b)
	{
		if(b)
			mat_index |= 0x1;
		else
			mat_index &= 0xFFFFFFFE;
	}
	inline uint32 getUseShadingNormals() const
	{
		return mat_index & 0x1;
	}

	inline void setMatIndex(uint32_t i)
	{ 
		// Clear upper 31 bits, and OR with new index.
		mat_index = (mat_index & 0x1) | (i << 1); 
	}
	inline uint32 getMatIndex() const { return mat_index >> 1; }

private:
	uint32_t mat_index; // least significant bit is normal smoothing flag.

	uint32_t padding[2]; // to make structure multiple of 4 32bit ints
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

	inline bool operator < (const RayMeshVertex& b) const
	{
		if(pos < b.pos)
			return true;
		else if(b.pos < pos) // else if a.pos > b.pos
			return false;
		else	// else pos == b.pos
			return normal < b.normal;
	}

	inline bool operator == (const RayMeshVertex& other) const { return pos == other.pos && normal == other.normal; }
};


/*=====================================================================
RayMesh
-------

=====================================================================*/
SSE_CLASS_ALIGN RayMesh : public Geometry
{
public:
	INDIGO_ALIGNED_NEW_DELETE

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
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;
	virtual const js::AABBox& getAABBoxWS() const;
	//virtual const std::string debugName() const;
	
	//virtual const Vec3Type getShadingNormal(const HitInfo& hitinfo) const;
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_error_out) const;
	const TexCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumUVCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const;
	virtual void getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out) const;

	virtual void getAlphaBetaPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& m_out) const;

	//virtual void getAlphaBetaPartialDerivs(const HitInfo& hitinfo, unsigned int texcoord_set, Matrix2f& m_out) const;
	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const;

	virtual void sampleSurface(const SamplePair& samples, SampleResults& results_out) const;

	virtual bool subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose);
	virtual void build(const std::string& appdata_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	virtual bool areSubElementsCurved() const;
	virtual Vec3RealType getBoundingRadius() const;
	//virtual const Vec3Type positionForHitInfo(const HitInfo& hitinfo) const;
	//////////////////////////////////////////////////////////

	AlignedRef<RayMesh, 16> getClippedCopy(const std::vector<Plane<float> >& section_planes_os) const;

	
	void fromIndigoMesh(const Indigo::Mesh& mesh);

	//////////  old ModelLoadingStreamHandler interface /////////////
	//////////  raymesh does not implement this interface ///////////
	//////////  anymore but functions are still used by /////////////
	//////////  fromIndigoMesh() ////////////////////////////////////
	void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets);
	//virtual void addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets);
	//virtual void addTriangle(const unsigned int* vertex_indices, unsigned int material_index);
	void addVertex(const Vec3f& pos);
	void addVertex(const Vec3f& pos, const Vec3f& normal);
	void addUVs(const std::vector<Vec2f>& uvs);
	void addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);
	void addQuad(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);

	//void addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index);
	void addMaterialUsed(const std::string& material_name);
	void endOfModel();
	////////////////////////////////////////////////////////////////
	void addTriangleUnchecked(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index, bool use_shading_normals);
	void addVertexUnchecked(const Vec3f& pos, const Vec3f& normal);



	////////////////////// Stuff used by the kdtree/BIH ///////////////////////
	INDIGO_STRONG_INLINE const Vec3f& triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const;
	//INDIGO_STRONG_INLINE const Vec3f triNormal(unsigned int triindex) const;
	INDIGO_STRONG_INLINE const unsigned int getNumTris() const { return (unsigned int)triangles.size(); }
	INDIGO_STRONG_INLINE const unsigned int getNumQuads() const { return (unsigned int)quads.size(); }
	//inline const unsigned int getNumVerts() const { return num_vertices; }

	INDIGO_STRONG_INLINE const unsigned int getNumVerts() const { return (unsigned int)vertices.size(); }

	//inline const std::vector<unsigned int>& getTriMaterialIndices() const { return tri_mat_indices; }
	////////////////////////////////////////////////////////////////////////////

	// This is the number of UV pairs per uv set (layer).
	inline unsigned int getNumUVGroups() const { return num_uv_sets == 0 ? 0 : (unsigned int)uvs.size() / num_uv_sets; }
	
	//Debugging:

	const js::Tree* getTreeDebug() const { return tritree; }

	void printTreeStats();
	void printTraceStats();


	const std::vector<RayMeshVertex>& getVertices() const { return vertices; }
	//const std::vector<RayMeshTriangle>& getTriangles() const { return triangles; }
	const js::Vector<RayMeshTriangle, 16>& getTriangles() const { return triangles; }
	const std::vector<Vec2f>& getUVs() const { return uvs; }
	
	const unsigned int numUVSets() const { return num_uv_sets; }

	bool isUsingShadingNormals() const { return enable_normal_smoothing; }

private:
	void computeShadingNormals(PrintOutput& print_output, bool verbose);
	void mergeVerticesWithSamePosAndNormal(PrintOutput& print_output, bool verbose);
	void mergeUVs(PrintOutput& print_output, bool verbose);
	void doInitAsEmitter();
	bool built() const { return tritree != NULL; }

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
	//std::vector<RayMeshTriangle> triangles;
	js::Vector<RayMeshTriangle, 16> triangles;
	js::Vector<RayMeshQuad, 16> quads;

	float bounding_radius; // Computed in build()

	//std::vector<Vec3f> triangle_geom_normals;
	
	unsigned int num_uv_sets;
	std::vector<Vec2f> uvs;

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

