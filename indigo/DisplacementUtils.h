/*=====================================================================
DisplacementUtils.h
-------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISPLACEMENTUTILS_H_666_
#define __DISPLACEMENTUTILS_H_666_


#include "../utils/platform.h"
#include "../simpleraytracer/raymesh.h"
class ThreadContext;
class PrintOutput;


class DUVertex
{
public:
	DUVertex(){ anchored = false; }
	DUVertex(const Vec3f& pos_, const Vec3f& normal_) : pos(pos_), normal(normal_) { 
		//texcoords[0] = texcoords[1] = texcoords[2] = texcoords[3] = Vec2f(0.f, 0.f); 
		anchored = false; uv_discontinuity = false; }
	Vec3f pos;
	Vec3f normal;
	//Vec2f texcoords[4];
	//int adjacent_subdivided_tris;
	int adjacent_vert_0, adjacent_vert_1;
	float displacement; // will be set after displace() is called.
	bool anchored;

	bool uv_discontinuity;
	bool displaced; // Used as a flag in DisplacementUtils::displace().

	//Vec2f texcoords; // TEMP
	
	//static const unsigned int MAX_NUM_UV_SET_INDICES = 8;
	//unsigned int uv_set_indices[MAX_NUM_UV_SET_INDICES];
	//unsigned int num_uv_set_indices;
};


class DUVertexPolygon
{
public:
	unsigned int vertex_index;
	unsigned int uv_index;
};


// dimension 1
class DUEdge
{
public:
	DUEdge(){}
	DUEdge(uint32 v0, uint32 v1, uint32 uv0, uint32 uv1)
	{
		vertex_indices[0] = v0;
		vertex_indices[1] = v1;
		uv_indices[0] = uv0;
		uv_indices[1] = uv1;
	}
	unsigned int vertex_indices[2];
	unsigned int uv_indices[2];
};


class DUTriangle
{
public:
	DUTriangle(){}
	DUTriangle(unsigned int v0_, unsigned int v1_, unsigned int v2_, unsigned int uv0, unsigned int uv1, unsigned int uv2, unsigned int matindex/*, unsigned int dimension_*/) : tri_mat_index(matindex)/*, dimension(dimension_)*///, num_subdivs(num_subdivs_)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;

		uv_indices[0] = uv0;
		uv_indices[1] = uv1;
		uv_indices[2] = uv2;
	}
	unsigned int vertex_indices[3];
	unsigned int uv_indices[3];
	unsigned int tri_mat_index;
};

class DUQuad
{
public:
	DUQuad(){}
	DUQuad(	uint32_t v0_, uint32_t v1_, uint32_t v2_, uint32_t v3_,
			uint32_t uv0, uint32_t uv1, uint32_t uv2, uint32_t uv3,
			uint32_t mat_index_) : mat_index(mat_index_)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
		vertex_indices[3] = v3_;

		uv_indices[0] = uv0;
		uv_indices[1] = uv1;
		uv_indices[2] = uv2;
		uv_indices[3] = uv3;
	}
	uint32_t vertex_indices[4];
	uint32_t uv_indices[4];
	uint32_t mat_index;

	//uint32_t padding[2];
};


SSE_CLASS_ALIGN DUOptions
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	Matrix4f object_to_camera;

	bool wrap_u;
	bool wrap_v;

	bool view_dependent_subdivision;
	double pixel_height_at_dist_one;
	double subdivide_pixel_threshold;
	double subdivide_curvature_threshold;
	double displacement_error_threshold;
	unsigned int max_num_subdivisions;

	std::vector<Plane<float> > camera_clip_planes_os; // Camera clip planes in object space.
};


/*=====================================================================
DisplacementUtils
-----------------

=====================================================================*/
class DisplacementUtils
{
public:
	/*=====================================================================
	DisplacementUtils
	-----------------
	
	=====================================================================*/
	DisplacementUtils();

	~DisplacementUtils();


	static void subdivideAndDisplace(
		PrintOutput& print_output,
		ThreadContext& context,
		const std::vector<Reference<Material> >& materials,
		bool smooth,
		const js::Vector<RayMeshTriangle, 16>& tris_in,
		const js::Vector<RayMeshQuad, 16>& quads_in,
		const std::vector<RayMeshVertex>& verts_in,
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		bool use_shading_normals,
		js::Vector<RayMeshTriangle, 16>& tris_out,
		std::vector<RayMeshVertex>& verts_out,
		std::vector<Vec2f>& uvs_out
	);


	static void test();

private:
	static void displace(
		ThreadContext& context,
		const std::vector<Reference<Material> >& materials,
		bool use_anchoring,
		const std::vector<DUTriangle>& tris,
		const std::vector<DUQuad>& quads,
		const std::vector<DUVertex>& verts_in,
		const std::vector<Vec2f>& uvs,
		unsigned int num_uv_sets,
		std::vector<DUVertex>& verts_out
		);

	static void linearSubdivision(
		PrintOutput& print_output,
		ThreadContext& context,
		const std::vector<Reference<Material> >& materials,
		const std::vector<DUVertexPolygon>& vert_polygons_in,
		const std::vector<DUEdge>& edges_in,
		const std::vector<DUTriangle>& tris_in,
		const std::vector<DUQuad>& quads_in,
		const std::vector<DUVertex>& verts_in,
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		std::vector<DUVertexPolygon>& vert_polygons_out,
		std::vector<DUEdge>& edges_out,
		std::vector<DUTriangle>& tris_out,
		std::vector<DUQuad>& quads_out,
		std::vector<DUVertex>& verts_out,
		std::vector<Vec2f>& uvs_out
		);

	static void averagePass(
		const std::vector<DUVertexPolygon>& vert_polygons,
		const std::vector<DUEdge>& edges,
		const std::vector<DUTriangle>& tris,
		const std::vector<DUQuad>& quads,
		const std::vector<DUVertex>& verts,
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets,
		const DUOptions& options,
		std::vector<DUVertex>& new_verts_out,
		std::vector<Vec2f>& uvs_out
		);
};


#endif //__DISPLACEMENTUTILS_H_666_
