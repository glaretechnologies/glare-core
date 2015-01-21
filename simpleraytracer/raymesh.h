/*=====================================================================
raymesh.h
---------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "geometry.h"
#include "../physics/jscol_Tree.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"
#include "../utils/Platform.h"
#include "../utils/Reference.h"
#include "../utils/Vector.h"
#include <string>
#include <memory>
#include <vector>
#include <limits>
class Material;
class RendererSettings;
class PrintOutput;
namespace Indigo{ class Mesh; }


enum RayMesh_ShadingNormals
{
	RayMesh_NoShadingNormals  = 0,
	RayMesh_UseShadingNormals = 1
};


#ifdef _MSC_VER // If we are compiling with visual studio:
#pragma warning ( push )
#pragma warning ( disable: 4324 ) // Disable 'structure was padded due to __declspec(align())'
#endif


// Should have a size of 32 bytes
SSE_CLASS_ALIGN RayMeshTriangle
{
public:
	RayMeshTriangle() : tri_mat_index(0)
	{
#ifndef NDEBUG
		inv_cross_magnitude = std::numeric_limits<float>::quiet_NaN();
#endif
	}

	RayMeshTriangle(unsigned int v0_, unsigned int v1_, unsigned int v2_, unsigned int matindex, RayMesh_ShadingNormals use_shading_normals) 
	:	tri_mat_index((matindex << 1) | (uint32)use_shading_normals)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;

#ifndef NDEBUG
		inv_cross_magnitude = std::numeric_limits<float>::quiet_NaN();
#endif
	}
	
	inline void setUseShadingNormals(RayMesh_ShadingNormals use_shading_normals)
	{
		tri_mat_index &= 0xFFFFFFFE; // Clear lower bit
		tri_mat_index |= (uint32)use_shading_normals;
	}

	inline RayMesh_ShadingNormals getUseShadingNormals() const
	{
		return (RayMesh_ShadingNormals)(tri_mat_index & 0x1);
	}

	inline void setTriMatIndex(uint32 i)
	{ 
		// Clear upper 31 bits, and OR with new index.
		tri_mat_index = (tri_mat_index & 0x1) | (i << 1); 
	}
	inline uint32 getTriMatIndex() const { return tri_mat_index >> 1; }

	inline uint32 getRawTriMatIndex() const { return tri_mat_index; }
	inline void setRawTriMatIndex(uint32 i) { tri_mat_index = i; }

public:
	uint32 vertex_indices[3];
	uint32 uv_indices[3];
	float inv_cross_magnitude;
private:
	uint32 tri_mat_index; // least significant bit is normal smoothing flag.
};


// Should have a size of 48 bytes.
SSE_CLASS_ALIGN RayMeshQuad
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	RayMeshQuad(){}

	RayMeshQuad(uint32_t v0_, uint32_t v1_, uint32_t v2_, uint32_t v3_, uint32_t mat_index_, RayMesh_ShadingNormals use_shading_normals) 
	:	mat_index((mat_index_ << 1) | (uint32)use_shading_normals)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
		vertex_indices[3] = v3_;
	}

	inline void setUseShadingNormals(RayMesh_ShadingNormals use_shading_normals)
	{
		mat_index &= 0xFFFFFFFE; // Clear lower bit
		mat_index |= (uint32)use_shading_normals;
	}

	inline RayMesh_ShadingNormals getUseShadingNormals() const
	{
		return (RayMesh_ShadingNormals)(mat_index & 0x1);
	}

	inline void setMatIndex(uint32_t i)
	{ 
		// Clear upper 31 bits, and OR with new index.
		mat_index = (mat_index & 0x1) | (i << 1); 
	}
	inline uint32 getMatIndex() const { return mat_index >> 1; }

	inline uint32 getRawMatIndex() const { return mat_index; }

public:
	uint32_t vertex_indices[4];
	uint32_t uv_indices[4];
private:
	uint32_t mat_index; // least significant bit is normal smoothing flag.
};


// Should have a size of 32 bytes.
SSE_CLASS_ALIGN RayMeshVertex
{
public:
	RayMeshVertex(){}
	RayMeshVertex(const Vec3f& pos_, const Vec3f& normal_, float H_) : 
		pos(pos_), 
		normal(normal_),
		H(H_)
	{}

	Vec3f pos;
	Vec3f normal;
	float H; // Mean curvature
	//float padding;

	inline bool operator < (const RayMeshVertex& b) const
	{
		/*if(pos < b.pos)
			return true;
		else if(b.pos < pos) // else if a.pos > b.pos
			return false;
		else	// else pos == b.pos
			return normal < b.normal;*/
		return pos < b.pos;
	}

	inline bool operator == (const RayMeshVertex& other) const { return pos == other.pos;/* && normal == other.normal;*/ }
};


#ifdef _MSC_VER // If we are compiling with visual studio:
#pragma warning ( pop ) // re-enable 'structure was padded due to __declspec(align())'
#endif


/*=====================================================================
RayMesh
-------

=====================================================================*/
SSE_CLASS_ALIGN RayMesh : public Geometry
{
public:
	INDIGO_ALIGNED_NEW_DELETE


	RayMesh(const std::string& name, bool enable_normal_smoothing, 
		unsigned int max_num_subdivisions = 0, 
		double subdivide_pixel_threshold = 0.0, 
		bool subdivision_smoothing = true, 
		double subdivide_curvature_threshold = 0.0,
		bool merge_vertices_with_same_pos_and_normal = false,
		bool view_dependent_subdivision = false,
		double displacement_error_threshold = 0.0
		);

	virtual ~RayMesh();


	////////////////////// Geometry interface ///////////////////
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual const js::AABBox getAABBoxWS() const;
	
	virtual const Vec3Type getGeometricNormal(const HitInfo& hitinfo) const;
	virtual void getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const;
	virtual void getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_error_out, Vec2f& uv0_out) const;
	const TexCoordsType getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const;
	virtual unsigned int getNumUVCoordSets() const;
	virtual void getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const;

	virtual unsigned int getMaterialIndexForTri(unsigned int tri_index) const;
	
	virtual void getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const;
	virtual void sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, 
		float recip_sub_elem_area_ws, Real& p_out, unsigned int& mat_index_out, Vec2f& uv0_out) const;
	virtual double subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, float recip_sub_elem_area_ws) const;

	virtual void sampleSurface(const SamplePair& samples, SampleResults& results_out) const;

	virtual bool subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one,
		const std::vector<Plane<Vec3RealType> >& camera_clip_planes, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose);
	virtual void build(const std::string& cache_dir_path, const RendererSettings& settings, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager); // throws GeometryExcep
	virtual const std::string getName() const;
	virtual bool isEnvSphereGeometry() const;
	virtual Vec3RealType getBoundingRadius() const;
	virtual float meanCurvature(const HitInfo& hitinfo) const;
	//////////////////////////////////////////////////////////

	Reference<RayMesh> getClippedCopy(const std::vector<Plane<float> >& section_planes_os) const;

	
	void fromIndigoMesh(const Indigo::Mesh& mesh);

	///// These functions are used by various tests which construct RayMeshes directly. //////
	// They are also used by World::mergeObjectsToIdentityObject().
	// None of these functions check the validity of the data being passed in.
	// This means that all indices should be valid, and normals should be unit length.
	void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets);
	void addVertex(const Vec3f& pos);
	void addVertex(const Vec3f& pos, const Vec3f& normal);
	void addUVs(const std::vector<Vec2f>& uvs);
	void addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);
	void addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index, bool use_shading_normals);
	void addQuad(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index);
	////////////////////////////////////////////////////////////////


	////////////////////// Stuff used by the kdtree/BIH ///////////////////////
	INDIGO_STRONG_INLINE const Vec3f& triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const;
	INDIGO_STRONG_INLINE const unsigned int getNumTris() const { return (unsigned int)triangles.size(); }
	INDIGO_STRONG_INLINE const unsigned int getNumQuads() const { return (unsigned int)quads.size(); }
	INDIGO_STRONG_INLINE const unsigned int getNumVerts() const { return (unsigned int)vertices.size(); }
	////////////////////////////////////////////////////////////////////////////

	// This is the number of UV pairs per uv set (layer).
	inline unsigned int getNumUVsPerSet() const { return num_uv_sets == 0 ? 0 : (unsigned int)uvs.size() / num_uv_sets; }
	
	
	const js::Tree* getTreeDebug() const { return tritree; }

	void printTreeStats();
	void printTraceStats();

	typedef js::Vector<RayMeshVertex, 32> VertexVectorType;
	typedef js::Vector<RayMeshTriangle, 32> TriangleVectorType;
	typedef js::Vector<RayMeshQuad, 16> QuadVectorType;


	VertexVectorType& getVertices() { return vertices; }
	const VertexVectorType& getVertices() const { return vertices; }
	const TriangleVectorType& getTriangles() const { return triangles; }
	const std::vector<Vec2f>& getUVs() const { return uvs; }
	
	const unsigned int numUVSets() const { return num_uv_sets; }

	bool isUsingShadingNormals() const { return enable_normal_smoothing; }

	void setVertexShadingNormalsProvided(bool vertex_shading_normals_provided_) { vertex_shading_normals_provided = vertex_shading_normals_provided_; }

private:
	void computeShadingNormalsAndMeanCurvature(bool update_shading_normals, PrintOutput& print_output, bool verbose);
	void mergeVerticesWithSamePosAndNormal(PrintOutput& print_output, bool verbose);
	void mergeUVs(PrintOutput& print_output, bool verbose);
	void doInitAsEmitter();
	bool built() const { return tritree != NULL; }

	inline const Vec3f& vertNormal(unsigned int vertindex) const;
	inline const Vec3f& vertPos(unsigned int vertindex) const;


	std::string name;

	js::Tree* tritree;

	bool enable_normal_smoothing;


	VertexVectorType vertices;
	TriangleVectorType triangles;
	QuadVectorType quads;

	float bounding_radius; // Computed in build()
	
	unsigned int num_uv_sets;
	std::vector<Vec2f> uvs;
public:
	std::vector<Vec3f> vert_dp_du;
	std::vector<Vec3f> vert_dp_dv;
private:
	unsigned int max_num_subdivisions;
	double subdivide_pixel_threshold;
	double subdivide_curvature_threshold;
	bool subdivision_smoothing;

	bool subdivide_and_displace_done;
	bool vertex_shading_normals_provided;

	bool merge_vertices_with_same_pos_and_normal;
	bool view_dependent_subdivision;
	double displacement_error_threshold;
};


const Vec3f& RayMesh::vertPos(unsigned int vertindex) const
{
	return vertices[vertindex].pos;
}


const Vec3f& RayMesh::triVertPos(unsigned int triindex, unsigned int vertindex_in_tri) const
{
	return vertices[triangles[triindex].vertex_indices[vertindex_in_tri]].pos;
}


const Vec3f& RayMesh::vertNormal(unsigned int vertindex) const
{
	return vertices[vertindex].normal;
}
