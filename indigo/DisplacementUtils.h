/*=====================================================================
DisplacementUtils.h
-------------------
Copyright Glare Technologies Limited 2016 -
File created by ClassTemplate on Thu May 15 20:31:26 2008
=====================================================================*/
#pragma once


#include "../simpleraytracer/raymesh.h"
#include "../maths/Matrix4f.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/BitField.h"
class PrintOutput;
struct DUScratchInfo;
namespace glare { class TaskManager; }


class DUVertex
{
public:
	DUVertex(){}
	DUVertex(const Vec3f& pos_, const Vec3f& normal_) : pos(pos_), normal(normal_)
	{ 
		anchored = false; 
		uv_discontinuity = false;
	}

	Vec3f pos;
	Vec3f normal;
	float H;
	int adjacent_vert_0, adjacent_vert_1; // Used when anchored = true.
	float displacement; // will be set after displace() is called.
	bool anchored; // If this vertex lies on a T-junction, it is 'anchored', and will be given a position which is the average of adjacent_vert_0 and adjacent_vert_0, in order to prevent gaps.
	bool uv_discontinuity; // Does this vertex lie on an edge with a UV discontinuity?
};


// Actually this represents either a triangle or a quad.
class DUQuad
{
public:
	DUQuad(){}
	DUQuad(	uint32_t v0_, uint32_t v1_, uint32_t v2_, uint32_t v3_,
			uint32_t mat_index_)
	{
		vertex_indices[0] = v0_;
		vertex_indices[1] = v1_;
		vertex_indices[2] = v2_;
		vertex_indices[3] = v3_;

		adjacent_quad_index[0] = -1;
		adjacent_quad_index[1] = -1;
		adjacent_quad_index[2] = -1;
		adjacent_quad_index[3] = -1;

		edge_midpoint_vert_index[0] = -1;
		edge_midpoint_vert_index[1] = -1;
		edge_midpoint_vert_index[2] = -1;
		edge_midpoint_vert_index[3] = -1;

		mat_index = mat_index_;

		bitfield = BitField<uint32>(0);
	}

	inline bool isTri()   const { return vertex_indices[3] == std::numeric_limits<uint32>::max(); }
	inline bool isQuad()  const { return vertex_indices[3] != std::numeric_limits<uint32>::max(); }
	inline int numSides() const { return vertex_indices[3] == std::numeric_limits<uint32>::max() ? 3 : 4; }


	// Returns 0 if not, non-zero if there is a UV edge discontinuity.
	// NOTE: also records normal discontinuities.
	inline uint32 isUVEdgeDiscontinuity(int edge) const { return bitfield.getBitMasked(edge); }
	inline uint32 getUVEdgeDiscontinuity(int edge) const { return bitfield.getBit(edge); }

	inline void setUVEdgeDiscontinuity(int edge, uint32 val) { assert(val == 0 || val == 1); bitfield.setBit(edge, val); }
	inline void setUVEdgeDiscontinuityFalse(int edge) { bitfield.setBitToZero(edge); }
	inline void setUVEdgeDiscontinuityTrue(int edge)  { bitfield.setBitToOne(edge); }

	// Does the adjacent polygon over the given edge have a different winding order?  E.g. is its edge orientation the same?  In that case it needs to be reversed.
	inline uint32 isOrienReversed(int edge) const { return bitfield.getBitMasked(4 + edge); } // Returns non-zero if orientation is reversed.
	inline void setOrienReversedTrue(int edge) { bitfield.setBitToOne(4 + edge); }

	inline void setAdjacentQuadEdgeIndex(int this_edge, uint32 other_edge) { bitfield.setBitPair(8 + this_edge*2, other_edge); }
	inline uint32 getAdjacentQuadEdgeIndex(int this_edge) const
	{
		// Workaround for internal compiler error in VS2015.
		assert(bitfield.getBitPairICEWorkaround(8 + this_edge*2) == bitfield.getBitPair(8 + this_edge*2));
		return bitfield.getBitPairICEWorkaround(8 + this_edge*2);
	}

	inline uint32 isDead() const { return bitfield.getBitMasked(16); } // Returns non-zero value if quad is dead.
	inline void setDeadTrue() { bitfield.setBitToOne(16); }

	uint32_t vertex_indices[4]; // Indices of the corner vertices.  If vertex_indices[3] == std::numeric_limits<uint32>::max(), then this is a triangle.
	//16 bytes

	int adjacent_quad_index[4]; // Index of adjacent quad along the given edge, or -1 if no adjacent quad. (border edge)
	int edge_midpoint_vert_index[4]; // Indices of the new vertices at the midpoint of each edge.
	// 48 bytes

	int centroid_vert_index;

	uint32_t mat_index; // material index

	int child_quads_index; // Index of the first child (result of subdivision) quad of this quad.

	/*
	bits 0-3:  edge_uv_discontinuity for edge 0-3
	bits 4-7:  adjacent quad over edge has reversed orientation relative to this quad.
	bits 8-15: index (0-3) of edge of adjacent quad over edge, for each edge.
	bit 16:    quad is dead. A dead quad is a quad that does not need to be subdivided any more.
	*/
	BitField<uint32> bitfield;
};


typedef js::Vector<DUQuad, 64> DUQuadVector;
typedef js::Vector<DUVertex, 16> DUVertexVector;
typedef js::Vector<Vec2f, 16> UVVector;


struct Polygons
{
	js::Vector<DUQuad, 64> quads;
};


struct VertsAndUVs
{
	js::Vector<DUVertex, 16> verts;
	js::Vector<Vec2f, 16> uvs;
};


struct DUOptions
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	Matrix4f object_to_camera;

	bool view_dependent_subdivision;
	double pixel_height_at_dist_one;
	double subdivide_pixel_threshold;
	double subdivide_curvature_threshold;
	double displacement_error_threshold;
	unsigned int max_num_subdivisions;
	bool subdivision_smoothing;

	// This is the max number of smoothing/averaging passes. 
	// While smoothing/averaging passes are being done, all quads are subdivided, e.g. adaptive subdivision is not done.
	// This is because smoothing doesn't currently work with adaptive subdivision, as adjacency information can't be maintained
	// properly for adaptively subdivided meshes.
	unsigned int num_smoothings;

	std::vector<Planef> camera_clip_planes_os; // Camera clip planes in object space.
};


/*=====================================================================
DisplacementUtils
-----------------

=====================================================================*/
class DisplacementUtils
{
public:
	// Subdivide and displace the mesh.
	// All quads will be converted to triangles.
	// Sets verts_in_out.pos, .normal and .H.
	// Returns true if subdivision could be done, returns false if the mesh was invalid and subdivision could not be done.
	// Throws Indigo::CancelledException if cancelled.
	static bool subdivideAndDisplace(
		const std::string& mesh_name,
		glare::TaskManager& task_manager,
		PrintOutput& print_output,
		const ArrayRef<Reference<Material> >& materials,
		RayMesh::TriangleVectorType& tris_in_out,
		const RayMesh::QuadVectorType& quads_in,
		RayMesh::VertexVectorType& verts_in_out,
		RayMesh::UVVectorType& uvs_in_out,
		js::Vector<float, 16>& mean_curvature_out,
		unsigned int num_uv_sets,
		const DUOptions& options,
		bool use_shading_normals,
		const WorldParams& world_params,
		ShouldCancelCallback* should_cancel_callback
	);


	// Displace all vertices - updates verts_in_out.pos
	static void doDisplacementOnly(
		const std::string& mesh_name,
		glare::TaskManager& task_manager,
		PrintOutput& print_output,
		const ArrayRef<Reference<Material> >& materials,
		const RayMesh::TriangleVectorType& tris_in,
		const RayMesh::QuadVectorType& quads_in,
		RayMesh::VertexVectorType& verts_in_out,
		const RayMesh::UVVectorType& uvs_in,
		unsigned int num_uv_sets,
		const WorldParams& world_params
	);
	

private:
	static void initAndBuildAdjacencyInfo(
		const std::string& mesh_name,
		glare::TaskManager& task_manager,
		PrintOutput& print_output,
		const RayMesh::TriangleVectorType& triangles_in, 
		const RayMesh::QuadVectorType& quads_in,
		const RayMesh::VertexVectorType& vertices_in,
		const RayMesh::UVVectorType& uvs_in,
		unsigned int num_uv_sets,
		Polygons& temp_polygons_out,
		VertsAndUVs& temp_verts_uvs_out
	);


	static void displace(
		glare::TaskManager& task_manager,
		const ArrayRef<Reference<Material> >& materials,
		const DUQuadVector& quads,
		const DUVertexVector& verts_in,
		const UVVector& uvs,
		unsigned int num_uv_sets,
		bool compute_H,
		const WorldParams& world_params,
		DUVertexVector& verts_out
	);


	static void linearSubdivision(
		glare::TaskManager& task_manager,
		PrintOutput& print_output,
		const ArrayRef<Reference<Material> >& materials,
		Polygons& polygons_in,
		const VertsAndUVs& verts_and_uvs_in,
		unsigned int num_uv_sets,
		unsigned int num_subdivs_done,
		bool do_subdivision_smoothing,
		const DUOptions& options,
		const WorldParams& world_params,
		Polygons& polygons_out,
		VertsAndUVs& verts_and_uvs_out
	);


	static void splineSubdiv(
		glare::TaskManager& task_manager,
		PrintOutput& print_output,
		Polygons& polygons_in,
		const VertsAndUVs& verts_and_uvs_in,
		unsigned int num_uv_sets,
		Polygons& polygons_out,
		VertsAndUVs& verts_and_uvs_out
	);

	static void draw(const Polygons& polygons, const VertsAndUVs& verts_and_uvs, int num_uv_sets, const std::string& image_path);
};
