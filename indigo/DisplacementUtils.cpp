/*=====================================================================
DisplacementUtils.cpp
---------------------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Thu May 15 20:31:26 2008
=====================================================================*/
#include "DisplacementUtils.h"

/*
This code does Catmull-Clark subdivision, as well as displacement of the subdivided mesh.

http://www.cs.berkeley.edu/~sequin/CS284/PAPERS/CatmullClark_SDSurf.pdf
https://en.wikipedia.org/wiki/Catmull%E2%80%93Clark_subdivision_surface

The old subdivision code used
'A Factored Approach to Subdivision Surfaces', by Joe Warren and Scott Schaefer
http://www.cs.rice.edu/~jwarren/papers/subdivision_tutorial.pdf
*/


#include "ScalarMatParameter.h"
#include "Vec3MatParameter.h"
#include "VoidMedium.h"
#include "TestUtils.h"
#include "PrintOutput.h"
#include "StandardPrintOutput.h"
#include "ThreadContext.h"
#include "Diffuse.h"
#include "SpectrumMatParameter.h"
#include "DisplaceMatParameter.h"
#include "../maths/Rect2.h"
#include "../maths/mathstypes.h"
#include "../graphics/TriBoxIntersection.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/Task.h"
#include "../utils/ConPrint.h"
#include "../dll/include/IndigoMap.h"
#include "../utils/TaskManager.h"
#include "../utils/IndigoAtomic.h"
#include "../graphics/Drawing.h"
#include "../graphics/TextDrawer.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/imformatdecoder.h"
#include "../utils/HashMapInsertOnly2.h"
#include "../utils/BitUtils.h"
#include <unordered_map>


static const bool PROFILE = false;
//#define DISPLACEMENT_UTILS_STATS 1
#if DISPLACEMENT_UTILS_STATS
#define DISPLACEMENT_CREATE_TIMER(timer) Timer (timer)
#define DISPLACEMENT_RESET_TIMER(timer) ((timer).reset())
#define DISPLACEMENT_PRINT_RESULTS(expr) (expr)
#else
#define DISPLACEMENT_CREATE_TIMER(timer)
#define DISPLACEMENT_RESET_TIMER(timer)
#define DISPLACEMENT_PRINT_RESULTS(expr)
#endif


static const bool DRAW_SUBDIVISION_STEPS = false;


DisplacementUtils::DisplacementUtils()
{	
}


DisplacementUtils::~DisplacementUtils()
{	
}


static const uint32_t mod3_table[] = { 0, 1, 2, 0, 1, 2 };

inline static uint32 mod3(uint32 x)
{
	return mod3_table[x];
}

inline static uint32 mod4(uint32 x)
{
	return x & 0x3;
}


static inline const Vec3f triGeometricNormal(const DUVertexVector& verts, uint32_t v0, uint32_t v1, uint32_t v2)
{
	return normalise(crossProduct(verts[v1].pos - verts[v0].pos, verts[v2].pos - verts[v0].pos));
}


class DUVertIndexPair
{
public:
	DUVertIndexPair(){}
	DUVertIndexPair(unsigned int v_a_, unsigned int v_b_) : v_a(v_a_), v_b(v_b_) {}

	inline bool operator < (const DUVertIndexPair& b) const
	{
		if(v_a < b.v_a)
			return true;
		else if(v_a > b.v_a)
			return false;
		else	//else v_a = b.v_a
		{
			return v_b < b.v_b;
		}
	}

	inline bool operator == (const DUVertIndexPair& other) const
	{
		return v_a == other.v_a && v_b == other.v_b;
	}
	inline bool operator != (const DUVertIndexPair& other) const
	{
		return v_a != other.v_a || v_b != other.v_b;
	}

	unsigned int v_a, v_b;
};


/*static inline uint32_t pixelHash(uint32_t x)
{
	x  = (x ^ 12345391u) * 2654435769u;
	x ^= (x << 6) ^ (x >> 26);
	x *= 2654435769u;
	x += (x << 5) ^ (x >> 12);

	return x;
}*/


// Total subdiv and displace times on terrain_test.igs:
// Using just key.v_a:
// Time with identity hash: ~9.0s
// Time with std::hash<size_t>: 10.3s
// Time with pixelHash: 9.3s

// Time using both key.v_a and key.v_b:
// v_a XOR v_b: extremely slow
// Time with std::hash<unsigned int>: 10.73, 10.9, 10.87
// Time with _mm_crc32_u64: 10.08, 10.06

// Using Google dense hash map:
// Time with std::hash<unsigned int>: 9.06, 9.09, 9.07
// with _mm_crc32_u64: 8.62, 8.62, 8.59

// Using Google dense hash map: with resize first:
// Time with std::hash<unsigned int>: 8.47, 8.53, 8.58
// with _mm_crc32_u64: 8.14, 8.14, 8.17


// Modified from std::hash: from c:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\include\xstddef, renamed from _Hash_seq
// I copied this version here because the one from vs2010 (vs10) sucks serious balls, so use this one instead.
//#if defined(_WIN32)

static inline size_t use_Hash_seq(const unsigned char *_First, size_t _Count)
{	// FNV-1a hash function for bytes in [_First, _First+_Count)
	
	if(sizeof(size_t) == 8)
	{
		const size_t _FNV_offset_basis = 14695981039346656037ULL;
		const size_t _FNV_prime = 1099511628211ULL;

		size_t _Val = _FNV_offset_basis;
		for (size_t _Next = 0; _Next < _Count; ++_Next)
			{	// fold in another byte
			_Val ^= (size_t)_First[_Next];
			_Val *= _FNV_prime;
			}

		_Val ^= _Val >> 32;
		return _Val;
	}
	else
	{
		const size_t _FNV_offset_basis = 2166136261U;
		const size_t _FNV_prime = 16777619U;

		size_t _Val = _FNV_offset_basis;
		for (size_t _Next = 0; _Next < _Count; ++_Next)
			{	// fold in another byte
			_Val ^= (size_t)_First[_Next];
			_Val *= _FNV_prime;
			}

		return _Val;
	}
}


/*
static inline size_t use_Hash_seq(const unsigned char *_First, size_t _Count)
{	// FNV-1a hash function for bytes in [_First, _First+_Count)
	#ifdef _M_X64
	//static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
	const size_t _FNV_offset_basis = 14695981039346656037ULL;
	const size_t _FNV_prime = 1099511628211ULL;

	#else // _M_X64
	//static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
	const size_t _FNV_offset_basis = 2166136261U;
	const size_t _FNV_prime = 16777619U;
	#endif // _M_X64

	size_t _Val = _FNV_offset_basis;
	for (size_t _Next = 0; _Next < _Count; ++_Next)
		{	// fold in another byte
		_Val ^= (size_t)_First[_Next];
		_Val *= _FNV_prime;
		}

	#ifdef _M_X64
	//static_assert(sizeof(size_t) == 8, "This code is for 64-bit size_t.");
	_Val ^= _Val >> 32;

	#else // _M_X64
	//static_assert(sizeof(size_t) == 4, "This code is for 32-bit size_t.");
	#endif // _M_X64

	return (_Val);
}*/


template <class T>
static inline size_t useHash(const T& t)
{
	return use_Hash_seq((const unsigned char*)&t, sizeof(T));
}

//#endif // _WIN32


class DUVertIndexPairHash
{	// hash functor
public:
	inline size_t operator()(const DUVertIndexPair& key) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform
		
		//return (size_t)(key.v_a ^ key.v_b);
		//return (size_t)(pixelHash(key.v_a);

		/*const uint64 crc = 0x000011115555AAAA;

		unsigned __int64 res = _mm_crc32_u64(crc, key.v_a);
		res =  _mm_crc32_u64(res, key.v_b);
		return res;*/

//#if defined(_WIN32)
		return useHash(key.v_a) ^ useHash(key.v_b);
//#else
//		std::tr1::hash<unsigned int> h;
//		return h(key.v_a) ^ h(key.v_b);
//#endif
	}
};


static inline const Vec2f& getUVs(const std::vector<Vec2f>& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


static inline const Vec2f& getUVs(const UVVector& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


//static inline uint32_t uvIndex(uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
//{
//	return uv_index * num_uv_sets + set_index;
//}


static inline Vec2f& getUVs(UVVector& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


static void computeVertexNormals(const DUQuadVector& quads,
								 DUVertexVector& vertices)
{
	// Initialise vertex normals to null vector
	for(size_t i = 0; i < vertices.size(); ++i) vertices[i].normal = Vec3f(0, 0, 0);

	// Add quad face normals to constituent vertices' normals
	for(size_t q = 0; q < quads.size(); ++q)
	{
		const Vec3f quad_normal = triGeometricNormal(vertices, quads[q].vertex_indices[0], quads[q].vertex_indices[1], quads[q].vertex_indices[2]);

		const int num_sides = quads[q].numSides();
		for(int i = 0; i < num_sides; ++i)
			vertices[quads[q].vertex_indices[i]].normal += quad_normal;
	}

	for(size_t i = 0; i < vertices.size(); ++i)
		vertices[i].normal.normalise();
}


// Hash function for RayMeshVertex
class RayMeshVertexHash
{
public:
	inline size_t operator()(const RayMeshVertex& v) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform
		const float sum = v.pos.x + v.pos.y + v.pos.z;
		uint32 i;
		std::memcpy(&i, &sum, 4);
		return i;
	}
};


// Hash table from RayMeshVertex to new vertex index pair
//typedef std::unordered_map<RayMeshVertex, unsigned int, RayMeshVertexHash> VertToIndexMap;

struct VertKey
{
	Vec3f pos;
	Vec2f uv;
	Vec3f normal;

	inline bool operator == (const VertKey& other) const { return pos == other.pos && uv == other.uv && normal == other.normal; }
};


class VertKeyHash
{
public:
	inline size_t operator()(const VertKey& v) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform
		const float sum = v.pos.x + v.pos.y + v.pos.z;
		uint32 i;
		std::memcpy(&i, &sum, 4);
		return i;
	}
};


//typedef std::unordered_map<RayMeshVertex, unsigned int, RayMeshVertexHash> VertToIndexMap;
typedef std::unordered_map<VertKey, unsigned int, VertKeyHash> VertToIndexMap;


struct EdgeKey
{
	Vec3f start;
	Vec3f end;

	inline bool operator == (const EdgeKey& other) const { return start == other.start && end == other.end; }
};


class EdgeKeyHash
{
public:
	inline size_t operator()(const EdgeKey& v) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform
		const float sum = v.start.x + v.start.y + v.start.z;
		uint32 i;
		std::memcpy(&i, &sum, 4);
		return i;
	}
};


struct EdgeInfo
{
	EdgeInfo() : /*num_adjacent_polys(0), *//*uv_discontinuity(false), */adjacent_quad_a(-1)/*, adjacent_quad_b(-1)*/ {}
	//EdgeInfo(/*const DUVertIndexPair& old_edge_, */int num_adjacent_polys_) : /*old_edge(old_edge_), num_adjacent_polys(num_adjacent_polys_), uv_discontinuity(false), */adjacent_quad_a(-1), adjacent_quad_b(-1) {}

	//DUVertIndexPair old_edge;
	//int num_adjacent_polys;

	Vec2f start_uv;
	Vec2f end_uv;
	//bool uv_discontinuity;

	Vec3f start_normal;
	Vec3f end_normal;

	int adjacent_quad_a;
	//int adjacent_quad_b;
	int adj_a_side_i;
	//int adj_b_side_i;
	int num_quads_adjacent_to_edge;
	bool quad_a_flipped;
};


struct DUScratchInfo
{
	//EdgeInfoMapType edge_info_map;
};


struct QuadInfo
{
	QuadInfo(){}
	QuadInfo(int i_, bool r) : i(i_), reversed(r) {}
	int i;
	bool reversed;
};


bool DisplacementUtils::subdivideAndDisplace(
	const std::string& mesh_name,
	Indigo::TaskManager& task_manager,
	PrintOutput& print_output,
	ThreadContext& context,
	const std::vector<Reference<Material> >& materials,
	bool subdivision_smoothing,
	RayMesh::TriangleVectorType& triangles_in_out, 
	const RayMesh::QuadVectorType& quads_in,
	RayMesh::VertexVectorType& vertices_in_out,
	std::vector<Vec2f>& uvs_in_out,
	unsigned int num_uv_sets,
	const DUOptions& options,
	bool use_shading_normals
	)
{
	if(PROFILE) conPrint("\n-----------subdivideAndDisplace-----------");
	if(PROFILE) conPrint("mesh: " + mesh_name);

	Timer total_timer; // This one is used to create a message that is always printed.
	DISPLACEMENT_CREATE_TIMER(timer);

	const RayMesh::TriangleVectorType& triangles_in = triangles_in_out;
	const RayMesh::VertexVectorType& vertices_in = vertices_in_out;
	const std::vector<Vec2f>& uvs_in = uvs_in_out;


	// We will maintain two sets of geometry information, and 'ping-pong' between them.
	Polygons temp_polygons_1;
	Polygons temp_polygons_2;

	VertsAndUVs temp_verts_uvs_1;
	VertsAndUVs temp_verts_uvs_2;
	

	// Map from DUVertex to index in temp_verts
	//std::map<DUVertex, int> du_vert_indices;

	// Copying incoming data to temp_polygons_1, temp_verts_uvs_1.
	{ 
		js::Vector<DUQuad, 64>& temp_quads = temp_polygons_1.quads;
		js::Vector<DUVertex, 16>& temp_verts = temp_verts_uvs_1.verts;
		js::Vector<Vec2f, 16>& temp_uvs = temp_verts_uvs_1.uvs;

		const float UV_DIST2_THRESHOLD = 0.001f * 0.001f;

		std::unordered_map<EdgeKey, EdgeInfo, EdgeKeyHash> edges;

		temp_quads.resize(triangles_in.size() + quads_in.size());
		temp_verts.reserve(vertices_in.size());
		temp_uvs.reserve(uvs_in.size());

		// Build adjacency info.
		// OLD: Two polygons will be considered adjacent if they share a common edge, where an edge is defined as an ordered pair of vertex indices.
		// Two polygons will be considered adjacent if they share a common edge, where an edge is defined as an ordered pair of vertex positions

		//============== Process tris ===============

		const size_t triangles_in_size = triangles_in.size();
		for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
		{
			const RayMeshTriangle& tri_in = triangles_in[t];

			temp_quads[t].bitfield = BitField<uint32>(0);
			temp_quads[t].adjacent_quad_index[3] = -1;

			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const unsigned int i1 = mod3(i + 1); // Next vert
				const uint32 vi  = tri_in.vertex_indices[i];
				const uint32 vi1 = tri_in.vertex_indices[i1];
			
				// Get UVs at start and end of this edge, for this tri.
				Vec2f start_uv(0.f);
				Vec2f end_uv(0.f);
				if(num_uv_sets > 0)
				{
					const Vec2f uv_i  = getUVs(uvs_in, num_uv_sets, tri_in.uv_indices[i], /*uv_set=*/0);
					const Vec2f uv_i1 = getUVs(uvs_in, num_uv_sets, tri_in.uv_indices[i1], /*uv_set=*/0);
					if(vi < vi1) 
						{ start_uv = uv_i; end_uv = uv_i1; } 
					else 
						{ start_uv = uv_i1; end_uv = uv_i; }
				}

				Vec3f start_normal;
				Vec3f end_normal;
				{
					const Vec3f normal_i  = vertices_in[vi ].normal;
					const Vec3f normal_i1 = vertices_in[vi1].normal;
					if(vi < vi1) 
						{ start_normal = normal_i; end_normal = normal_i1; } 
					else 
						{ start_normal = normal_i1; end_normal = normal_i; }
				}
				
				//DUVertIndexPair edge_key(myMin(vi, vi1), myMax(vi, vi1));
				bool flipped;
				EdgeKey edge_key;
				const Vec3f vi_pos  = vertices_in[vi].pos;
				const Vec3f vi1_pos = vertices_in[vi1].pos;
				if(vi_pos < vi1_pos)
					{ edge_key.start = vi_pos;  edge_key.end = vi1_pos; flipped = false; }
				else
					{ edge_key.start = vi1_pos; edge_key.end = vi_pos;  flipped = true; }

				std::unordered_map<EdgeKey, EdgeInfo, EdgeKeyHash>::iterator result = edges.find(edge_key); // Lookup edge
				if(result == edges.end()) // If edge not added yet:
				{
					EdgeInfo edge_info;
					//edge_info.num_adjacent_polys = 1;
					edge_info.start_uv = start_uv;
					edge_info.end_uv = end_uv;
					edge_info.start_normal = start_normal;
					edge_info.end_normal = end_normal;
					edge_info.quad_a_flipped = flipped;

					assert(edge_info.adjacent_quad_a == -1);
					edge_info.adjacent_quad_a = (int)t; // Since this edge is just created, it won't have any adjacent quads marked yet.
					edge_info.adj_a_side_i = i;
					edge_info.num_quads_adjacent_to_edge = 1;

					edges.insert(std::make_pair(edge_key, edge_info)); // Add edge

					temp_quads[t].adjacent_quad_index[i] = -1;
				}
				else
				{
					EdgeInfo& edge_info = result->second;

					if(edge_info.num_quads_adjacent_to_edge > 1)
					{
						//print_output.print("Can't subdivide mesh '" + mesh_name + "': More than 2 polygons share a single edge.");
						//return false;
						temp_quads[t].adjacent_quad_index[i] = -1;
					}
					else
					{
						/*if(result->second.old_edge != old_edge) // If adjacent triangle used different edge vertices then there is a discontinuity, unless the vert shading normals are the same.
						{
							// Get edges sorted by vertex position, compare normals at the ends of the edges.
							const DUVertIndexPair& old_edge2 = result->second.old_edge;

							const Vec3f edge_1_smaller_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_a].normal : vertices_in[old_edge.v_b].normal;
							const Vec3f edge_1_greater_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_b].normal : vertices_in[old_edge.v_a].normal;

							const Vec3f edge_2_smaller_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_a].normal : vertices_in[old_edge2.v_b].normal;
							const Vec3f edge_2_greater_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_b].normal : vertices_in[old_edge2.v_a].normal;

							if((edge_1_smaller_n != edge_2_smaller_n) || (edge_1_greater_n != edge_2_greater_n)) // If shading normals are different on the old edge vertices:
							{
								//TEMP temp_edges.push_back(DUEdge(tri_new_vert_indices[i], tri_new_vert_indices[i1], temp_tris[t].uv_indices[i], temp_tris[t].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
								// TODO Work this out temp_edges.back().adjacent_quad_index = //
							}
						}*/

						// We know another poly is adjacent to this edge already.
						assert(result->second.adjacent_quad_a != -1);
						DUQuad& adj_quad = temp_quads[edge_info.adjacent_quad_a];

						// Check for uv discontinuity:
						if((num_uv_sets > 0) && ((start_uv.getDist2(edge_info.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(edge_info.end_uv) > UV_DIST2_THRESHOLD)))
						{
							//result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity
							temp_quads[t].setUVEdgeDiscontinuityTrue(i); // Mark edge of polygon
							adj_quad.setUVEdgeDiscontinuityTrue(edge_info.adj_a_side_i); // Mark edge of adjacent polygon
						}

						// Check for shading normal discontinuity.
						if(start_normal != edge_info.start_normal || end_normal != edge_info.end_normal)
						{
							temp_quads[t].setUVEdgeDiscontinuityTrue(i); // Mark edge of polygon
							adj_quad.setUVEdgeDiscontinuityTrue(edge_info.adj_a_side_i); // Mark edge of adjacent polygon
						}
						

						//edge_info.num_adjacent_polys++;

						// Mark this quad as the 'b' adjacent quad.
						//edge_info.adjacent_quad_b = (int)t;
						//edge_info.adj_b_side_i = i;

						temp_quads[t].adjacent_quad_index[i] = edge_info.adjacent_quad_a;
						temp_quads[t].setAdjacentQuadEdgeIndex(i, edge_info.adj_a_side_i);
					
						// Mark this quad as adjacent to the other quad.
						adj_quad.adjacent_quad_index[edge_info.adj_a_side_i] = (int)t;
						adj_quad.setAdjacentQuadEdgeIndex(edge_info.adj_a_side_i, i);

						if(edge_info.quad_a_flipped == flipped)
						{
							// Both quads have same orientation along edge.  THis means the surface is not orientable.  we need to flip this quad.
							//conPrint("================!!!!!!!!!Surface orientation differs!");
							temp_quads[t].setOrienReversedTrue(i);
							adj_quad.setOrienReversedTrue(edge_info.adj_a_side_i);
						}

						edge_info.num_quads_adjacent_to_edge++;
					}
				}
			}

			temp_quads[t].mat_index = tri_in.getTriMatIndex();
			temp_quads[t].dead = false;
			for(int v=0; v<4; ++v)
				temp_quads[t].edge_midpoint_vert_index[v] = -1;
		}

		


		//============== Process quads ===============

		// Build adjacency info
		const size_t quads_in_size = quads_in.size();
		for(size_t q = 0; q < quads_in_size; ++q)
		{
			const RayMeshQuad& quad_in = quads_in[q];

			const int new_quad_index = (int)(q + triangles_in_size);
			DUQuad& quad_out = temp_quads[new_quad_index];
			quad_out.bitfield = BitField<uint32>(0);

			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const unsigned int i1 = mod4(i + 1); // Next vert
				const uint32 vi  = quad_in.vertex_indices[i];
				const uint32 vi1 = quad_in.vertex_indices[i1];
			
				// Get UVs at start and end of this edge, for this tri.
				Vec2f start_uv(0.f);
				Vec2f end_uv(0.f);
				if(num_uv_sets > 0)
				{
					const Vec2f uv_i  = getUVs(uvs_in, num_uv_sets, quad_in.uv_indices[i] , /*uv_set=*/0);
					const Vec2f uv_i1 = getUVs(uvs_in, num_uv_sets, quad_in.uv_indices[i1], /*uv_set=*/0);
					if(vi < vi1)
						{ start_uv = uv_i; end_uv = uv_i1; } 
					else 
						{ start_uv = uv_i1; end_uv = uv_i; }
				}

				Vec3f start_normal;
				Vec3f end_normal;
				{
					const Vec3f normal_i  = vertices_in[vi ].normal;
					const Vec3f normal_i1 = vertices_in[vi1].normal;
					if(vi < vi1) 
						{ start_normal = normal_i; end_normal = normal_i1; } 
					else 
						{ start_normal = normal_i1; end_normal = normal_i; }
				}
				
				//DUVertIndexPair edge_key(myMin(vi, vi1), myMax(vi, vi1));
				bool flipped;
				EdgeKey edge_key;
				const Vec3f vi_pos  = vertices_in[vi].pos;
				const Vec3f vi1_pos = vertices_in[vi1].pos;
				if(vi_pos < vi1_pos)
					{ edge_key.start = vi_pos;  edge_key.end = vi1_pos; flipped = false; }
				else
					{ edge_key.start = vi1_pos; edge_key.end = vi_pos;  flipped = true; }

				std::unordered_map<EdgeKey, EdgeInfo, EdgeKeyHash>::iterator result = edges.find(edge_key); // Lookup edge
				if(result == edges.end()) // If edge not added yet:
				{
					EdgeInfo edge_info;
					//edge_info.num_adjacent_polys = 1;
					edge_info.start_uv = start_uv;
					edge_info.end_uv = end_uv;
					edge_info.start_normal = start_normal;
					edge_info.end_normal = end_normal;
					edge_info.quad_a_flipped = flipped;

					assert(edge_info.adjacent_quad_a == -1);
					edge_info.adjacent_quad_a = (int)new_quad_index; // Since this edge is just created, it won't have any adjacent quads marked yet.
					edge_info.adj_a_side_i = i;
					edge_info.num_quads_adjacent_to_edge = 1;

					edges.insert(std::make_pair(edge_key, edge_info)); // Add edge

					quad_out.adjacent_quad_index[i] = -1;
				}
				else
				{
					EdgeInfo& edge_info = result->second;

					if(edge_info.num_quads_adjacent_to_edge > 1)
					{
						//print_output.print("Can't subdivide mesh '" + mesh_name + "': More than 2 polygons share a single edge.");
						//return false;
						quad_out.adjacent_quad_index[i] = -1;
					}
					else
					{
						/*if(result->second.old_edge != old_edge) // If adjacent triangle used different edge vertices then there is a discontinuity, unless the vert shading normals are the same.
						{
							// Get edges sorted by vertex position, compare normals at the ends of the edges.
							const DUVertIndexPair& old_edge2 = result->second.old_edge;

							const Vec3f edge_1_smaller_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_a].normal : vertices_in[old_edge.v_b].normal;
							const Vec3f edge_1_greater_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_b].normal : vertices_in[old_edge.v_a].normal;

							const Vec3f edge_2_smaller_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_a].normal : vertices_in[old_edge2.v_b].normal;
							const Vec3f edge_2_greater_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_b].normal : vertices_in[old_edge2.v_a].normal;

							if((edge_1_smaller_n != edge_2_smaller_n) || (edge_1_greater_n != edge_2_greater_n)) // If shading normals are different on the old edge vertices:
							{
								//TEMP temp_edges.push_back(DUEdge(tri_new_vert_indices[i], tri_new_vert_indices[i1], temp_tris[t].uv_indices[i], temp_tris[t].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
								// TODO Work this out temp_edges.back().adjacent_quad_index = //
							}
						}*/

						// We know another poly is adjacent to this edge already.
						assert(result->second.adjacent_quad_a != -1);
						DUQuad& adj_quad = temp_quads[edge_info.adjacent_quad_a];

						// Check for uv discontinuity:
						if((num_uv_sets > 0) && ((start_uv.getDist2(edge_info.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(edge_info.end_uv) > UV_DIST2_THRESHOLD)))
						{
							//result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity
							quad_out.setUVEdgeDiscontinuityTrue(i); // Mark edge of polygon
							adj_quad.setUVEdgeDiscontinuityTrue(edge_info.adj_a_side_i); // Mark edge of adjacent polygon
						}


						// Check for shading normal discontinuity.
						if(start_normal != edge_info.start_normal || end_normal != edge_info.end_normal)
						{
							quad_out.setUVEdgeDiscontinuityTrue(i); // Mark edge of polygon
							adj_quad.setUVEdgeDiscontinuityTrue(edge_info.adj_a_side_i); // Mark edge of adjacent polygon
						}


						//edge_info.num_adjacent_polys++;

						// Mark this quad as the 'b' adjacent quad.
						//edge_info.adjacent_quad_b = new_quad_index;
						//edge_info.adj_b_side_i = i;

						quad_out.adjacent_quad_index[i] = edge_info.adjacent_quad_a;
						quad_out.setAdjacentQuadEdgeIndex(i, edge_info.adj_a_side_i);
					
						// Mark this quad as adjacent to the other quad.
						adj_quad.adjacent_quad_index[edge_info.adj_a_side_i] = new_quad_index;
						adj_quad.setAdjacentQuadEdgeIndex(edge_info.adj_a_side_i, i);

						if(edge_info.quad_a_flipped == flipped)
						{
							// Both quads have same orientation along edge.  THis means the surface is not orientable.  we need to flip this quad.
							// conPrint("================!!!!!!!!!Surface orientation differs!");
							quad_out.setOrienReversedTrue(i);
							adj_quad.setOrienReversedTrue(edge_info.adj_a_side_i);
						}

						edge_info.num_quads_adjacent_to_edge++;
					}
				}
			}

			quad_out.mat_index = quad_in.getMatIndex();
			quad_out.dead = false;
			for(int v=0; v<4; ++v)
				quad_out.edge_midpoint_vert_index[v] = -1;
		}



		// Do a traversal over the mesh to work out any quads we should reverse.
		// We will do this by doing a traversal over each connected component of the mesh.
		// After we are done all quads in each connected component will have a consistent winding order.

		// TODO: Show an error message or abort subdivision if we detect an inconsistent winding that we can't fix? (e.g. a moebius twist)
		const int temp_quads_size = (int)temp_quads.size();
		std::vector<bool> initial_poly_reversed(temp_quads_size, false);
		std::vector<bool> processed(temp_quads_size, false);
		int first_unprocessed_i = 0;
		
		std::vector<QuadInfo> stack;
		stack.reserve(1000);

		while(1)
		{
			// Increment first_unprocessed_i until we find the first unprocessed quad.
			while(processed[first_unprocessed_i])
			{
				first_unprocessed_i++;
				if(first_unprocessed_i >= temp_quads_size) // If all quads are processed, we are done.
					goto done;
			}
		
			// Traverse until we have processed all quads in this connected component of the mesh.
			bool reverse = false;
			assert(stack.empty());
			stack.push_back(QuadInfo(first_unprocessed_i, reverse));
			while(!stack.empty())
			{
				const QuadInfo quad_info = stack.back();
				stack.pop_back();
				const int q = quad_info.i;
				const bool q_reversed = quad_info.reversed;

				initial_poly_reversed[q] = q_reversed;
				processed[q] = true;

				// Add adjacent quad on each edge to stack
				for(int v=0; v<4; ++v)
				{
					const int adj_quad_i = temp_quads[q].adjacent_quad_index[v];
					if(adj_quad_i != -1 && !processed[adj_quad_i]) // If there is an adjacent quad, and we haven't already processed it:
					{
						bool adj_reversed = q_reversed;
						if(temp_quads[q].isOrienReversed(v) != 0) // If the adjacent quad needs to have its orientation flipped:
							adj_reversed = !adj_reversed;

						stack.push_back(QuadInfo(adj_quad_i, adj_reversed));
					}
				}
			}
		}
done:	1;




		// Create vertices for each input triangle.  Each vertex should have a unique (position, uv).
		VertToIndexMap new_vert_indices;
		for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
		{
			const RayMeshTriangle& tri_in = triangles_in[t];
			const bool reverse = initial_poly_reversed[t];

			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const unsigned int src_i = reverse ? 2 - i : i;
				const Vec3f pos = vertices_in[tri_in.vertex_indices[src_i]].pos;
				const Vec2f uv0 = num_uv_sets == 0 ? Vec2f(0.f) : getUVs(uvs_in, num_uv_sets, tri_in.uv_indices[src_i], /*uv_set=*/0);

				VertKey key;  key.pos = pos;  key.uv = uv0;  key.normal = vertices_in[tri_in.vertex_indices[src_i]].normal;
				VertToIndexMap::iterator res = new_vert_indices.find(key);
				unsigned int new_vert_index;;
				if(res == new_vert_indices.end())
				{
					new_vert_index = (unsigned int)temp_verts.size();
					temp_verts.push_back(DUVertex(pos, vertices_in[tri_in.vertex_indices[src_i]].normal));
					for(uint32 z=0; z<num_uv_sets; ++z)
						temp_uvs.push_back(getUVs(uvs_in, num_uv_sets, tri_in.uv_indices[src_i], z));
					
					new_vert_indices.insert(std::make_pair(key, new_vert_index));
				}
				else
					new_vert_index = res->second; // Use existing vertex index.
				
				temp_quads[t].vertex_indices[i] = new_vert_index;

				// Mark the vertex as having a UV discontinuity.  NOTE: should these be using src_i?
				if(temp_quads[t].isUVEdgeDiscontinuity(i) != 0 || temp_quads[t].isUVEdgeDiscontinuity(mod3(i + 2)) != 0)
					temp_verts[new_vert_index].uv_discontinuity = true;
			}

			temp_quads[t].vertex_indices[3] = std::numeric_limits<uint32>::max();

			/*
			v2____e1_____v1             v0____e0_____v1
			|            /              |            /
			|          /                |          /
			|e2      /        =>        |e2      /
			|      /e0                  |      /e1
			|    /                      |    /
			|  /                        |  /
			v0                          v2
			*/

			// if we have reversed the vertex order, have to swap edge info as well.  Edge 0 and 1 swap, edge 2 stays the same.
			if(reverse)
			{
				mySwap(temp_quads[t].adjacent_quad_index[0], temp_quads[t].adjacent_quad_index[1]); // Swap adjacent_quad_index
				const int e0_uv_disc = temp_quads[t].getUVEdgeDiscontinuity(0); // Swap UVEdgeDiscontinuity
				const int e1_uv_disc = temp_quads[t].getUVEdgeDiscontinuity(1);
				temp_quads[t].setUVEdgeDiscontinuity(0, e1_uv_disc);
				temp_quads[t].setUVEdgeDiscontinuity(1, e0_uv_disc);

				// Sawp adjacent quad edge indices
				const uint32 e0_adj_quad_edge_i = temp_quads[t].getAdjacentQuadEdgeIndex(0);
				const uint32 e1_adj_quad_edge_i = temp_quads[t].getAdjacentQuadEdgeIndex(1);
				temp_quads[t].setAdjacentQuadEdgeIndex(0, e1_adj_quad_edge_i);
				temp_quads[t].setAdjacentQuadEdgeIndex(1, e0_adj_quad_edge_i);
			}

			// If adjacent polygon over edge is reversed, then we will need to update our adjacent quad edge indices.
			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const int adj_poly_i = temp_quads[t].adjacent_quad_index[i];
				if(adj_poly_i != -1 && initial_poly_reversed[adj_poly_i])
				{
					const uint32 adj_quad_edge_i = temp_quads[t].getAdjacentQuadEdgeIndex(i);
					uint32 new_adj_quad_edge_i;
					if(adj_poly_i < triangles_in_size) // temp_quads[adj_poly_i].numSides() == 3) // If adjacent poly is a triangle, edges 0 and 1 swap.
					{
						if(adj_quad_edge_i == 0)
							new_adj_quad_edge_i = 1;
						else if(adj_quad_edge_i == 1)
							new_adj_quad_edge_i = 0;
						else
							new_adj_quad_edge_i = 2;
					}
					else // else if adjacent poly is a quad, edges 0 and 2 swap:
					{
						if(adj_quad_edge_i == 0)
							new_adj_quad_edge_i = 2;
						else if(adj_quad_edge_i == 2)
							new_adj_quad_edge_i = 0;
						else
							new_adj_quad_edge_i = adj_quad_edge_i;
					}
					temp_quads[t].setAdjacentQuadEdgeIndex(i, new_adj_quad_edge_i);
				}
			}
		}

		// Create vertices for each input quad.  Each vertex should have a unique (position, uv).
		for(size_t q = 0; q < quads_in_size; ++q)
		{
			const RayMeshQuad& quad_in = quads_in[q];
			const int new_quad_index = (int)(q + triangles_in_size);
			const bool reverse = initial_poly_reversed[new_quad_index];

			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const unsigned int src_i = reverse ? 3 - i : i;
				const Vec3f pos = vertices_in[quad_in.vertex_indices[src_i]].pos;
				const Vec2f uv0 = num_uv_sets == 0 ? Vec2f(0.f) : getUVs(uvs_in, num_uv_sets, quad_in.uv_indices[src_i], /*uv_set=*/0);

				VertKey key;  key.pos = pos;  key.uv = uv0;  key.normal = vertices_in[quad_in.vertex_indices[src_i]].normal;
				VertToIndexMap::iterator res = new_vert_indices.find(key);
				unsigned int new_vert_index;
				if(res == new_vert_indices.end())
				{
					new_vert_index = (unsigned int)temp_verts.size();
					temp_verts.push_back(DUVertex(pos, vertices_in[quad_in.vertex_indices[src_i]].normal));
					for(uint32 z=0; z<num_uv_sets; ++z)
						temp_uvs.push_back(getUVs(uvs_in, num_uv_sets, quad_in.uv_indices[src_i], z));
					
					new_vert_indices.insert(std::make_pair(key, new_vert_index));
				}
				else
					new_vert_index = res->second; // Use existing vertex index.
				
				temp_quads[new_quad_index].vertex_indices[i] = new_vert_index;

				// Mark the vertex as having a UV discontinuity.
				if(temp_quads[new_quad_index].isUVEdgeDiscontinuity(i) != 0 || temp_quads[new_quad_index].isUVEdgeDiscontinuity(mod4(i + 3)) != 0)
					temp_verts[new_vert_index].uv_discontinuity = true;
			}

			// If we have reversed the vertex order, have to swap edge info as well.  Edge 0 and 2 swap, edges 0 and 3 stay the same.
			if(reverse)
			{
				mySwap(temp_quads[new_quad_index].adjacent_quad_index[0], temp_quads[new_quad_index].adjacent_quad_index[2]); // Swap adjacent_quad_index
				const int e0_uv_disc = temp_quads[new_quad_index].getUVEdgeDiscontinuity(0); // Swap UVEdgeDiscontinuity
				const int e2_uv_disc = temp_quads[new_quad_index].getUVEdgeDiscontinuity(2);
				temp_quads[new_quad_index].setUVEdgeDiscontinuity(0, e2_uv_disc);
				temp_quads[new_quad_index].setUVEdgeDiscontinuity(2, e0_uv_disc);

				const uint32 e0_adj_quad_edge_i = temp_quads[new_quad_index].getAdjacentQuadEdgeIndex(0);
				const uint32 e2_adj_quad_edge_i = temp_quads[new_quad_index].getAdjacentQuadEdgeIndex(2);
				temp_quads[new_quad_index].setAdjacentQuadEdgeIndex(0, e2_adj_quad_edge_i);
				temp_quads[new_quad_index].setAdjacentQuadEdgeIndex(2, e0_adj_quad_edge_i);
			}

			// If adjacent polygon over edge is reversed, then we will need to update our adjacent quad edge indices.
			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const int adj_poly_i = temp_quads[new_quad_index].adjacent_quad_index[i];
				if(adj_poly_i != -1 && initial_poly_reversed[adj_poly_i])
				{
					const uint32 adj_quad_edge_i = temp_quads[new_quad_index].getAdjacentQuadEdgeIndex(i);
					uint32 new_adj_quad_edge_i;
					if(adj_poly_i < triangles_in_size) // temp_quads[adj_poly_i].numSides() == 3) // If adjacent poly is a triangle, edges 0 and 1 swap.
					{
						if(adj_quad_edge_i == 0)
							new_adj_quad_edge_i = 1;
						else if(adj_quad_edge_i == 1)
							new_adj_quad_edge_i = 0;
						else
							new_adj_quad_edge_i = 2;
					}
					else // else if adjacent poly is a quad, edges 0 and 2 swap:
					{
						assert(adj_quad_edge_i >= 0 && adj_quad_edge_i < 4);
						if(adj_quad_edge_i == 0)
							new_adj_quad_edge_i = 2;
						else if(adj_quad_edge_i == 2)
							new_adj_quad_edge_i = 0;
						else
							new_adj_quad_edge_i = adj_quad_edge_i;
					}
					temp_quads[new_quad_index].setAdjacentQuadEdgeIndex(i, new_adj_quad_edge_i);
				}
			}
		}



		// Sanity check quads
#ifndef NDEBUG
		for(size_t q=0; q<temp_quads.size(); ++q)
		{
			const DUQuad& quad = temp_quads[q];
			for(int v=0; v<4; ++v)
			{
				const int adjacent_quad_i = quad.adjacent_quad_index[v];
				if(adjacent_quad_i != -1)
				{
					assert(adjacent_quad_i >= 0 && adjacent_quad_i < temp_quads.size());
					const DUQuad& adj_quad = temp_quads[adjacent_quad_i];
					
					// Check that the adjacent quad does indeed have this quad as its adjacent quad along the given edge.
					const int adjacent_quads_edge = quad.getAdjacentQuadEdgeIndex(v);
					assert(adjacent_quads_edge >= 0 && adjacent_quads_edge < adj_quad.numSides());
					assert(adj_quad.adjacent_quad_index[adjacent_quads_edge] == q);
				}
			}
		}
#endif


		// Build adjacent_quad_index
		//for(size_t q = 0; q < temp_quads.size(); ++q) // For each quad
		//{
		//	DUQuad& quad = temp_quads[q];
		//	const int num_sides = quad.isTri() ? 3 : 4;
		//	const unsigned int vert_i[4] = { quad.vertex_indices[0], quad.vertex_indices[1], quad.vertex_indices[2], quad.vertex_indices[3] };

		//	quad.adjacent_quad_index[3] = -1;

		//	for(int i = 0; i < num_sides; ++i) // For each edge
		//	{
		//		const unsigned int i1 = num_sides == 3 ? mod3(i + 1) : mod4(i + 1); // Next vert
		//		const DUVertIndexPair edge_key(myMin(vert_i[i], vert_i[i1]), myMax(vert_i[i], vert_i[i1]));

		//		const std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = edges.find(edge_key);
		//		assert(result != edges.end());
		//		const EdgeInfo& edge_info = result->second;

		//		quad.edge_uv_discontinuity[i] = result->second.uv_discontinuity;

		//		// At this point the edge should have recorded the two quads adjacent to it.
		//		if(edge_info.adjacent_quad_a == q) // If this quad is quad 'a'.
		//			quad.adjacent_quad_index[i] = edge_info.adjacent_quad_b; // Then this quad is adjacent to quad 'b'.
		//		else if(edge_info.adjacent_quad_b == q) // If this quad is quad 'b'.
		//			quad.adjacent_quad_index[i] = edge_info.adjacent_quad_a; // Then this quad is adjacent to quad 'a'.
		//		else
		//		{
		//			assert(false);
		//		}
		//	}
		//}
#if 0
		//temp_uvs.resize((temp_polygons_1.tris.size() * 3 + temp_polygons_1.quads.size() * 4) * num_uv_sets);
		//uint32 temp_uvs_next_i = 0; // Next index in temp_uvs to write to.
		//uint32 new_uv_index = 0; // = temp_uvs_next_i / num_uv_sets


		

		// Map from new edge (smaller new vert index, larger new vert index) to first old edge: (smaller old vert index, larger old vert index).
		// If there are different old edges, then this edge is a discontinuity.

		std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash> new_edge_info;


		VertToIndexMap new_vert_indices;


		const size_t triangles_in_size = triangles_in.size();
		for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
		{
			const RayMeshTriangle& tri = triangles_in[t];

			// Explode UVs
			/*for(unsigned int i = 0; i < 3; ++i)
			{
				// Create new UV
				for(unsigned int z = 0; z < num_uv_sets; ++z)
					temp_uvs[temp_uvs_next_i++] = getUVs(uvs_in, num_uv_sets, tri.uv_indices[i], z);
			
				// Set new tri UV index
				temp_tris[t].uv_indices[i] = new_uv_index;
				new_uv_index++;
			}*/

			unsigned int tri_new_vert_indices[3];

			for(unsigned int i = 0; i < 3; ++i)
			{
				const unsigned int old_vert_index = tri.vertex_indices[i];
				const RayMeshVertex& old_vert = vertices_in[old_vert_index]; // Get old vertex

				const VertToIndexMap::const_iterator result = new_vert_indices.find(old_vert); // See if it has already been added to map

				unsigned int new_vert_index;
				if(result == new_vert_indices.end()) // If not found:
				{
					// Add new vertex index to map with old vertex as key.
					new_vert_index = (unsigned int)temp_verts.size();
					temp_verts.push_back(DUVertex(old_vert.pos, old_vert.normal));
					temp_verts.back().uv = uvs_in[tri.uv_indices[i]];//NEW
					
					new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
				}
				else
					new_vert_index = (*result).second; // Use existing vertex index.

				temp_quads[t].vertex_indices[i] = new_vert_index;
				tri_new_vert_indices[i] = new_vert_index;
			}

			temp_quads[t].vertex_indices[3] = std::numeric_limits<uint32>::max();


			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const unsigned int i1 = mod3(i + 1); // Next vert
			
				// Get UVs at start and end of this edge, for this tri.
				Vec2f start_uv(0.f);
				Vec2f end_uv(0.f);
				if(num_uv_sets > 0)
				{
					const Vec2f uv_i = getUVs(uvs_in, num_uv_sets, tri.uv_indices[i], /*uv_set=*/0);
					const Vec2f uv_i1 = getUVs(uvs_in, num_uv_sets, tri.uv_indices[i1], /*uv_set=*/0);
					if(tri_new_vert_indices[i] < tri_new_vert_indices[i1])
					{
						start_uv = uv_i;
						end_uv = uv_i1;
					}
					else
					{
						start_uv = uv_i1;
						end_uv = uv_i;
					}
				}

				DUVertIndexPair edge_key(myMin(tri_new_vert_indices[i], tri_new_vert_indices[i1]), myMax(tri_new_vert_indices[i], tri_new_vert_indices[i1]));
				DUVertIndexPair old_edge(myMin(tri.vertex_indices[i], tri.vertex_indices[i1]), myMax(tri.vertex_indices[i], tri.vertex_indices[i1]));

				std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = new_edge_info.find(edge_key); // Lookup edge
				if(result == new_edge_info.end()) // If edge not added yet:
				{
					EdgeInfo edge_info(old_edge, /*num_adjacent_polys=*/1);
					edge_info.start_uv = start_uv;
					edge_info.end_uv = end_uv;

					assert(edge_info.adjacent_quad_a == -1);
					edge_info.adjacent_quad_a = (int)t; // Since this edge is just created, it won't have any adjacent quads marked yet.

					new_edge_info.insert(std::make_pair(edge_key, edge_info)); // Add edge
				}
				else
				{
					if(result->second.old_edge != old_edge) // If adjacent triangle used different edge vertices then there is a discontinuity, unless the vert shading normals are the same.
					{
						// Get edges sorted by vertex position, compare normals at the ends of the edges.
						const DUVertIndexPair& old_edge2 = result->second.old_edge;

						const Vec3f edge_1_smaller_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_a].normal : vertices_in[old_edge.v_b].normal;
						const Vec3f edge_1_greater_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_b].normal : vertices_in[old_edge.v_a].normal;

						const Vec3f edge_2_smaller_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_a].normal : vertices_in[old_edge2.v_b].normal;
						const Vec3f edge_2_greater_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_b].normal : vertices_in[old_edge2.v_a].normal;

						if((edge_1_smaller_n != edge_2_smaller_n) || (edge_1_greater_n != edge_2_greater_n)) // If shading normals are different on the old edge vertices:
						{
							//TEMP temp_edges.push_back(DUEdge(tri_new_vert_indices[i], tri_new_vert_indices[i1], temp_tris[t].uv_indices[i], temp_tris[t].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
							// TODO Work this out temp_edges.back().adjacent_quad_index = //
						}
					}

					// Check for uv discontinuity:
					if((num_uv_sets > 0) && ((start_uv.getDist2(result->second.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(result->second.end_uv) > UV_DIST2_THRESHOLD)))
						result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity

					result->second.num_adjacent_polys++;

					assert(result->second.adjacent_quad_a != -1);
					result->second.adjacent_quad_b = (int)t;
				}
			}

			temp_quads[t].mat_index = tri.getTriMatIndex();
			temp_quads[t].dead = false;

			for(int v=0; v<4; ++v)
				temp_quads[t].edge_midpoint_vert_index[v] = -1;
		}


		const size_t quads_in_size = quads_in.size();
		for(size_t q = 0; q < quads_in_size; ++q) // For each quad
		{
			const RayMeshQuad& quad = quads_in[q];
			const int new_quad_index = (int)(q + triangles_in_size);
			DUQuad& quad_out = temp_quads[new_quad_index];

			// Explode UVs
			/*for(unsigned int i = 0; i < 4; ++i)
			{
				// Create new UV
				for(unsigned int z = 0; z < num_uv_sets; ++z)
					temp_uvs[temp_uvs_next_i++] = getUVs(uvs_in, num_uv_sets, quad.uv_indices[i], z);
			
				// Set new quad UV index
			//	temp_quads[q].uv_indices[i] = new_uv_index;
				new_uv_index++;
			}*/

			unsigned int quad_new_vert_indices[4];

			for(unsigned int i = 0; i < 4; ++i)
			{
				const unsigned int old_vert_index = quad.vertex_indices[i];
				const RayMeshVertex& old_vert = vertices_in[old_vert_index]; // Get old vertex

				const VertToIndexMap::const_iterator result = new_vert_indices.find(old_vert); // See if it has already been added to map

				unsigned int new_vert_index;
				if(result == new_vert_indices.end()) // If not found:
				{
					// Add new vertex index to map with old vertex as key.
					new_vert_index = (unsigned int)temp_verts.size();
					temp_verts.push_back(DUVertex(old_vert.pos, old_vert.normal));
					temp_verts.back().uv = uvs_in[quad.uv_indices[i]];//NEW
					new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
				}
				else
					new_vert_index = (*result).second; // Use existing vertex index.

				quad_out.vertex_indices[i] = new_vert_index;
				quad_new_vert_indices[i] = new_vert_index;
			}

			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const unsigned int i1 = mod4(i + 1); // Next vert

				// Get UVs at start and end of this edge, for this quad.
				Vec2f start_uv(0.f);
				Vec2f end_uv(0.f);
				if(num_uv_sets > 0)
				{
					const Vec2f uv_i = getUVs(uvs_in, num_uv_sets, quad.uv_indices[i], /*uv_set=*/0);
					const Vec2f uv_i1 = getUVs(uvs_in, num_uv_sets, quad.uv_indices[i1], /*uv_set=*/0);
					if(quad_new_vert_indices[i] < quad_new_vert_indices[i1])
					{
						start_uv = uv_i;
						end_uv = uv_i1;
					}
					else
					{
						start_uv = uv_i1;
						end_uv = uv_i;
					}
				}

				DUVertIndexPair edge_key(myMin(quad_new_vert_indices[i], quad_new_vert_indices[i1]), myMax(quad_new_vert_indices[i], quad_new_vert_indices[i1]));
				DUVertIndexPair old_edge(myMin(quad.vertex_indices[i], quad.vertex_indices[i1]), myMax(quad.vertex_indices[i], quad.vertex_indices[i1]));

				std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = new_edge_info.find(edge_key);
				if(result == new_edge_info.end()) // If edge not added yet:
				{
					EdgeInfo edge_info(old_edge, /*num_adjacent_polys=*/1);
					edge_info.start_uv = start_uv;
					edge_info.end_uv = end_uv;

					//TEMP NEW:
					// Record in the edge that this quad is adjacent to it.
					assert(edge_info.adjacent_quad_a == -1);
					edge_info.adjacent_quad_a = (int)new_quad_index; // Since this edge is just created, it won't have any adjacent quads marked yet.

					new_edge_info.insert(std::make_pair(edge_key, edge_info));
				}
				else
				{
					if(result->second.old_edge != old_edge) // If adjacent polygon used different edge vertices then there is a discontinuity.
					{
						// Get edges sorted by vertex position, compare normals at the ends of the edges.
						const DUVertIndexPair& old_edge2 = result->second.old_edge;

						const Vec3f edge_1_smaller_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_a].normal : vertices_in[old_edge.v_b].normal;
						const Vec3f edge_1_greater_n = vertices_in[old_edge.v_a].pos < vertices_in[old_edge.v_b].pos ? vertices_in[old_edge.v_b].normal : vertices_in[old_edge.v_a].normal;

						const Vec3f edge_2_smaller_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_a].normal : vertices_in[old_edge2.v_b].normal;
						const Vec3f edge_2_greater_n = vertices_in[old_edge2.v_a].pos < vertices_in[old_edge2.v_b].pos ? vertices_in[old_edge2.v_b].normal : vertices_in[old_edge2.v_a].normal;

						//if((edge_1_smaller_n != edge_2_smaller_n) || (edge_1_greater_n != edge_2_greater_n)) // If shading normals are different on the old edge vertices:
						//	temp_edges.push_back(DUEdge(quad_new_vert_indices[i], quad_new_vert_indices[i1], temp_quads[q].uv_indices[i], temp_quads[q].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
					}

					// Check for uv discontinuity:
					if((num_uv_sets > 0) && ((start_uv.getDist2(result->second.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(result->second.end_uv) > UV_DIST2_THRESHOLD)))
						result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity

					result->second.num_adjacent_polys++;

					assert(result->second.adjacent_quad_a != -1);
					result->second.adjacent_quad_b = (int)new_quad_index;
				}
			}

			quad_out.mat_index = quad.getMatIndex();
			quad_out.dead = false;

			for(int v=0; v<4; ++v)
				quad_out.edge_midpoint_vert_index[v] = -1;
		}

		DISPLACEMENT_PRINT_RESULTS(conPrint("Creating temp verts/tris/quads: " + timer.elapsedStringNPlaces(5)));
		DISPLACEMENT_RESET_TIMER(timer);

		//========================== Create Boundary edge polygons, mark UV discontinuities ==========================
		// Now that we have marked all edges that have UV discontinuities, do a pass over the (temp) primitives to put that information in the primitives themselves.
		// Create boundary edge polygons while we are doing this as well.

		for(size_t q = 0; q < temp_quads.size(); ++q) // For each quad
		{
			DUQuad& quad = temp_quads[q];
			const int num_sides = quad.isTri() ? 3 : 4;
			const unsigned int vert_i[4] = { quad.vertex_indices[0], quad.vertex_indices[1], quad.vertex_indices[2], quad.vertex_indices[3] };

			quad.adjacent_quad_index[3] = -1;

			for(int i = 0; i < num_sides; ++i) // For each edge
			{
				const unsigned int i1 = num_sides == 3 ? mod3(i + 1) : mod4(i + 1); // Next vert
				const DUVertIndexPair edge_key(myMin(vert_i[i], vert_i[i1]), myMax(vert_i[i], vert_i[i1]));

				const std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = new_edge_info.find(edge_key);
				assert(result != new_edge_info.end());

				const EdgeInfo& edge_info = result->second;
				if(result->second.uv_discontinuity)
				{
					//conPrint("!!! Quad " + toString(q) + " has UV disc. on edge " + toString(i));

					// Add new vertices, that have the correct vert pos and UVs for this quad on this edge.
					DUVertex& current_v_i = temp_verts[vert_i[i]];
					temp_verts.push_back(current_v_i);
					if(q < triangles_in_size)
						temp_verts.back().uv = uvs_in[triangles_in[q].uv_indices[i]];// get correct UV
					else
						temp_verts.back().uv = uvs_in[quads_in[q - triangles_in_size].uv_indices[i]];// get correct UV

					// Update vert index for this quad
					quad.vertex_indices[i] = (uint32)temp_verts.size() - 1;

					// Copy next vert around quad as well
					DUVertex& current_v_i1 = temp_verts[vert_i[i1]];
					temp_verts.push_back(current_v_i1);
					if(q < triangles_in_size)
						temp_verts.back().uv = uvs_in[triangles_in[q].uv_indices[i1]];// get correct UV
					else
						temp_verts.back().uv = uvs_in[quads_in[q - triangles_in_size].uv_indices[i1]];// get correct UV

					// Update vert index for this quad
					quad.vertex_indices[i1] = (uint32)temp_verts.size() - 1;
				}

				quad.edge_uv_discontinuity[i] = result->second.uv_discontinuity;

				if(edge_info.num_adjacent_polys == 1) // If this is a boundary edge (has only 1 adjacent poly):
				{
					//temp_edges.push_back(DUEdge(quad.vertex_indices[i], quad.vertex_indices[i1], quad.uv_indices[i], quad.uv_indices[i1]));
					//temp_edges.back().adjacent_quad_index = (int)q;
				}

				// At this point the edge should have recorded the two quads adjacent to it.
				if(edge_info.adjacent_quad_a == q) // If this quad is quad 'a'.
					quad.adjacent_quad_index[i] = edge_info.adjacent_quad_b; // Then this quad is adjacent to quad 'b'.
				else if(edge_info.adjacent_quad_b == q) // If this quad is quad 'b'.
					quad.adjacent_quad_index[i] = edge_info.adjacent_quad_a; // Then this quad is adjacent to quad 'a'.
				else
				{
					assert(false);
				}
			}
		}
#endif
		//assert(temp_uvs_next_i == (uint32)temp_uvs.size());

		DISPLACEMENT_PRINT_RESULTS(conPrint("Create Boundary edge polygons, mark UV discontinuities: " + timer.elapsedStringNPlaces(5)));
		DISPLACEMENT_RESET_TIMER(timer);


		// Count number of edges adjacent to each vertex.
		// If the number of edges adjacent to the vertex is > 2, then make this a fixed vertex.
		// TEMP TODO
		/*std::vector<unsigned int> num_edges_adjacent(temp_verts.size(), 0);

		for(size_t i=0; i<temp_edges.size(); ++i)
		{
			num_edges_adjacent[temp_edges[i].vertex_indices[0]]++;
			num_edges_adjacent[temp_edges[i].vertex_indices[1]]++;
		}

		for(size_t i=0; i<num_edges_adjacent.size(); ++i)
		{
			if(num_edges_adjacent[i] > 2)
			{
				temp_vert_polygons.push_back(DUVertexPolygon());
				temp_vert_polygons.back().vertex_index = (uint32)i;
				temp_vert_polygons.back().uv_index = (uint32)i;
			}
		}*/

	} // End init

	//conPrint("num fixed verts created: " + toString(temp_vert_polygons.size()));

	if(DRAW_SUBDIVISION_STEPS) draw(temp_polygons_1, temp_verts_uvs_1, num_uv_sets, "initial_mesh.png");

	DISPLACEMENT_PRINT_RESULTS(conPrint("Adding vertex polygons: " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	DUScratchInfo scratch_info;

	if(PROFILE)
	{
		conPrint("---Subdiv options---");
		printVar(subdivision_smoothing);
		printVar(options.displacement_error_threshold);
		printVar(options.max_num_subdivisions);
		printVar(options.num_smoothings);
		printVar(options.view_dependent_subdivision);
		printVar(options.pixel_height_at_dist_one);
		printVar(options.subdivide_curvature_threshold);
		printVar(options.subdivide_pixel_threshold);
	}

	Polygons* current_polygons = &temp_polygons_1;
	Polygons* next_polygons    = &temp_polygons_2;

	VertsAndUVs* current_verts_and_uvs = &temp_verts_uvs_1;
	VertsAndUVs* next_verts_and_uvs    = &temp_verts_uvs_2;

	if(false)
	{
		splineSubdiv(
			task_manager,
			print_output,
			context,
			*current_polygons,
			*current_verts_and_uvs,
			num_uv_sets,
			*next_polygons, // polygons_out
			*next_verts_and_uvs // verts_and_uvs_out
		);

		mySwap(next_polygons, current_polygons);
		mySwap(next_verts_and_uvs, current_verts_and_uvs);
	}
	else
	{


	// Loop pre and post-condition: The most recent information is in *current_polygons, *current_verts_and_uvs.
	for(uint32_t i = 0; i < options.max_num_subdivisions; ++i)
	{
		print_output.print("\tSubdividing '" + mesh_name + "', level " + toString(i) + "...");

		DISPLACEMENT_CREATE_TIMER(linear_timer);
		if(PROFILE) conPrint("\nDoing linearSubdivision for mesh '" + mesh_name + "', level " + toString(i) + "...");
		
		linearSubdivision(
			task_manager,
			print_output,
			context,
			materials,
			*current_polygons, // polygons_in
			*current_verts_and_uvs, // verts_and_uvs_in
			num_uv_sets,
			i, // num subdivs done
			subdivision_smoothing && (i < options.num_smoothings), // do subdivision smoothing
			options,
			scratch_info,
			*next_polygons, // polygons_out
			*next_verts_and_uvs // verts_and_uvs_out
		);

		// Updated information is now in *next_polygons, *next_verts_and_uvs

		if(DRAW_SUBDIVISION_STEPS) draw(*next_polygons, *next_verts_and_uvs, num_uv_sets, "after_linear_subd_" + toString(i) + ".png");

		DISPLACEMENT_PRINT_RESULTS(conPrint("linearSubdivision took            " + linear_timer.elapsedStringNPlaces(5) + "\n"));

		// Recompute vertex normals.  (Updates *next_verts_and_uvs)
		//DISPLACEMENT_CREATE_TIMER(recompute_vertex_normals);
		//computeVertexNormals(next_polygons->quads, next_verts_and_uvs->verts);
		//DISPLACEMENT_PRINT_RESULTS(conPrint("computeVertexNormals took " + recompute_vertex_normals.elapsedString()));

		print_output.print("\t\tresulting num vertices: " + toString((unsigned int)next_verts_and_uvs->verts.size()));
		print_output.print("\t\tresulting num quads: " + toString((unsigned int)next_polygons->quads.size()));
		print_output.print("\t\tDone.");

		// Swap next_polygons, current_polygons etc..
		mySwap(next_polygons, current_polygons);
		mySwap(next_verts_and_uvs, current_verts_and_uvs);

		// The most recently updated information is now in current_polygons, current_verts_and_uvs
	}

	}

	// Apply the final displacement
	DISPLACEMENT_RESET_TIMER(timer);
	displace(
		task_manager,
		context,
		materials,
		current_polygons->quads, // quads in
		current_verts_and_uvs->verts, // verts in
		current_verts_and_uvs->uvs, // uvs in
		num_uv_sets,
		next_verts_and_uvs->verts // verts out
	);
	DISPLACEMENT_PRINT_RESULTS(conPrint("final displace took               " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	const RayMesh_ShadingNormals use_s_n = use_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals;

	DUQuadVector& temp_quads = current_polygons->quads;
	DUVertexVector& temp_verts = next_verts_and_uvs->verts;

	// Build triangles_in_out from temp_quads
	const size_t temp_quads_size = temp_quads.size();
	triangles_in_out.resizeUninitialised(temp_quads_size * 2); // Pre-allocate space

	DISPLACEMENT_PRINT_RESULTS(conPrint("tris_out alloc took               " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);
	
	size_t tri_write_i = 0;
	for(size_t i = 0; i < temp_quads_size; ++i)
	{
		assert(temp_quads[i].isQuad());

		// Split the quad into two triangles
		RayMeshTriangle& tri_a = triangles_in_out[tri_write_i];

		tri_a.vertex_indices[0] = temp_quads[i].vertex_indices[0];
		tri_a.vertex_indices[1] = temp_quads[i].vertex_indices[1];
		tri_a.vertex_indices[2] = temp_quads[i].vertex_indices[2];

		tri_a.uv_indices[0] = temp_quads[i].vertex_indices[0];
		tri_a.uv_indices[1] = temp_quads[i].vertex_indices[1];
		tri_a.uv_indices[2] = temp_quads[i].vertex_indices[2];

		tri_a.setTriMatIndex(temp_quads[i].mat_index);
		tri_a.setUseShadingNormals(use_s_n);

		RayMeshTriangle& tri_b = triangles_in_out[tri_write_i + 1];

		tri_b.vertex_indices[0] = temp_quads[i].vertex_indices[0];
		tri_b.vertex_indices[1] = temp_quads[i].vertex_indices[2];
		tri_b.vertex_indices[2] = temp_quads[i].vertex_indices[3];

		tri_b.uv_indices[0] = temp_quads[i].vertex_indices[0];
		tri_b.uv_indices[1] = temp_quads[i].vertex_indices[2];
		tri_b.uv_indices[2] = temp_quads[i].vertex_indices[3];

		tri_b.setTriMatIndex(temp_quads[i].mat_index);
		tri_b.setUseShadingNormals(use_s_n);

		tri_write_i += 2;
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("Writing to tris_out took          " + timer.elapsedStringNPlaces(5)));


	// Recompute all vertex normals, as they will be completely wrong by now due to any displacement.
	DISPLACEMENT_RESET_TIMER(timer);
	computeVertexNormals(temp_quads, temp_verts);
	DISPLACEMENT_PRINT_RESULTS(conPrint("final computeVertexNormals() took " + timer.elapsedStringNPlaces(5)));


	// Convert DUVertex's back into RayMeshVertex and store in vertices_in_out.
	DISPLACEMENT_RESET_TIMER(timer);
	const size_t temp_verts_size = temp_verts.size();
	vertices_in_out.resizeUninitialised(temp_verts_size);
	for(size_t i = 0; i < temp_verts_size; ++i)
		vertices_in_out[i] = RayMeshVertex(temp_verts[i].pos, temp_verts[i].normal,
			0 // H - mean curvature - just set to zero, we will recompute it later.
		);

	uvs_in_out.resize(current_verts_and_uvs->uvs.size());
	if(!current_verts_and_uvs->uvs.empty())
		std::memcpy(&uvs_in_out[0], &current_verts_and_uvs->uvs[0], current_verts_and_uvs->uvs.size() * sizeof(Vec2f));

	DISPLACEMENT_PRINT_RESULTS(conPrint("creating verts_out and uvs_out:   " + timer.elapsedStringNPlaces(5)));

	print_output.print("Subdivision and displacement took " + total_timer.elapsedStringNPlaces(5));
	DISPLACEMENT_PRINT_RESULTS(conPrint("Total time elapsed:               " + total_timer.elapsedStringNPlaces(5)));
	return true;
}


struct RayMeshDisplaceTaskClosure
{
	const RayMesh::TriangleVectorType* tris_in;
	const RayMesh::QuadVectorType* quads_in;
	unsigned int num_uv_sets;
	const std::vector<Vec2f>* uvs_in;
	RayMesh::VertexVectorType* verts;
	const std::vector<Reference<Material> >* materials;
	std::vector<IndigoAtomic>* verts_processed;
	js::Vector<float, 16>* displacements_out;
};


class RayMeshTriDisplaceTask : public Indigo::Task
{
public:
	RayMeshTriDisplaceTask(const RayMeshDisplaceTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		ThreadContext context;
		DUUVCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.num_uvs = closure.num_uv_sets;
		const RayMesh::TriangleVectorType& tris = *closure.tris_in;
		const int num_uv_sets = closure.num_uv_sets;
		const std::vector<Vec2f>& uvs = *closure.uvs_in;
		RayMesh::VertexVectorType& verts = *closure.verts;
		std::vector<IndigoAtomic>& verts_processed = *closure.verts_processed;
		js::Vector<float, 16>& displacements_out = *closure.displacements_out;

		for(int t_i = begin; t_i < end; ++t_i)
		{
			const RayMeshTriangle& tri = tris[t_i];
			const Material* const material = (*closure.materials)[tri.getTriMatIndex()].getPointer();
			for(int i=0; i<4; ++i)
			{
				const uint32 v_i = tri.vertex_indices[i];
				if(verts_processed[v_i].increment() == 0) // If this vert has not been processed yet:
				{
					if(material->displacing())
					{
						HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));
						for(int z = 0; z < num_uv_sets; ++z)
							du_texcoord_evaluator.uvs[z] = getUVs(uvs, num_uv_sets, tri.vertex_indices[i], z);
						du_texcoord_evaluator.pos_os = verts[v_i].pos.toVec4fPoint();

						EvalDisplaceArgs args(
							context,
							hitinfo,
							du_texcoord_evaluator,
							Vec4f(0), // dp_dalpha  TEMP HACK
							Vec4f(0), // dp_dbeta  TEMP HACK
							Vec4f(0,0,1,0) // pre-bump N_s_ws TEMP HACK
						);

						const float displacement = material->evaluateDisplacement(args);
						displacements_out[v_i] = displacement;
					}
				}
			}
		}
	}

	const RayMeshDisplaceTaskClosure& closure;
	int begin, end;
};


class RayMeshQuadDisplaceTask : public Indigo::Task
{
public:
	RayMeshQuadDisplaceTask(const RayMeshDisplaceTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		ThreadContext context;
		DUUVCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.num_uvs = closure.num_uv_sets;
		const RayMesh::QuadVectorType& quads = *closure.quads_in;
		const int num_uv_sets = closure.num_uv_sets;
		const std::vector<Vec2f>& uvs = *closure.uvs_in;
		RayMesh::VertexVectorType& verts = *closure.verts;
		std::vector<IndigoAtomic>& verts_processed = *closure.verts_processed;
		js::Vector<float, 16>& displacements_out = *closure.displacements_out;

		for(int q_i = begin; q_i < end; ++q_i)
		{
			const RayMeshQuad& quad = quads[q_i];
			const Material* const material = (*closure.materials)[quad.getMatIndex()].getPointer();
			for(int i=0; i<4; ++i)
			{
				const uint32 v_i = quad.vertex_indices[i];
				
				if(verts_processed[v_i].increment() == 0) // If this vert has not been processed yet:
				{
					if(material->displacing())
					{
						HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));
						for(int z = 0; z < num_uv_sets; ++z)
							du_texcoord_evaluator.uvs[z] = getUVs(uvs, num_uv_sets, quad.vertex_indices[i], z);
						du_texcoord_evaluator.pos_os = verts[v_i].pos.toVec4fPoint();

						EvalDisplaceArgs args(
							context,
							hitinfo,
							du_texcoord_evaluator,
							Vec4f(0), // dp_dalpha  TEMP HACK
							Vec4f(0), // dp_dbeta  TEMP HACK
							Vec4f(0,0,1,0) // pre-bump N_s_ws TEMP HACK
						);

						const float displacement = material->evaluateDisplacement(args);
						displacements_out[v_i] = displacement;
					}
				}
			}
		}
	}

	const RayMeshDisplaceTaskClosure& closure;
	int begin, end;
};


class Vec3fHash
{
public:
	inline size_t operator()(const Vec3f& v) const
	{
		return useHash(bitCast<uint32>(v.x)) ^ useHash(bitCast<uint32>(v.y)) ^ useHash(bitCast<uint32>(v.z));
	}
};


struct VertDisplacement
{
	Vec3f sum_displacement;
	int num;
};


// Displace all vertices - updates verts_in_out.pos
void DisplacementUtils::doDisplacementOnly(
		const std::string& mesh_name,
		Indigo::TaskManager& task_manager,
		PrintOutput& print_output,
		ThreadContext& context,
		const std::vector<Reference<Material> >& materials,
		const RayMesh::TriangleVectorType& tris_in,
		const RayMesh::QuadVectorType& quads_in,
		RayMesh::VertexVectorType& verts_in_out,
		const std::vector<Vec2f>& uvs_in,
		unsigned int num_uv_sets
	)
{
	if(PROFILE) conPrint("\n-----------doDisplacementOnly-----------");
	if(PROFILE) conPrint("mesh: " + mesh_name);
	DISPLACEMENT_CREATE_TIMER(timer);

	// Do a pass over the mesh to compute the displacement for each RayMeshVertex

	std::vector<IndigoAtomic> verts_processed(verts_in_out.size());
	js::Vector<float, 16> displacements(verts_in_out.size());

	RayMeshDisplaceTaskClosure closure;
	closure.tris_in = &tris_in;
	closure.quads_in = &quads_in;
	closure.num_uv_sets = num_uv_sets;
	closure.uvs_in = &uvs_in;
	closure.verts = &verts_in_out;
	closure.materials = &materials;
	closure.displacements_out = &displacements;
	closure.verts_processed = &verts_processed;
	task_manager.runParallelForTasks<RayMeshTriDisplaceTask,  RayMeshDisplaceTaskClosure>(closure, 0, tris_in.size());
	task_manager.runParallelForTasks<RayMeshQuadDisplaceTask, RayMeshDisplaceTaskClosure>(closure, 0, quads_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("Computing vertex displacments took   " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	// To avoid breaks in meshes, we want to set the displacement of vertices to the average displacement of all vertices at the same posiiton.
	// NOTE: this is a bit slow (e.g. 0.3 s for 1M quads)
	// Not sure the slowness is a big issue though, as usually high poly counts will come from subdivision, which we handle displacement for differently.
	const Vec3f inf_v = Vec3f(std::numeric_limits<float>::infinity());
	HashMapInsertOnly2<Vec3f, VertDisplacement, Vec3fHash> vert_displacements(inf_v, 
		verts_in_out.size() // expected num items
	);

	for(size_t t = 0; t < tris_in.size(); ++t)
	{
		const RayMeshTriangle& tri = tris_in[t];
		if(materials[tri.getTriMatIndex()]->displacing())
		{
			for(int i = 0; i < 3; ++i)
			{
				const uint32 v_i = tri.vertex_indices[i];
				const Vec3f displacement_vec = verts_in_out[v_i].normal * displacements[v_i];
				auto it = vert_displacements.find(verts_in_out[v_i].pos);
				if(it == vert_displacements.end())
				{
					VertDisplacement d;
					d.sum_displacement = displacement_vec;
					d.num = 1;
					vert_displacements.insert(std::make_pair(verts_in_out[v_i].pos, d));
				}
				else
				{
					it->second.sum_displacement += displacement_vec;
					it->second.num++;
				}
			}
		}
	}
	for(size_t q = 0; q < quads_in.size(); ++q)
	{
		const RayMeshQuad& quad = quads_in[q];
		if(materials[quad.getMatIndex()]->displacing())
		{
			for(int i = 0; i < 4; ++i)
			{
				const uint32 v_i = quad.vertex_indices[i];
				const Vec3f displacement_vec = verts_in_out[v_i].normal * displacements[v_i];
				auto it = vert_displacements.find(verts_in_out[v_i].pos);
				if(it == vert_displacements.end())
				{
					VertDisplacement d;
					d.sum_displacement = displacement_vec;
					d.num = 1;
					vert_displacements.insert(std::make_pair(verts_in_out[v_i].pos, d));
				}
				else
				{
					it->second.sum_displacement += displacement_vec;
					it->second.num++;
				}
			}
		}
	}

	for(size_t v = 0; v < verts_in_out.size(); ++v)
	{
		auto it = vert_displacements.find(verts_in_out[v].pos);
		if(it != vert_displacements.end())
		{
			const Vec3f av_displacement_vec = it->second.sum_displacement / (float)it->second.num;
			verts_in_out[v].pos += av_displacement_vec;
		}
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("Computing average displacements took " + timer.elapsedStringNPlaces(5)));
}


/*
Apply displacement to the given vertices, storing the displaced vertices in verts_out
*/
struct EvalVertDisplaceMentTaskClosure
{
	const DUQuadVector* quads;
	unsigned int num_uv_sets;
	const UVVector* uvs;
	const DUVertexVector* verts_in;
	DUVertexVector* verts_out;
	const std::vector<Reference<Material> >* materials;
	std::vector<IndigoAtomic>* verts_processed;
};


// Evaluate the displacement at each vertex.
// Sets verts_out displacement and verts_out pos.
class EvalVertDisplaceMentTask : public Indigo::Task
{
public:
	EvalVertDisplaceMentTask(const EvalVertDisplaceMentTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		ThreadContext context;
		DUUVCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.num_uvs = closure.num_uv_sets;
		const DUQuadVector& quads = *closure.quads;
		const int num_uv_sets = closure.num_uv_sets;
		const UVVector& uvs = *closure.uvs;
		const DUVertexVector& verts_in = *closure.verts_in;
		DUVertexVector& verts_out = *closure.verts_out;
		std::vector<IndigoAtomic>& verts_processed = *closure.verts_processed;

		for(int q_i = begin; q_i < end; ++q_i)
		{
			const DUQuad& quad = quads[q_i];
			const Material* const material = (*closure.materials)[quad.mat_index].getPointer();
			const int num_sides = quad.numSides();
			for(int i=0; i<num_sides; ++i)
			{
				const uint32 v_i = quad.vertex_indices[i];
				
				if(verts_processed[v_i].increment() == 0) // If this vert has not been processed yet:
				{
					if(material->displacing())
					{
						HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));
						for(int z = 0; z < num_uv_sets; ++z)
							du_texcoord_evaluator.uvs[z] = getUVs(uvs, num_uv_sets, quad.vertex_indices[i], z);
						du_texcoord_evaluator.pos_os = verts_in[v_i].pos.toVec4fPoint();

						EvalDisplaceArgs args(
							context,
							hitinfo,
							du_texcoord_evaluator,
							Vec4f(0), // dp_dalpha  TEMP HACK
							Vec4f(0), // dp_dbeta  TEMP HACK
							Vec4f(0,0,1,0) // pre-bump N_s_ws TEMP HACK
						);

						const float displacement = material->evaluateDisplacement(args);
						verts_out[v_i].displacement = displacement;
						verts_out[v_i].pos = verts_in[v_i].pos + normalise(verts_in[v_i].normal) * displacement;
					}
					else
					{
						verts_out[v_i].displacement = 0; // Applied material is not displacing.
						verts_out[v_i].pos = verts_in[v_i].pos;
					}
				}
			}
		}
	}

	const EvalVertDisplaceMentTaskClosure& closure;
	int begin, end;
};


void DisplacementUtils::displace(Indigo::TaskManager& task_manager,
								 ThreadContext& context,
								 const std::vector<Reference<Material> >& materials,
								 const DUQuadVector& quads,
								 const DUVertexVector& verts_in,
								 const UVVector& uvs,
								 unsigned int num_uv_sets,
								 DUVertexVector& verts_out
								 )
{
	DISPLACEMENT_CREATE_TIMER(timer);
	verts_out = verts_in;
	DISPLACEMENT_PRINT_RESULTS(conPrint("verts_out = verts_in:                        " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	std::vector<IndigoAtomic> verts_processed(verts_out.size());

	EvalVertDisplaceMentTaskClosure closure;
	closure.uvs = &uvs;
	closure.verts_processed = &verts_processed;
	closure.materials = &materials;
	closure.quads = &quads;
	closure.num_uv_sets = num_uv_sets;
	closure.verts_in = &verts_in;
	closure.verts_out = &verts_out;

	//Indigo::TaskManager dummy_task_manager(1);//TEMP

	// Evaluate the displacement at each vertex.  Sets verts_out displacement and verts_out pos.
	task_manager.runParallelForTasks<EvalVertDisplaceMentTask, EvalVertDisplaceMentTaskClosure>(closure, 0, quads.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("Running EvalVertDisplaceMentTask:            " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	// Special processing for vertices on shading or uv discontinuities.
	// To avoid breaks in meshes, we want to set the displacement of such vertices to the average displacement of all vertices at the same posiiton.
	const Vec3f inf_v = Vec3f(std::numeric_limits<float>::infinity());
	HashMapInsertOnly2<Vec3f, VertDisplacement, Vec3fHash> vert_displacements(inf_v);

	DUUVCoordEvaluator du_texcoord_evaluator;
	du_texcoord_evaluator.num_uvs = num_uv_sets;

	for(size_t q = 0; q < quads.size(); ++q)
	{
		const DUQuad& quad = quads[q];
		if(materials[quad.mat_index]->displacing())
		{
			const int num_sides = quad.numSides();
			for(int i = 0; i < num_sides; ++i)
			{
				const uint32 v_i = quads[q].vertex_indices[i];
				if(verts_in[v_i].uv_discontinuity)
				{
					const Vec3f displacement_vec = verts_out[v_i].pos - verts_in[v_i].pos;
					auto it = vert_displacements.find(verts_in[v_i].pos);
					if(it == vert_displacements.end())
					{
						VertDisplacement d;
						d.sum_displacement = displacement_vec;
						d.num = 1;
						vert_displacements.insert(std::make_pair(verts_in[v_i].pos, d));
					}
					else
					{
						it->second.sum_displacement += displacement_vec;
						it->second.num++;
					}
				}
			}
		}
	}
	for(size_t v = 0; v < verts_out.size(); ++v)
	{
		if(verts_in[v].uv_discontinuity)
		{
			auto it = vert_displacements.find(verts_in[v].pos);
			if(it != vert_displacements.end())
			{
				const Vec3f av_displacement_vec = it->second.sum_displacement / (float)it->second.num;
				verts_out[v].pos = verts_in[v].pos + av_displacement_vec;
				verts_out[v].displacement = av_displacement_vec.length();
			}
			else
				verts_out[v].displacement = 0;
		}

		// If any vertex is anchored, then set its position to the average of its 'parent' vertices
		if(verts_out[v].anchored)
			verts_out[v].pos = (verts_out[verts_out[v].adjacent_vert_0].pos + verts_out[verts_out[v].adjacent_vert_1].pos) * 0.5f;
	}
	DISPLACEMENT_PRINT_RESULTS(conPrint("Special vert processing:                     " + timer.elapsedStringNPlaces(5)));
}


inline static const Vec2f screenSpacePosForCameraSpacePos(const Vec3f& cam_space_pos)
{
	float recip_y = 1 / cam_space_pos.y;
	return Vec2f(cam_space_pos.x  * recip_y, cam_space_pos.z * recip_y);
}


// Heuristic for getting a reasonable sampling rate, when computing the max displacement error across a quad.
// The constant below (512) was chosen as the smallest value that allowed adaptive_tri_tex_shader_displacement_test.igs to displace without major artifacts.
// This heuristic won't be used if the texture space footprint of the quad can be determined. (e.g. in the TextureDisplaceMatParameter case)
static int getDispErrorSamplingResForQuads(size_t num_quads)
{
	const float side_res = std::sqrt((float)num_quads); // assuming uniform subidivsion

	const float res = 512 / side_res;

	const int final_res = myMax(4, (int)res);

	return final_res;

	/*
	side_res	=>  res
	-------------------
	1			=>	512
	2			=>	256
	4			=>	128
	8			=>	64
	16			=>	32
	32			=>  16
	etc..
	*/
}


// Determine curvature >= curvature_threshold
inline static bool isQuadCurvatureGrEqThreshold(const DUVertexVector& displaced_in_verts, const DUQuad& quad_in, float c_threshold, float dot_threshold)
{
	// Define a measure of curvature c = max angle (in radians) between quad vertex normals.
	// Therefore maximum possible c is pi.  Maximum reasonable c is pi/2.
	// let d = cos(c)  (e.g. min dot product between quad vert normals)
	// d ranges from 0 to 1 for reasonable quads.
	// c >= c_thresh   <=>  d <= d_thresh

	// If subdivide_curvature_threshold is <= 0, then since quad_curvature is always >= 0, then we will always subdivide the quad, so avoid computing the curvature.
	if(c_threshold <= 0)
		return true;

	// If c_threshold is greater than any resonable c can be, don't compute c, just return false.
	if(c_threshold > Maths::pi_2<float>())
		return false;

	const Vec3f nnorm0 = normalise(displaced_in_verts[quad_in.vertex_indices[0]].normal);
	const Vec3f nnorm1 = normalise(displaced_in_verts[quad_in.vertex_indices[1]].normal);
	const Vec3f nnorm2 = normalise(displaced_in_verts[quad_in.vertex_indices[2]].normal);
	const Vec3f nnorm3 = normalise(displaced_in_verts[quad_in.vertex_indices[3]].normal);

	const float dot_0 = dot(nnorm0, nnorm1);
	const float dot_1 = dot(nnorm1, nnorm2);
	const float dot_2 = dot(nnorm2, nnorm3);
	const float dot_3 = dot(nnorm3, nnorm0);

	const float mindot = myMin(myMin(dot_0, dot_1), myMin(dot_2, dot_3));

	return mindot <= dot_threshold;
}


struct BuildSubdividingPrimitiveTaskClosure
{
	const DUOptions* options;
	int num_subdivs_done;
	int min_num_subdivisions;
	unsigned int num_uv_sets;
	js::Vector<int, 16>* subdividing_quad;

	const DUVertexVector* displaced_in_verts;
	const DUQuadVector* quads_in;
	const UVVector* uvs_in;
	const std::vector<Reference<Material> >* materials;
};


/*
	view dependent subdivision:
		* triangle in view frustrum
		* screen space pixel size > subdivide_pixel_threshold

	subdivide = 
		num_subdivs < min_num_subdivisions ||
		(
		num_subdivs < max_num_subdivs && 
		(curvature >= curvature_threshold && 
		triangle in view frustrum &&
		screen space pixel size > subdivide_pixel_threshold ) ||
		displacement_error > displacement_error_threshold
		)


		if view_dependent:
			subdivide = 
				num_subdivs < max_num_subdivs AND
				triangle in view frustrum AND
				screen space pixel size > subdivide_pixel_threshold AND
				(curvature >= curvature_threshold OR 
				displacement_error >= displacement_error_threshold)

		else if not view dependent:
			subdivide = 
				num_subdivs < max_num_subdivs AND
				(curvature >= curvature_threshold OR 
				displacement_error >= displacement_error_threshold)
*/
class BuildSubdividingQuadTask : public Indigo::Task
{
public:
	BuildSubdividingQuadTask(const BuildSubdividingPrimitiveTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const DUVertexVector& displaced_in_verts = *closure.displaced_in_verts;
		const DUQuadVector& quads_in = *closure.quads_in;

		float curvature_dot_thresh;
		if(closure.options->subdivide_curvature_threshold < 0)
			curvature_dot_thresh = 1; // Treat subdivide_curvature_threshold as 0, cos(0) = 1.
		else if(closure.options->subdivide_curvature_threshold > Maths::pi_2<double>())
			curvature_dot_thresh = 0;
		else 
			curvature_dot_thresh = (float)std::cos(closure.options->subdivide_curvature_threshold);

		// Create some temporary buffers
		std::vector<Vec3f> quad_verts_pos_os(4);

		std::vector<Vec3f> clipped_poly_verts_os;
		clipped_poly_verts_os.reserve(32);

		std::vector<Vec3f> clipped_poly_verts_cs;
		clipped_poly_verts_cs.reserve(32);

		std::vector<Vec3f> temp_vert_buffer;
		temp_vert_buffer.reserve(32);

		DUUVCoordEvaluator temp_du_texcoord_evaluator;
		temp_du_texcoord_evaluator.num_uvs = closure.num_uv_sets;

		ThreadContext context;

		std::vector<Vec2f> temp_uv_buffer;
		temp_uv_buffer.resize(closure.num_uv_sets * 4);

		for(int q = begin; q < end; ++q)
		{
			const DUQuad& quad = quads_in[q];
			const int num_sides = quad.numSides();
			// Decide if we are going to subdivide the quad
			bool subdivide_quad = false;

			if(quad.dead)
			{
				subdivide_quad = false;
			}
			else
			{
				if(closure.num_subdivs_done < closure.min_num_subdivisions || num_sides == 3) // Make sure all tris are subdivided to quads.
				{
					subdivide_quad = true;
				}
				else
				{
					assert(num_sides == 4);
					bool view_dependent_subdivide; // = (triangle in view frustrum) AND (screen space pixel size > subdivide_pixel_threshold)
					
					if(!closure.options->view_dependent_subdivision)
						view_dependent_subdivide = true;
					else
					{
						// Build vector of displaced quad vertex positions. (in object space)
						for(uint32_t i = 0; i < 4; ++i)
							quad_verts_pos_os[i] = displaced_in_verts[quad.vertex_indices[i]].pos;

						// Fast path to see if all verts are completely inside all clipping planes
						bool completely_unclipped = true;
						for(size_t p=0; p<closure.options->camera_clip_planes_os.size(); ++p)
							for(uint32_t i = 0; i < 4; ++i)
								if(closure.options->camera_clip_planes_os[p].signedDistToPoint(quad_verts_pos_os[i]) > 0)
								{
									completely_unclipped = false;
									goto done_quad_unclipped_check;
								}
done_quad_unclipped_check:
						if(completely_unclipped)
							clipped_poly_verts_os = quad_verts_pos_os;
						else
						{
							// Clip quad against frustrum planes
							TriBoxIntersection::clipPolyToPlaneHalfSpaces(closure.options->camera_clip_planes_os, quad_verts_pos_os, temp_vert_buffer, clipped_poly_verts_os);
						}

						if(clipped_poly_verts_os.size() > 0) // If the quad has not been completely clipped away:
						{
							// Convert clipped verts to camera space
							clipped_poly_verts_cs.resize(clipped_poly_verts_os.size());
							for(uint32_t i = 0; i < clipped_poly_verts_cs.size(); ++i)
							{
								Vec4f v_os;
								clipped_poly_verts_os[i].pointToVec4f(v_os);

								const Vec4f v_cs = closure.options->object_to_camera * v_os;

								clipped_poly_verts_cs[i] = Vec3f(v_cs);
							}

							// Compute 2D bounding box of clipped quad in screen space
							const Vec2f v0_ss = screenSpacePosForCameraSpacePos(clipped_poly_verts_cs[0]);
							Rect2f rect_ss(v0_ss, v0_ss);

							for(size_t i = 1; i < clipped_poly_verts_cs.size(); ++i)
								rect_ss.enlargeToHoldPoint(
									screenSpacePosForCameraSpacePos(clipped_poly_verts_cs[i])
								);

							// Subdivide only if the width of height of the screen space quad bounding rectangle is bigger than the pixel height threshold
							const bool exceeds_pixel_threshold = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y) > closure.options->pixel_height_at_dist_one * closure.options->subdivide_pixel_threshold;

							view_dependent_subdivide = exceeds_pixel_threshold;
						}
						else
							view_dependent_subdivide = false;
					}
					
					// view_dependent_subdivide should be set now (and will be true if !view_dependent_subdivision)
					if(view_dependent_subdivide)
					{
						if(isQuadCurvatureGrEqThreshold(displaced_in_verts, quad, (float)closure.options->subdivide_curvature_threshold, curvature_dot_thresh))
							subdivide_quad = true;
						else
						{
							if(closure.options->displacement_error_threshold <= 0)
							{
								// If displacement_error_threshold is <= 0, then since displacment_error is always >= 0, then we will always subdivide the quad.
								// So avoid computing the displacment_error.
								subdivide_quad = true;
							}
							else
							{
								// Test displacement error
									
								// Eval UVs
								for(uint32_t z = 0; z < closure.num_uv_sets; ++z)
								{
									temp_uv_buffer[z*closure.num_uv_sets + 0] = getUVs(*closure.uvs_in, closure.num_uv_sets, quad.vertex_indices[0], z);
									temp_uv_buffer[z*closure.num_uv_sets + 1] = getUVs(*closure.uvs_in, closure.num_uv_sets, quad.vertex_indices[1], z);
									temp_uv_buffer[z*closure.num_uv_sets + 2] = getUVs(*closure.uvs_in, closure.num_uv_sets, quad.vertex_indices[2], z);
									temp_uv_buffer[z*closure.num_uv_sets + 3] = getUVs(*closure.uvs_in, closure.num_uv_sets, quad.vertex_indices[3], z);
								}

								EvalDispBoundsInfo eval_info;
								for(int i=0; i<4; ++i)
									eval_info.vert_pos_os[i] = displaced_in_verts[quad.vertex_indices[i]].pos.toVec4fPoint();
								eval_info.context = &context;
								eval_info.temp_uv_coord_evaluator = &temp_du_texcoord_evaluator;
								eval_info.uvs = &temp_uv_buffer[0];
								eval_info.num_verts = 4;
								eval_info.num_uv_sets = closure.num_uv_sets;
								for(int i=0; i<4; ++i)
									eval_info.vert_displacements[i] = displaced_in_verts[quad.vertex_indices[i]].displacement;
								eval_info.error_threshold = (float)closure.options->displacement_error_threshold;

								eval_info.suggested_sampling_res = getDispErrorSamplingResForQuads(closure.quads_in->size());
								
								subdivide_quad = (*closure.materials)[quad.mat_index]->doesDisplacementErrorExceedThreshold(eval_info);
							}
						}
					}
				}
			}

			(*closure.subdividing_quad)[q] = subdivide_quad ? num_sides : 1;
		}
	}

	const BuildSubdividingPrimitiveTaskClosure& closure;
	int begin, end;
};


struct ProcessQuadsTaskClosure
{
	const js::Vector<int, 16>* subdividing_quad;
	const js::Vector<int, 16>* quad_write_index;

	unsigned int num_uv_sets;
	int num_subdivs_done;
	bool averaging;
	const DUVertexVector* verts_in;
	DUVertexVector* verts_out;
	const UVVector* uvs_in;
	UVVector* uvs_out;
	const DUQuadVector* quads_in;
	DUQuadVector* quads_out;

	std::vector<IndigoAtomic>* verts_processed; // Which vertices from verts_out have been processed.

	std::vector<int>* existing_vert_new_index; // For each vert in verts_in, the index of the corresponding vertex in verts_out.
};


// Do processing for quads and vertices that can be done in parallel, after the subdivision has been done.
// * Set updated vertex positions and UVs
class ProcessQuadsTask : public Indigo::Task
{
public:
	ProcessQuadsTask(const ProcessQuadsTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const js::Vector<int, 16>& subdividing_quad = *closure.subdividing_quad;
		const DUVertexVector& verts_in = *closure.verts_in;
		DUVertexVector& verts_out = *closure.verts_out;
		const UVVector& uvs_in = *closure.uvs_in;
		UVVector& uvs_out = *closure.uvs_out;
		const DUQuadVector& quads_in = *closure.quads_in;
		std::vector<IndigoAtomic>& verts_processed = *closure.verts_processed;
		const std::vector<int>& existing_vert_new_index = *closure.existing_vert_new_index;
		const int num_uv_sets = closure.num_uv_sets;
		const bool no_averaging = !closure.averaging;

		Vec2f centroid_uvs[MAX_NUM_UV_SETS];
		Vec2f Q_sum_uvs[MAX_NUM_UV_SETS];
		Vec2f R_sum_uvs[MAX_NUM_UV_SETS];

		for(size_t q = begin; q < end; ++q) // For each quad in range of quads_in
		{
			const DUQuad& quad_in = quads_in[q];

			if(subdividing_quad[q] > 1)
			{
				// Create the quad's centroid vertex and UVs
				const int num_sides = quad_in.numSides();

				Vec3f centroid_pos;
				Vec3f centroid_normal;
				if(num_sides == 4)
				{
					centroid_pos    = (verts_in[quad_in.vertex_indices[0]].pos + verts_in[quad_in.vertex_indices[1]].pos + verts_in[quad_in.vertex_indices[2]].pos + verts_in[quad_in.vertex_indices[3]].pos) * 0.25f;
					centroid_normal = (verts_in[quad_in.vertex_indices[0]].normal + verts_in[quad_in.vertex_indices[1]].normal + verts_in[quad_in.vertex_indices[2]].normal + verts_in[quad_in.vertex_indices[3]].normal) * 0.25f;
					for(int z = 0; z < num_uv_sets; ++z)
						centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[1], z) +
							getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[2], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[3], z)) * 0.25f;
				}
				else
				{
					centroid_pos    = (verts_in[quad_in.vertex_indices[0]].pos + verts_in[quad_in.vertex_indices[1]].pos + verts_in[quad_in.vertex_indices[2]].pos) * (1.f / 3.f);
					centroid_normal = (verts_in[quad_in.vertex_indices[0]].normal + verts_in[quad_in.vertex_indices[1]].normal + verts_in[quad_in.vertex_indices[2]].normal) * (1.f / 3.f);
					for(int z = 0; z < num_uv_sets; ++z)
						centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[1], z) +
							getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[2], z)) * (1.f / 3.f);
				}


				for(int v = 0; v < num_sides; ++v)
				{
					const unsigned int vi   = quad_in.vertex_indices[v]; // Current vert
					const unsigned int vi1  = quad_in.vertex_indices[num_sides == 3 ? mod3(v + 1) : mod4(v + 1)]; // Next vert around poly
					const int v_minus_1 = num_sides == 3 ? mod3(v + 2) : mod4(v + 3); // v_{i-1} mod num_vertices
					const unsigned int v_i_minus_1 = quad_in.vertex_indices[v_minus_1]; // Prev vert around poly

					// Set edge midpoint vertex position and UV
					const int edge_midpoint_v_i = quad_in.edge_midpoint_vert_index[v];
					if(edge_midpoint_v_i != -1)
					{
						if(verts_processed[edge_midpoint_v_i].increment() == 0) // If this vert has not been processed yet:
						{
							const int adj_quad_i = quad_in.adjacent_quad_index[v]; // Get the index of the quad adjacent to this quad over the current edge.

							Vec3f pos;
							Vec3f normal;
							Vec2f temp_uvs[MAX_NUM_UV_SETS];
							if(adj_quad_i == -1 || no_averaging)
							{
								// Special case for border edge.
								pos    = (verts_in[vi].pos    + verts_in[vi1].pos   ) * 0.5f;
								normal = (verts_in[vi].normal + verts_in[vi1].normal) * 0.5f;
								for(int z=0; z<num_uv_sets; ++z)
									temp_uvs[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, vi1, z)) * 0.5f;
							}
							else
							{
								const DUQuad& adj_quad = quads_in[adj_quad_i];
								Vec3f adjacent_quad_centroid_pos;
								Vec3f adjacent_quad_centroid_normal;
								Vec2f adjacent_quad_centroid_uvs[MAX_NUM_UV_SETS];
								if(adj_quad.isQuad())
								{
									adjacent_quad_centroid_pos = (verts_in[adj_quad.vertex_indices[0]].pos + verts_in[adj_quad.vertex_indices[1]].pos + verts_in[adj_quad.vertex_indices[2]].pos + verts_in[adj_quad.vertex_indices[3]].pos) * 0.25f;
									adjacent_quad_centroid_normal = (verts_in[adj_quad.vertex_indices[0]].normal + verts_in[adj_quad.vertex_indices[1]].normal + verts_in[adj_quad.vertex_indices[2]].normal + verts_in[adj_quad.vertex_indices[3]].normal) * 0.25f;
									for(int z=0; z<num_uv_sets; ++z)
										adjacent_quad_centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[1], z) +
											getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[2], z) + getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[3], z)) * 0.25f;
								}
								else
								{
									adjacent_quad_centroid_pos = (verts_in[adj_quad.vertex_indices[0]].pos + verts_in[adj_quad.vertex_indices[1]].pos + verts_in[adj_quad.vertex_indices[2]].pos) * (1.f / 3.f);
									adjacent_quad_centroid_normal = (verts_in[adj_quad.vertex_indices[0]].normal + verts_in[adj_quad.vertex_indices[1]].normal + verts_in[adj_quad.vertex_indices[2]].normal) * (1.f / 3.f);
									for(int z=0; z<num_uv_sets; ++z)
										adjacent_quad_centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[1], z) +
											getUVs(uvs_in, num_uv_sets, adj_quad.vertex_indices[2], z)) * (1.f / 3.f);
								}

								pos = (verts_in[vi].pos + verts_in[vi1].pos + // Edge endpoints
									centroid_pos + adjacent_quad_centroid_pos) * 0.25f;

								normal = (verts_in[vi].normal + verts_in[vi1].normal + // Edge endpoints
									centroid_normal + adjacent_quad_centroid_normal) * 0.25f;

								for(int z=0; z<num_uv_sets; ++z)
									temp_uvs[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, vi1, z) + // Edge endpoints
										centroid_uvs[z] + adjacent_quad_centroid_uvs[z]) * 0.25f;
							}

							verts_out[edge_midpoint_v_i].pos = pos;
							verts_out[edge_midpoint_v_i].normal = normal;

							if(verts_in[vi].uv_discontinuity)
							{
								for(int z=0; z<num_uv_sets; ++z)
									getUVs(uvs_out, num_uv_sets, edge_midpoint_v_i, z) = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, vi1, z)) * 0.5f;
							}
							else
							{
								for(int z=0; z<num_uv_sets; ++z)
									getUVs(uvs_out, num_uv_sets, edge_midpoint_v_i, z) = temp_uvs[z];
							}
						}
					}

					// Update corner vertex information.
					const int new_vi = existing_vert_new_index[vi];
					if(verts_processed[new_vi].increment() == 0) // If this vert has not been processed yet:
					{
						if(no_averaging)
						{
							verts_out[new_vi].pos = verts_in[vi].pos;
							verts_out[new_vi].normal = verts_in[vi].normal;
							for(int z=0; z<num_uv_sets; ++z)
								getUVs(uvs_out, num_uv_sets, new_vi, z) = getUVs(uvs_in, num_uv_sets, vi, z);
						}
						else
						{

						// Set updated position and UVs for old quad vertices.

						// We will traverse clockwise around all quads adjacent to this vertex.
						// 'Q = The average of the new face points for all faces adjacent to the old vertex point'.
						Vec3f Q_sum = centroid_pos;
						Vec3f Q_sum_n = centroid_normal;
						Vec2f Q_sum_uvs[MAX_NUM_UV_SETS];
						for(int z=0; z<num_uv_sets; ++z)
							Q_sum_uvs[z] = centroid_uvs[z];


						Vec3f clockwise_border_midpoint    = (verts_in[vi].pos + verts_in[vi1].pos) * 0.5f;
						Vec3f clockwise_border_n_midpoint  = (verts_in[vi].normal + verts_in[vi1].normal) * 0.5f;
						Vec2f clockwise_border_uv_midpoint[MAX_NUM_UV_SETS];
						for(int z=0; z<num_uv_sets; ++z)
							clockwise_border_uv_midpoint[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, vi1, z)) * 0.5f;

						// 'R = the average of the midpoints of all old edges incident on the old vertex point'
						Vec3f R_sum = clockwise_border_midpoint;
						Vec3f R_sum_n = clockwise_border_n_midpoint;
						for(int z=0; z<num_uv_sets; ++z)
							R_sum_uvs[z] = clockwise_border_uv_midpoint[z];

						int cur_quad_i = quad_in.adjacent_quad_index[v]; // Start at adjacent quad
						int cur_quad_edge_i = quad_in.getAdjacentQuadEdgeIndex(v); // Edge that cur_quad shares with prev_quad NEW
						int num_adjacent_faces = 1;
						int num_adjacent_edges = 1; // aka. n = recip factor for R_sum.
						while(cur_quad_i != -1 && cur_quad_i != q) // Keep traversing around the vert until we get to a border, or we get back to quad q.  NOTE: this does not stricly bound this loop.  May get caught in a cycle for a weird mesh.
						{
							const DUQuad& cur_quad = quads_in[cur_quad_i];
							const int cur_quad_sides = cur_quad.numSides();

							// Find which edge of cur_quad is adjacent to prev_quad.
							const int c = cur_quad_edge_i;

							// Accumulate stuff from this quad
							Vec3f centroid;
							Vec3f centroid_normal;
							Vec2f uv_centroid[MAX_NUM_UV_SETS];
							if(cur_quad_sides == 4)
							{
								centroid    = (verts_in[cur_quad.vertex_indices[0]].pos + verts_in[cur_quad.vertex_indices[1]].pos + verts_in[cur_quad.vertex_indices[2]].pos + verts_in[cur_quad.vertex_indices[3]].pos) * 0.25f;
								centroid_normal = (verts_in[cur_quad.vertex_indices[0]].normal + verts_in[cur_quad.vertex_indices[1]].normal + verts_in[cur_quad.vertex_indices[2]].normal + verts_in[cur_quad.vertex_indices[3]].normal) * 0.25f;
								for(int z = 0; z < num_uv_sets; ++z)
									uv_centroid[z] = (getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[1], z) +
										getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[2], z) + getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[3], z)) * 0.25f;
							}
							else
							{
							 	centroid    = (verts_in[cur_quad.vertex_indices[0]].pos + verts_in[cur_quad.vertex_indices[1]].pos + verts_in[cur_quad.vertex_indices[2]].pos) * (1.f / 3.f);
								centroid_normal = (verts_in[cur_quad.vertex_indices[0]].normal + verts_in[cur_quad.vertex_indices[1]].normal + verts_in[cur_quad.vertex_indices[2]].normal) * (1.f / 3.f);
								for(int z = 0; z < num_uv_sets; ++z)
									uv_centroid[z] = (getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[1], z) +
										getUVs(uvs_in, num_uv_sets, cur_quad.vertex_indices[2], z)) * (1.f / 3.f);
							}
							Q_sum += centroid;
							Q_sum_n += centroid_normal;
							for(int z=0; z<num_uv_sets; ++z)
								Q_sum_uvs[z] += uv_centroid[z];

							/*
							cur_q is the next quad from quad q when doing a clockwise traversal around v_i.
							___________________________________________
							v_c+2   c+1    v_c+1|v_i              v_i-1
							                    |
							                    |
							         cur_q     c|v         quad q
							                    |
							                    |
							                    |
							_________________v_c|v_i+1_________________   
							
							*/
							const int next_edge_i = cur_quad_sides == 4 ? mod4(c + 1) : mod3(c + 1);
							const int next_quad_i = cur_quad.adjacent_quad_index[next_edge_i];

							// Accumulate edge midpoint position from leading edge (unless we have done this edge already)
							const int far_edge_v_i  = cur_quad.vertex_indices[cur_quad_sides == 4 ? mod4(c + 2) : mod3(c + 2)];
							const Vec3f edge_midpoint    = (verts_in[vi].pos + verts_in[far_edge_v_i].pos) * 0.5f;
							const Vec3f edge_n_midpoint  = (verts_in[vi].normal + verts_in[far_edge_v_i].normal) * 0.5f;
							clockwise_border_midpoint    = edge_midpoint;
							clockwise_border_n_midpoint  = edge_n_midpoint;
							for(int z=0; z<num_uv_sets; ++z)
								clockwise_border_uv_midpoint[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, far_edge_v_i, z)) * 0.5f;


							R_sum += edge_midpoint;
							R_sum_n += edge_n_midpoint;
							for(int z=0; z<num_uv_sets; ++z)
								R_sum_uvs[z] += clockwise_border_uv_midpoint[z];
							num_adjacent_edges++;

							// Go to next quad around the vertex.
							cur_quad_i = next_quad_i;
							cur_quad_edge_i = cur_quad.getAdjacentQuadEdgeIndex(next_edge_i); // Edge that cur_quad shares with prev_quad

							num_adjacent_faces++;
						}

						assert(cur_quad_i == -1 || cur_quad_i == q); // We either traversed to a border edge, or back to the original quad.
						// if we traversed to a border edge, we need to traverse the other way (counter-clockwise) to get the other border edge.
						if(cur_quad_i == -1) // cur_quad_i == -1  =>   we traversed to a border edge, this is a border vert.
						{
							// We also need to walk the other way (counter-clockwise) around the vertex, until we meet up, or hit an empty edge.
							Vec3f cntr_clockwise_border_midpoint    = (verts_in[vi].pos + verts_in[v_i_minus_1].pos) * 0.5f;
							Vec3f cntr_clockwise_border_n_midpoint  = (verts_in[vi].normal + verts_in[v_i_minus_1].normal) * 0.5f;
							Vec2f cntr_clockwise_border_uv_midpoint[MAX_NUM_UV_SETS];
							for(int z=0; z<num_uv_sets; ++z)
								cntr_clockwise_border_uv_midpoint[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, v_i_minus_1, z)) * 0.5f;

							num_adjacent_edges++; // Add the edge leading to vert v.
							cur_quad_i = quad_in.adjacent_quad_index[num_sides == 4 ? mod4(v + 3) : mod3(v + 2)]; // Start at prev adjacent quad
							cur_quad_edge_i = quad_in.getAdjacentQuadEdgeIndex(num_sides == 4 ? mod4(v + 3) : mod3(v + 2)); // Edge that cur_quad shares with prev_quad NEW
							while(cur_quad_i != -1 && cur_quad_i != q) // Keep traversing around the vert until we get to a border, or to where we got with the clockwise traversal.
							{
								const DUQuad& cur_quad = quads_in[cur_quad_i];
								const int cur_quad_sides = cur_quad.numSides();

								// Find which edge of cur_quad is adjacent to prev_quad.
								const int c = cur_quad_edge_i;

								/*
								Now cur_q is the next quad from quad q when doing a counter-clockwise traversal around v_i.
								___________________________________________
								v_i+1            v_i|v_c       c-1     v_c-1
								                    |
								                    |
								        quad q     v|c         cur_q
								                    |
								                    |
								                    |
								_______________v_i-1|v_c+1_________________   

								*/

								const int far_edge_v_i  = cur_quad.vertex_indices[cur_quad_sides == 4 ? mod4(c + 3) : mod3(c + 2)];
								const Vec3f edge_midpoint    = (verts_in[vi].pos + verts_in[far_edge_v_i].pos) * 0.5f;
								const Vec3f edge_n_midpoint  = (verts_in[vi].normal + verts_in[far_edge_v_i].normal) * 0.5f;
								cntr_clockwise_border_midpoint    = edge_midpoint;
								cntr_clockwise_border_n_midpoint  = edge_n_midpoint;
								for(int z=0; z<num_uv_sets; ++z)
									cntr_clockwise_border_uv_midpoint[z] = (getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, far_edge_v_i, z)) * 0.5f;
								num_adjacent_edges++;

								// Go to next quad around the vertex.
								const int next_edge_i = cur_quad_sides == 4 ? mod4(c + 3) : mod3(c + 2);//NEW
								cur_quad_i = cur_quad.adjacent_quad_index[next_edge_i];
								cur_quad_edge_i = cur_quad.getAdjacentQuadEdgeIndex(next_edge_i); // Edge that cur_quad shares with prev_quad
							}

							
							// NOTE: These two cases are the same currently.
							if(num_adjacent_edges == 2)
							{
								// Special case for vertex that is only adjacent to one quad.
								Vec3f R_sum    = (verts_in[vi].pos + verts_in[vi1].pos * 0.5f + verts_in[v_i_minus_1].pos * 0.5f) * 0.25f;
								Vec3f R_sum_n  = (verts_in[vi].normal + verts_in[vi1].normal * 0.5f + verts_in[v_i_minus_1].normal * 0.5f) * 0.25f;
								verts_out[new_vi].pos = verts_in[vi].pos * 0.5f + R_sum;
								verts_out[new_vi].normal = verts_in[vi].normal * 0.5f + R_sum_n;
								for(int z=0; z<num_uv_sets; ++z)
									getUVs(uvs_out, num_uv_sets, new_vi, z) = getUVs(uvs_in, num_uv_sets, vi, z) * 0.5f + 
										(getUVs(uvs_in, num_uv_sets, vi, z) + getUVs(uvs_in, num_uv_sets, vi1, z) * 0.5f + getUVs(uvs_in, num_uv_sets, v_i_minus_1, z) * 0.5f) * 0.25f;
							}
							else// if(border_vert)
							{
								Vec3f R_sum    = (clockwise_border_midpoint    + cntr_clockwise_border_midpoint   ) * 0.25f;
								Vec3f R_sum_n  = (clockwise_border_n_midpoint  + clockwise_border_n_midpoint      ) * 0.25f;
								verts_out[new_vi].pos    = verts_in[vi].pos    * 0.5f + R_sum;
								verts_out[new_vi].normal = verts_in[vi].normal * 0.5f + R_sum_n;
								for(int z=0; z<num_uv_sets; ++z)
									getUVs(uvs_out, num_uv_sets, new_vi, z) = getUVs(uvs_in, num_uv_sets, vi, z) * 0.5f + 
										(clockwise_border_uv_midpoint[z] + cntr_clockwise_border_uv_midpoint[z]) * 0.25f;
							}
						}
						else
						{
							const int n = num_adjacent_edges;
							// 'S = old vertex point'
							const Vec3f S = verts_in[vi].pos;
							const Vec3f Q = Q_sum / (float)num_adjacent_faces;
							const Vec3f R = R_sum / (float)num_adjacent_edges;
							const Vec3f new_V = (Q + R * 2.f + S * (n - 3.f)) * (1.f / n);

							const Vec3f S_n = verts_in[vi].normal;
							const Vec3f Q_n = Q_sum_n / (float)num_adjacent_faces;
							const Vec3f R_n = R_sum_n / (float)num_adjacent_edges;
							const Vec3f new_V_n = (Q_n + R_n * 2.f + S_n * (n - 3.f)) * (1.f / n);

							for(int z=0; z<num_uv_sets; ++z)
							{
								const Vec2f S_uv = getUVs(uvs_in, num_uv_sets, vi, z);
								const Vec2f Q_uv = Q_sum_uvs[z] / (float)num_adjacent_faces;
								const Vec2f R_uv = R_sum_uvs[z] / (float)num_adjacent_edges;
								const Vec2f new_V_uv = (Q_uv + R_uv * 2.f + S_uv * (n - 3.f)) * (1.f / n);
								getUVs(uvs_out, num_uv_sets, new_vi, z) = new_V_uv;
							}
							
							verts_out[new_vi].pos = new_V;
							verts_out[new_vi].normal = new_V_n;
						}

						// If the edge from this vert to the next, or the edge from the prev to this vert has a UV discontinuity across it, then just use the old UV.
						if(verts_in[vi].uv_discontinuity) //quad_in.edge_uv_discontinuity[v] || quad_in.edge_uv_discontinuity[v_minus_1])
							for(int z=0; z<num_uv_sets; ++z)
								getUVs(uvs_out, num_uv_sets, new_vi, z) = getUVs(uvs_in, num_uv_sets, vi, z);
						}
					} // End If this vert has not been processed yet:
				}
			}
		} // End for each quad in task range.
	}

	const ProcessQuadsTaskClosure& closure;
	int begin, end;
};


void DisplacementUtils::linearSubdivision(
	Indigo::TaskManager& task_manager,
	PrintOutput& print_output,
	ThreadContext& context,
	const std::vector<Reference<Material> >& materials,
	Polygons& polygons_in,
	const VertsAndUVs& verts_and_uvs_in,
	unsigned int num_uv_sets,
	unsigned int num_subdivs_done,
	bool subdivision_smoothing,
	const DUOptions& options,
	DUScratchInfo& scratch_info,
	Polygons& polygons_out,
	VertsAndUVs& verts_and_uvs_out
	)
{
	DISPLACEMENT_CREATE_TIMER(timer);

	DUQuadVector& quads_in = polygons_in.quads;
	const DUVertexVector& verts_in = verts_and_uvs_in.verts;
	const UVVector& uvs_in = verts_and_uvs_in.uvs;

	DUQuadVector& quads_out = polygons_out.quads;
	DUVertexVector& verts_out = verts_and_uvs_out.verts;
	UVVector& uvs_out = verts_and_uvs_out.uvs;

	// Get displaced vertices, which are needed for testing if subdivision is needed, in some cases.
	DUVertexVector displaced_in_verts;
	if(options.view_dependent_subdivision || options.subdivide_curvature_threshold > 0) // Only generate if needed.
	{
		DISPLACEMENT_RESET_TIMER(timer);

		displace(
			task_manager,
			context,
			materials,
			quads_in,
			verts_in,
			uvs_in,
			num_uv_sets,
			displaced_in_verts // verts out
		);

		DISPLACEMENT_PRINT_RESULTS(conPrint("   displace():                  " + timer.elapsedStringNPlaces(5)));
		DISPLACEMENT_RESET_TIMER(timer);

		// Calculate normals of the displaced vertices
		computeVertexNormals(quads_in, displaced_in_verts);

		DISPLACEMENT_PRINT_RESULTS(conPrint("   computeVertexNormals():      " + timer.elapsedStringNPlaces(5)));
	}

	// Do a pass to decide whether or not to subdivide each quad, and create new vertices if subdividing.

	// If we are doing view-dependent subdivisions, then do a few levels of subdivision, without the fine detail.
	// Otherwise we get situations like view-dependent subdivision done to create mountains, where outside of the camera frustum the mountains
	// fall away immediately to zero height.  This is bad for several reasons - including shading and lighting artifacts.
	const unsigned int min_num_subdivisions = options.view_dependent_subdivision ? options.max_num_subdivisions / 2 : 0;

	//========================== Work out if we are subdividing each triangle ==========================
	//DISPLACEMENT_RESET_TIMER(timer);
	const size_t quads_in_size = quads_in.size();

	js::Vector<int, 16> subdividing_quad(quads_in_size); // Store the number of new quads in each element.  1 = not subdividing.

	BuildSubdividingPrimitiveTaskClosure sub_prim_closure;
	sub_prim_closure.options = &options;
	sub_prim_closure.num_subdivs_done = num_subdivs_done;
	sub_prim_closure.min_num_subdivisions = min_num_subdivisions;
	sub_prim_closure.num_uv_sets = num_uv_sets;
	sub_prim_closure.subdividing_quad = &subdividing_quad;
	sub_prim_closure.displaced_in_verts = &displaced_in_verts;
	sub_prim_closure.quads_in = &quads_in;
	sub_prim_closure.uvs_in = &uvs_in;
	sub_prim_closure.materials = &materials;

	//========================== Work out if we are subdividing each quad ==========================
	DISPLACEMENT_RESET_TIMER(timer);
	if(subdivision_smoothing)
	{
		// Mark all quads as subdividing.
		for(size_t q=0; q<quads_in_size; ++q)
			subdividing_quad[q] = quads_in[q].numSides();
	}
	else
		task_manager.runParallelForTasks<BuildSubdividingQuadTask, BuildSubdividingPrimitiveTaskClosure>(sub_prim_closure, 0, quads_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building subdividing_quad[]: " + timer.elapsedStringNPlaces(5)));


	//========================== Build quad_write_index array ==========================
	DISPLACEMENT_RESET_TIMER(timer);
	js::Vector<int, 16> quad_write_index(quads_in.size());

	int current_quad_write_index = 0;
	for(size_t i=0; i<quads_in_size; ++i)
	{
		quad_write_index[i] = current_quad_write_index;
		current_quad_write_index += subdividing_quad[i];
	}
	const int total_new_quads = current_quad_write_index;

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building quad_write_index[]: " + timer.elapsedStringNPlaces(5)));


	//========================== Subdivide quads ==========================
	DISPLACEMENT_RESET_TIMER(timer);

	quads_out.resizeUninitialised(total_new_quads);
	
	verts_out.resize(0);
	uvs_out.resize(0);

	// Reserve space for the new Verts and UVs.  This is very important for performance.
	verts_out.reserve(verts_in.size() * 4);
	uvs_out.reserve(uvs_in.size() * 4);
	std::vector<int> existing_vert_new_index(verts_in.size(), -1);

	// For each quad
	for(size_t q = 0; q < quads_in_size; ++q)
	{
		const int64 write_index = quad_write_index[q];

		DUQuad& quad_in = quads_in[q];
		const int num_sides = quad_in.numSides(); // num sides of this polygon
		if(subdividing_quad[q] > 1)
		{
			// Create the quad's centroid vertex and UVs
			const uint32_t centroid_vert_index = (uint32_t)verts_out.size();
			assert(uvs_out.size() == verts_out.size() * num_uv_sets);

			Vec3f centroid_pos;
			Vec3f centroid_normal;
			Vec2f centroid_uvs[MAX_NUM_UV_SETS];
			if(num_sides == 4)
			{
				centroid_pos = (verts_in[quad_in.vertex_indices[0]].pos + verts_in[quad_in.vertex_indices[1]].pos + verts_in[quad_in.vertex_indices[2]].pos + verts_in[quad_in.vertex_indices[3]].pos) * 0.25f;
				centroid_normal = (verts_in[quad_in.vertex_indices[0]].normal + verts_in[quad_in.vertex_indices[1]].normal + verts_in[quad_in.vertex_indices[2]].normal + verts_in[quad_in.vertex_indices[3]].normal);// * 0.25f
				for(uint32 z = 0; z < num_uv_sets; ++z)
					centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[1], z) +
						getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[2], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[3], z)) * 0.25f;
			}
			else
			{
				centroid_pos = (verts_in[quad_in.vertex_indices[0]].pos + verts_in[quad_in.vertex_indices[1]].pos + verts_in[quad_in.vertex_indices[2]].pos) * (1.f / 3.f);
				centroid_normal = (verts_in[quad_in.vertex_indices[0]].normal + verts_in[quad_in.vertex_indices[1]].normal + verts_in[quad_in.vertex_indices[2]].normal);// * 0.25f
				for(uint32 z = 0; z < num_uv_sets; ++z)
					centroid_uvs[z] = (getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[0], z) + getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[1], z) +
						getUVs(uvs_in, num_uv_sets, quad_in.vertex_indices[2], z)) * (1.f / 3.f);
			}
			verts_out.push_back(DUVertex(
				centroid_pos,
				centroid_normal
				));
			
			const uint32 centroid_uv_offset = centroid_vert_index * num_uv_sets;
			uvs_out.resize(centroid_uv_offset + num_uv_sets);
			for(uint32_t z = 0; z < num_uv_sets; ++z)
				uvs_out[centroid_uv_offset + z] = centroid_uvs[z];

			// Create the quad's corner vertices, store indices of new vertices in new_corner_vert_i.
			uint32 new_corner_vert_i[4];
			for(int v = 0; v < num_sides; ++v)
			{
				const unsigned int vi = quad_in.vertex_indices[v];

				int new_corner_vert_index = existing_vert_new_index[vi];
				if(new_corner_vert_index == -1) // If the new corner vertex has not been created yet:
				{
					const DUVertex& corner_vert_in = verts_in[vi];
					new_corner_vert_index = (int)verts_out.size();
					//verts_out.resize(new_corner_vert_index + 1);
					//verts_out[new_corner_vert_index].anchored = false;
					//verts_out[new_corner_vert_index].uv_discontinuity = false;
					verts_out.push_back(corner_vert_in);
					uvs_out.resize(uvs_out.size() + num_uv_sets);

					existing_vert_new_index[vi] = new_corner_vert_index;
				}
				new_corner_vert_i[v] = new_corner_vert_index;
			}

			// Create the quad's edge midpoint vertices
			for(int v = 0; v < num_sides; ++v)
			{
				const unsigned int vi  = new_corner_vert_i[v];
				const unsigned int vi1 = new_corner_vert_i[num_sides == 4 ? mod4(v + 1) : mod3(v + 1)];

				// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.
				bool need_to_create_midpoint_vert = false;

				int cur_c = 0;

				const int adj_quad_i = quad_in.adjacent_quad_index[v]; // Get the index of the quad adjacent to this quad over the current edge.
				if(adj_quad_i == -1) // If no adjacent quad:
				{
					need_to_create_midpoint_vert = true;
				}
				else // else if we have a valid adjacent quad
				{
					const DUQuad& adj_quad = quads_in[adj_quad_i];

					// Find which edge of the adjacent quad is adjacent to this quad, call it 'c'.
					
					cur_c = quad_in.getAdjacentQuadEdgeIndex(v);

					if(adj_quad.edge_midpoint_vert_index[cur_c] != -1)
					{
						// If the adjacent quad has been processed first, and has already created the edge midpoint vertex, use it:
						quad_in.edge_midpoint_vert_index[v] = adj_quad.edge_midpoint_vert_index[cur_c];
						
						if(quad_in.isUVEdgeDiscontinuity(v) != 0)
						{
							// Since there is a UV discontinuity along this edge, we can't use the midpoint UV from the adjacent quad, but have to create
							// our own midpoint UV.
							need_to_create_midpoint_vert = true;
						}
					}
					else // Else if the midpoint vert for this edge has not already been created, create it:
					{
						need_to_create_midpoint_vert = true;
					}
				}

				if(need_to_create_midpoint_vert)
				{
					// Create the edge midpoint vertex.  Pos, normal, UVs will be set later in ProcessQuadsTask
					const uint32_t new_vert_index = (uint32)verts_out.size();
					
					verts_out.resize(new_vert_index + 1);
					verts_out[new_vert_index].adjacent_vert_0 = vi;
					verts_out[new_vert_index].adjacent_vert_1 = vi1;
					verts_out[new_vert_index].anchored = false;
					verts_out[new_vert_index].uv_discontinuity = quad_in.isUVEdgeDiscontinuity(v) != 0; // Mark the vertex as having a UV discontinuity if it lies on an edge with a UV discontinuity.

					quad_in.edge_midpoint_vert_index[v] = new_vert_index;

					uvs_out.resize(uvs_out.size() + num_uv_sets);
				}
			}

			//============== TEMP NEW: Make new subdivided quads here: =========================
			quad_in.child_quads_index = (int)write_index;

			if(num_sides == 3)
			{
				/*
				Child quads and edges for a single original tri:
				  |\
				  |  \
				  |    \
				  |     1\
				  |2  q2   \  1
				  |     0 /  \
				2 |___3__/ 2   \
				  |   2  |      1\
				  |3 q0 1|3  q1    \
				  |___0__|____0______\
				           0
				*/

				const uint32 midpoint_vert_indices[3] = { (uint32)quad_in.edge_midpoint_vert_index[0], (uint32)quad_in.edge_midpoint_vert_index[1], (uint32)quad_in.edge_midpoint_vert_index[2] };

				//--------------- Create bottom left tri---------------
				quads_out[write_index + 0] = DUQuad(
					new_corner_vert_i[0], midpoint_vert_indices[0], centroid_vert_index, midpoint_vert_indices[2],
					quad_in.mat_index
				);

				// NOTE: because edge 2 gets mapped to edge 3, we can't use the AND trick here.
				quads_out[write_index + 0].setUVEdgeDiscontinuity(0, quad_in.getUVEdgeDiscontinuity(0));
				quads_out[write_index + 0].setUVEdgeDiscontinuity(3, quad_in.getUVEdgeDiscontinuity(2));

				//--------------- Create bottom right tri ---------------
				quads_out[write_index + 1] = DUQuad(
					midpoint_vert_indices[0], new_corner_vert_i[1], midpoint_vert_indices[1], centroid_vert_index,
					quad_in.mat_index
				);
				// AND with 0011b = 0x3
				quads_out[write_index + 1].bitfield.v = quad_in.bitfield.v & 0x3;

				//--------------- Create top tri ---------------
				quads_out[write_index + 2] = DUQuad(
					centroid_vert_index, midpoint_vert_indices[1], new_corner_vert_i[2], midpoint_vert_indices[2],
					quad_in.mat_index
				);
				// AND with 0110b = 0x6
				quads_out[write_index + 2].bitfield.v = quad_in.bitfield.v & 0x6;
			}
			else
			{
				const uint32 midpoint_vert_indices[4] = { (uint32)quad_in.edge_midpoint_vert_index[0], (uint32)quad_in.edge_midpoint_vert_index[1], (uint32)quad_in.edge_midpoint_vert_index[2], (uint32)quad_in.edge_midpoint_vert_index[3] };

				/*
				Child quads and edges for a single original quad:
				  _________2_________
				  |    2   |    2   |
				  |3  q3  1|3  q2  1|
				3 |____0___|____0___| 1
				  |    2   |    2   |
				  |3  q0  1|3  q1  1|
				  |____0___|____0___|
				           0
				*/

				//--------------- Create bottom left quad ---------------
				quads_out[write_index + 0] = DUQuad(
					new_corner_vert_i[0], midpoint_vert_indices[0], centroid_vert_index, midpoint_vert_indices[3],
					quad_in.mat_index
				);
				// Copy UV edge discon. info for edges 0 and 3 - AND with 10011001b
				quads_out[write_index + 0].bitfield.v = quad_in.bitfield.v & 153u;

				//--------------- Create bottom right quad ---------------
				quads_out[write_index + 1] = DUQuad(
					midpoint_vert_indices[0], new_corner_vert_i[1], midpoint_vert_indices[1], centroid_vert_index,
					quad_in.mat_index
				);
				// Copy UV edge discon. info for edges 0 and 1 - AND with 00110011b = 
				quads_out[write_index + 1].bitfield.v = quad_in.bitfield.v & 51u;

				//--------------- Create top right quad ---------------
				quads_out[write_index + 2] = DUQuad(
					centroid_vert_index, midpoint_vert_indices[1], new_corner_vert_i[2], midpoint_vert_indices[2],
					quad_in.mat_index
				);
				// Copy UV edge discon. info for edges 1 and 2 - AND with 01100110b = 
				quads_out[write_index + 2].bitfield.v = quad_in.bitfield.v & 102u;

				//--------------- Create top left quad ---------------
				quads_out[write_index + 3] = DUQuad(
					midpoint_vert_indices[3], centroid_vert_index, midpoint_vert_indices[2], new_corner_vert_i[3],
					quad_in.mat_index
				);
				// Copy UV edge discon. info for edges 2 and 3 - AND with 11001100b = 
				quads_out[write_index + 3].bitfield.v = quad_in.bitfield.v & 204u;
			}
		}
		else
		{
			// Else don't subdivide quad.
			quads_out[write_index] = quad_in;

			// Create the quad's corner vertices, store indices of new vertices in new_corner_vert_i.
			for(int v = 0; v < num_sides; ++v)
			{
				const unsigned int vi   = quad_in.vertex_indices[v];

				int new_corner_vert_index = existing_vert_new_index[vi];
				if(new_corner_vert_index == -1) // If the new corner vertex has not been created yet:
				{
					const DUVertex& corner_vert_in = verts_in[vi];

					// Copy corner vertex.
					new_corner_vert_index = (int)verts_out.size();
					verts_out.push_back(corner_vert_in);

					// Copy UVs
					const size_t uv_size = uvs_out.size();
					uvs_out.resize(uv_size + num_uv_sets);
					for(uint32 z=0; z<num_uv_sets; ++z)
						uvs_out[uv_size + z] = getUVs(uvs_in, num_uv_sets, vi, z);

					existing_vert_new_index[vi] = new_corner_vert_index;
				}
				quads_out[write_index].vertex_indices[v] = new_corner_vert_index;
			}

			quad_in.dead = true;
			quad_in.child_quads_index = (int)write_index;
			quads_out[write_index].dead = true;
			for(int v=0; v<4; ++v)
				quads_out[write_index].adjacent_quad_index[v] = -1;
		}
	}

	// Do a pass over anchored vertices to update adjacent_vert_0 and adjacent_vert_1
	// TODO: make a list of anchored vert indices, just iterate over that?
	const size_t verts_out_size = verts_out.size();
	for(size_t v=0; v<verts_out_size; ++v)
		if(verts_out[v].anchored)
		{
			verts_out[v].adjacent_vert_0 = existing_vert_new_index[verts_out[v].adjacent_vert_0];
			verts_out[v].adjacent_vert_1 = existing_vert_new_index[verts_out[v].adjacent_vert_1];
		}


	// TEMP: check quads_out.
	for(size_t q=0; q<quads_out.size(); ++q)
	{
		for(int v=0; v<4; ++v)
		{
			assert(quads_out[q].adjacent_quad_index[v] == -1 || (quads_out[q].adjacent_quad_index[v] >= 0 && quads_out[q].adjacent_quad_index[v] < (int)quads_out.size()));
		}
	}
	DISPLACEMENT_PRINT_RESULTS(conPrint("   subdividing quads:           " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);


	std::vector<IndigoAtomic> verts_processed(verts_out.size());

	//================================ Run ProcessQuads tasks =================================
	ProcessQuadsTaskClosure process_quad_task_closure;
	process_quad_task_closure.subdividing_quad = &subdividing_quad;
	process_quad_task_closure.quad_write_index = &quad_write_index;
	process_quad_task_closure.num_uv_sets = num_uv_sets;
	process_quad_task_closure.num_subdivs_done = num_subdivs_done;
	process_quad_task_closure.verts_in = &verts_in;
	process_quad_task_closure.verts_out = &verts_out;
	process_quad_task_closure.uvs_in = &uvs_in;
	process_quad_task_closure.uvs_out = &uvs_out;
	process_quad_task_closure.quads_in = &quads_in;
	process_quad_task_closure.quads_out = &quads_out;
	process_quad_task_closure.verts_processed = &verts_processed;
	process_quad_task_closure.existing_vert_new_index = &existing_vert_new_index;
	process_quad_task_closure.averaging = subdivision_smoothing;

	//Indigo::TaskManager temp_task_manager(1);
	task_manager.runParallelForTasks<ProcessQuadsTask, ProcessQuadsTaskClosure>(process_quad_task_closure, 0, quads_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   ProcessQuadsTask:            " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);


	// TEMP: check quads_in.
	for(size_t q=0; q<quads_in.size(); ++q)
		for(int v=0; v<4; ++v)
			assert(quads_in[q].adjacent_quad_index[v] == -1 || quads_in[q].adjacent_quad_index[v] >= 0);

	// Now that all child quads have been created, do another pass over the quads to set adjacent_quad_index
	for(size_t q = 0; q < quads_in_size; ++q)
	{
		const int64 write_index = quad_write_index[q];

		const DUQuad& quad_in = quads_in[q];
		const int num_sides = quad_in.isQuad() ? 4 : 3; // num sides of this polygon

		if(subdividing_quad[q] > 1)
		{
			int c[4];
			c[0] = quad_in.getAdjacentQuadEdgeIndex(0);
			c[1] = quad_in.getAdjacentQuadEdgeIndex(1);
			c[2] = quad_in.getAdjacentQuadEdgeIndex(2);
			c[3] = quad_in.getAdjacentQuadEdgeIndex(3);

			const int adj_quad_0 = quad_in.adjacent_quad_index[0];
			const int adj_quad_1 = quad_in.adjacent_quad_index[1];
			const int adj_quad_2 = quad_in.adjacent_quad_index[2];
			const int adj_quad_3 = quad_in.adjacent_quad_index[3];
			
			if(num_sides == 3)
			{
				//--------------- bottom left tri ---------------
				if(adj_quad_0 != -1) // If there is an adjacent quad along edge 0:
				{
					DUQuad& adj_quad = quads_in[adj_quad_0]; // Adjacent quad along edge 0.
					if(adj_quad.dead)
						quads_out[write_index + 0].adjacent_quad_index[0] = adj_quad.child_quads_index;
					else
					{
						const int adj_c = c[0];// Which edge of the adjacent quad is adjacent to the current quad
						const int adj_quad_child_i = adj_quad.child_quads_index + (adj_quad.isTri() ? mod3(adj_c + 1) : mod4(adj_c + 1)); // adj_quad.child_quads_index is the index of the zeroth child.
						quads_out[write_index + 0].adjacent_quad_index[0] = adj_quad_child_i;
					}
				}
				quads_out[write_index + 0].adjacent_quad_index[1] = (int)write_index + 1;
				quads_out[write_index + 0].adjacent_quad_index[2] = (int)write_index + 2;
				if(adj_quad_2 != -1) // If adjacent quad along edge 2:
					quads_out[write_index + 0].adjacent_quad_index[3] = quads_in[adj_quad_2].dead ? quads_in[adj_quad_2].child_quads_index : quads_in[adj_quad_2].child_quads_index + c[2]; // quads_in[adj_quad_3].child_quads[mod4(c[3] + 0)]; // We need to set adjacent_quad_index for this new quad
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(0, quad_in.getAdjacentQuadEdgeIndex(0));
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(1, 3); // edge internal to old polygon
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(2, 3); // edge internal to old polygon
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(3, quad_in.getAdjacentQuadEdgeIndex(2));
				// If the polygon adjacent to this poly along edge 0 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				// This is the only case where an adjacent polygons' edge index changes after subdivision.
				// We only need to check this on the first subdivision, since after the first division all polygons will be quads, so there will be no triangles.
				if(num_subdivs_done == 0 && adj_quad_0 != -1 && !quads_in[adj_quad_0].dead && quads_in[adj_quad_0].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(0) == 2)
					quads_out[write_index + 0].setAdjacentQuadEdgeIndex(0, 3);


				//--------------- bottom right tri ---------------
				if(adj_quad_0 != -1) // If adjacent quad along edge 0:
					quads_out[write_index + 1].adjacent_quad_index[0] = quads_in[adj_quad_0].dead ? quads_in[adj_quad_0].child_quads_index : quads_in[adj_quad_0].child_quads_index + c[0]; // child_quads[mod4(c[0] + 0)];
				if(adj_quad_1 != -1) // If adjacent quad along edge 1:
					quads_out[write_index + 1].adjacent_quad_index[1] = quads_in[adj_quad_1].dead ? quads_in[adj_quad_1].child_quads_index : quads_in[adj_quad_1].child_quads_index + (quads_in[adj_quad_1].isTri() ? mod3(c[1] + 1) : mod4(c[1] + 1)); // child_quads[mod4(c[1] + 1)];
				quads_out[write_index + 1].adjacent_quad_index[2] = (int)write_index + 2;
				quads_out[write_index + 1].adjacent_quad_index[3] = (int)write_index + 0;
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(0, quad_in.getAdjacentQuadEdgeIndex(0));
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(1, quad_in.getAdjacentQuadEdgeIndex(1));
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(2, 0); // edge internal to old polygon
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(3, 1); // edge internal to old polygon
				// If the polygon adjacent to this poly along edge 1 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_1 != -1 && !quads_in[adj_quad_1].dead && quads_in[adj_quad_1].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(1) == 2)
					quads_out[write_index + 1].setAdjacentQuadEdgeIndex(1, 3);



				//--------------- top tri ---------------
				quads_out[write_index + 2].adjacent_quad_index[0] = (int)write_index + 1;
				if(adj_quad_1 != -1) // If adjacent quad along edge 1:
					quads_out[write_index + 2].adjacent_quad_index[1] = quads_in[adj_quad_1].dead ? quads_in[adj_quad_1].child_quads_index : quads_in[adj_quad_1].child_quads_index + c[1]; //child_quads[mod4(c[1] + 0)];
				if(adj_quad_2 != -1) // If adjacent quad along edge 2:
					quads_out[write_index + 2].adjacent_quad_index[2] = quads_in[adj_quad_2].dead ? quads_in[adj_quad_2].child_quads_index : quads_in[adj_quad_2].child_quads_index + (quads_in[adj_quad_2].isTri() ? mod3(c[2] + 1) : mod4(c[2] + 1)); // child_quads[mod4(c[2] + 1)];
				quads_out[write_index + 2].adjacent_quad_index[3] = (int)write_index + 0;
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(0, 2); // edge internal to old polygon
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(1, quad_in.getAdjacentQuadEdgeIndex(1));
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(2, quad_in.getAdjacentQuadEdgeIndex(2));
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(3, 2); // edge internal to old polygon
				// If the polygon adjacent to this poly along edge 2 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_2 != -1 && !quads_in[adj_quad_2].dead && quads_in[adj_quad_2].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(2) == 2)
					quads_out[write_index + 2].setAdjacentQuadEdgeIndex(2, 3);

			
				if(adj_quad_0 != -1 && quads_in[adj_quad_0].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[0]].anchored = true; // Mark the edge midpoint vert along edge 0 as anchored.
				if(adj_quad_1 != -1 && quads_in[adj_quad_1].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[1]].anchored = true;
				if(adj_quad_2 != -1 && quads_in[adj_quad_2].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[2]].anchored = true;
				//if(adj_quad_3 != -1 && quads_in[adj_quad_3].dead)
				//	verts_out[quads_in[q].edge_midpoint_vert_index[3]].anchored = true;
			}
			else
			{
				/*
				Example for edge 1, where the adjacent quad for q is aq, and the winding order is the same:
				
				  v3_______2_______v2     v3_______2________v2
				  |    2   |    2   |     |    2   |    2   |
				  |3  q3  1|3  q2  1|     |3 aq3  1|3 aq2  1|
				3 |____0___|____0___| 1 3 |____0___|____0___| 1
				  |    2   |    2   |     |    2   |    2   |
				  |3  q0  1|3  q1  1|     |3 aq0  1|3 aq1  1|
				  |____0___|____0___|     |____0___|____0___|
				  v0       0       v1     v0       0       v1

				In this case q1 on edge 1 is adjcent to aq0, q2 on edge1 is adjacent to aq3.
				the adj quad edge index is 3.
				So q1 is adjacent to (3 + 1) % 4, q2 is adjacent to 3.


				Case when adjacent quad (here along edge 1) is dead:
				Result: child adjacent quad indices stay the same.

				  _________2_________     _________2_________
				  |    2   |    2   |     |                 |
				  |3  q3  1|3  q2  1|     |                 |
				3 |____0___|____0___| 1 3 |        aq       | 1
				  |    2   |    2   |     |                 |
				  |3  q0  1|3  q1  1|     |                 |
				  |____0___|____0___|     |_________________|
				           0                       0
				*/


				//--------------- bottom left quad ---------------
				if(adj_quad_0 != -1) // If there is an adjacent quad along edge 0:
				{
					DUQuad& adj_quad = quads_in[adj_quad_0]; // Adjacent quad along edge 0.
					if(adj_quad.dead)
						quads_out[write_index + 0].adjacent_quad_index[0] = -1;//adj_quad.child_quads_index;
					else
					{
						const int adj_c = c[0];// Which edge of the adjacent quad is adjacent to the current quad
						const int adj_quad_child_i = adj_quad.child_quads_index + (adj_quad.isTri() ? mod3(adj_c + 1) : mod4(adj_c + 1)); // adj_quad.child_quads_index is the index of the zeroth child.
						quads_out[write_index + 0].adjacent_quad_index[0] = adj_quad_child_i;
					}
				}
				quads_out[write_index + 0].adjacent_quad_index[1] = (int)write_index + 1;
				quads_out[write_index + 0].adjacent_quad_index[2] = (int)write_index + 3;
				if(adj_quad_3 != -1) // If adjacent quad along edge 3:
					quads_out[write_index + 0].adjacent_quad_index[3] = quads_in[adj_quad_3].dead ? -1/*quads_in[adj_quad_3].child_quads_index*/ : quads_in[adj_quad_3].child_quads_index + c[3]; // quads_in[adj_quad_3].child_quads[mod4(c[3] + 0)]; // We need to set adjacent_quad_index for this new quad
				/*quads_out[write_index + 0].setAdjacentQuadEdgeIndex(0, quad_in.getAdjacentQuadEdgeIndex(0));
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(1, 3); // edge internal to old polygon
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(2, 0); // edge internal to old polygon
				quads_out[write_index + 0].setAdjacentQuadEdgeIndex(3, quad_in.getAdjacentQuadEdgeIndex(3));*/

				// Get quad_in getAdjacentQuadEdgeIndex 0 and 3, with all other bits zeroed: 1100001100000000 & quad_in.bitfield
				{
				const uint32 quad_in_bits = 0xC300 & quad_in.bitfield.v;
				const uint32 masked_bits = (quads_out[write_index + 0].bitfield.v & 0xFFFF00FF) | 0xC00; // zero out bits 8-15, set getAdjacentQuadEdgeIndex 1 to the value 3.
				quads_out[write_index + 0].bitfield.v = masked_bits | quad_in_bits;
				}
				assert(quads_out[write_index + 0].getAdjacentQuadEdgeIndex(0) == quad_in.getAdjacentQuadEdgeIndex(0));
				assert(quads_out[write_index + 0].getAdjacentQuadEdgeIndex(1) == 3);
				assert(quads_out[write_index + 0].getAdjacentQuadEdgeIndex(2) == 0);
				assert(quads_out[write_index + 0].getAdjacentQuadEdgeIndex(3) == quad_in.getAdjacentQuadEdgeIndex(3));

				// If the polygon adjacent to this poly along edge 0 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_0 != -1 && !quads_in[adj_quad_0].dead && quads_in[adj_quad_0].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(0) == 2)
					quads_out[write_index + 0].setAdjacentQuadEdgeIndex(0, 3);

				//--------------- bottom right quad ---------------
				if(adj_quad_0 != -1) // If adjacent quad along edge 0:
					quads_out[write_index + 1].adjacent_quad_index[0] = quads_in[adj_quad_0].dead ? -1/*quads_in[adj_quad_0].child_quads_index*/ : quads_in[adj_quad_0].child_quads_index + c[0]; // child_quads[mod4(c[0] + 0)];
				if(adj_quad_1 != -1) // If adjacent quad along edge 1:
					quads_out[write_index + 1].adjacent_quad_index[1] = quads_in[adj_quad_1].dead ? -1/*quads_in[adj_quad_1].child_quads_index*/ : quads_in[adj_quad_1].child_quads_index + (quads_in[adj_quad_1].isTri() ? mod3(c[1] + 1) : mod4(c[1] + 1)); // child_quads[mod4(c[1] + 1)];
				quads_out[write_index + 1].adjacent_quad_index[2] = (int)write_index + 2;
				quads_out[write_index + 1].adjacent_quad_index[3] = (int)write_index + 0;
				/*quads_out[write_index + 1].setAdjacentQuadEdgeIndex(0, quad_in.getAdjacentQuadEdgeIndex(0));
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(1, quad_in.getAdjacentQuadEdgeIndex(1));
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(2, 0); // edge internal to old polygon
				quads_out[write_index + 1].setAdjacentQuadEdgeIndex(3, 1); // edge internal to old polygon*/
				// Get quad_in getAdjacentQuadEdgeIndex 0 and 1, with all other bits zeroed: 0000111100000000 & quad_in.bitfield
				{
				const uint32 quad_in_bits = 0xF00 & quad_in.bitfield.v;
				const uint32 masked_bits = (quads_out[write_index + 1].bitfield.v & 0xFFFF00FF) | 0x4000; // zero out bits 8-15, set getAdjacentQuadEdgeIndex 3 to the value 1.
				quads_out[write_index + 1].bitfield.v = masked_bits | quad_in_bits;
				}
				assert(quads_out[write_index + 1].getAdjacentQuadEdgeIndex(0) == quad_in.getAdjacentQuadEdgeIndex(0));
				assert(quads_out[write_index + 1].getAdjacentQuadEdgeIndex(1) == quad_in.getAdjacentQuadEdgeIndex(1));
				assert(quads_out[write_index + 1].getAdjacentQuadEdgeIndex(2) == 0);
				assert(quads_out[write_index + 1].getAdjacentQuadEdgeIndex(3) == 1);

				// If the polygon adjacent to this poly along edge 1 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_1 != -1 && !quads_in[adj_quad_1].dead && quads_in[adj_quad_1].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(1) == 2)
					quads_out[write_index + 1].setAdjacentQuadEdgeIndex(1, 3);


				//--------------- top right quad ---------------
				quads_out[write_index + 2].adjacent_quad_index[0] = (int)write_index + 1;
				if(adj_quad_1 != -1) // If adjacent quad along edge 1:
					quads_out[write_index + 2].adjacent_quad_index[1] = quads_in[adj_quad_1].dead ? -1/*quads_in[adj_quad_1].child_quads_index*/ : quads_in[adj_quad_1].child_quads_index + c[1]; //child_quads[mod4(c[1] + 0)];
				if(adj_quad_2 != -1) // If adjacent quad along edge 2:
					quads_out[write_index + 2].adjacent_quad_index[2] = quads_in[adj_quad_2].dead ? -1/*quads_in[adj_quad_2].child_quads_index*/ : quads_in[adj_quad_2].child_quads_index + (quads_in[adj_quad_2].isTri() ? mod3(c[2] + 1) : mod4(c[2] + 1)); // child_quads[mod4(c[2] + 1)];
				quads_out[write_index + 2].adjacent_quad_index[3] = (int)write_index + 3;
				/*quads_out[write_index + 2].setAdjacentQuadEdgeIndex(0, 2); // edge internal to old polygon
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(1, quad_in.getAdjacentQuadEdgeIndex(1));
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(2, quad_in.getAdjacentQuadEdgeIndex(2));
				quads_out[write_index + 2].setAdjacentQuadEdgeIndex(3, 1); // edge internal to old polygon*/
				// Get quad_in getAdjacentQuadEdgeIndex 1 and 2, with all other bits zeroed: 0011110000000000 & quad_in.bitfield
				{
				const uint32 quad_in_bits = 0x3C00 & quad_in.bitfield.v;
				const uint32 masked_bits = (quads_out[write_index + 2].bitfield.v & 0xFFFF00FF) | 0x4200; // zero out bits 8-15, set getAdjacentQuadEdgeIndex 0 to the value 2 and 3 to the value 1.
				quads_out[write_index + 2].bitfield.v = masked_bits | quad_in_bits;
				}
				assert(quads_out[write_index + 2].getAdjacentQuadEdgeIndex(0) == 2);
				assert(quads_out[write_index + 2].getAdjacentQuadEdgeIndex(1) == quad_in.getAdjacentQuadEdgeIndex(1));
				assert(quads_out[write_index + 2].getAdjacentQuadEdgeIndex(2) == quad_in.getAdjacentQuadEdgeIndex(2));
				assert(quads_out[write_index + 2].getAdjacentQuadEdgeIndex(3) == 1);

				// If the polygon adjacent to this poly along edge 2 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_2 != -1 && !quads_in[adj_quad_2].dead && quads_in[adj_quad_2].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(2) == 2)
					quads_out[write_index + 2].setAdjacentQuadEdgeIndex(2, 3);

				//--------------- top left quad ---------------
				quads_out[write_index + 3].adjacent_quad_index[0] = (int)write_index + 0;
				quads_out[write_index + 3].adjacent_quad_index[1] = (int)write_index + 2;
				if(adj_quad_2 != -1) // If adjacent quad along edge 2:
					quads_out[write_index + 3].adjacent_quad_index[2] = quads_in[adj_quad_2].dead ? -1/*quads_in[adj_quad_2].child_quads_index*/ : quads_in[adj_quad_2].child_quads_index + c[2]; // child_quads[mod4(c[2] + 0)];
				if(adj_quad_3 != -1) // If adjacent quad along edge 3:
					quads_out[write_index + 3].adjacent_quad_index[3] = quads_in[adj_quad_3].dead ? -1/*quads_in[adj_quad_3].child_quads_index*/ : quads_in[adj_quad_3].child_quads_index + (quads_in[adj_quad_3].isTri() ? mod3(c[3] + 1) : mod4(c[3] + 1)); // child_quads[mod4(c[3] + 1)];
				/*quads_out[write_index + 3].setAdjacentQuadEdgeIndex(0, 2); // edge internal to old polygon
				quads_out[write_index + 3].setAdjacentQuadEdgeIndex(1, 3); // edge internal to old polygon
				quads_out[write_index + 3].setAdjacentQuadEdgeIndex(2, quad_in.getAdjacentQuadEdgeIndex(2));
				quads_out[write_index + 3].setAdjacentQuadEdgeIndex(3, quad_in.getAdjacentQuadEdgeIndex(3));*/
				// Get quad_in getAdjacentQuadEdgeIndex 2 and 3, with all other bits zeroed: 1111000000000000 & quad_in.bitfield
				{
				const uint32 quad_in_bits = 0xF000 & quad_in.bitfield.v;
				const uint32 masked_bits = (quads_out[write_index + 3].bitfield.v & 0xFFFF00FF) | 0xE00; // zero out bits 8-15, set getAdjacentQuadEdgeIndex 0 to the value 2 and 1 to the value 3.
				quads_out[write_index + 3].bitfield.v = masked_bits | quad_in_bits;
				}
				assert(quads_out[write_index + 3].getAdjacentQuadEdgeIndex(0) == 2);
				assert(quads_out[write_index + 3].getAdjacentQuadEdgeIndex(1) == 3);
				assert(quads_out[write_index + 3].getAdjacentQuadEdgeIndex(2) == quad_in.getAdjacentQuadEdgeIndex(2));
				assert(quads_out[write_index + 3].getAdjacentQuadEdgeIndex(3) == quad_in.getAdjacentQuadEdgeIndex(3));

				// If the polygon adjacent to this poly along edge 3 is a triangle, and it is being subdivided, and the adjacent triangles edge was 2, then the new adjacent triangle edge should be 3.
				if(num_subdivs_done == 0 && adj_quad_3 != -1 && !quads_in[adj_quad_3].dead && quads_in[adj_quad_3].numSides() == 3 && quad_in.getAdjacentQuadEdgeIndex(3) == 2)
					quads_out[write_index + 3].setAdjacentQuadEdgeIndex(3, 3);

			
				if(adj_quad_0 != -1 && quads_in[adj_quad_0].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[0]].anchored = true; // Mark the edge midpoint vert along edge 0 as anchored.
				if(adj_quad_1 != -1 && quads_in[adj_quad_1].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[1]].anchored = true;
				if(adj_quad_2 != -1 && quads_in[adj_quad_2].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[2]].anchored = true;
				if(adj_quad_3 != -1 && quads_in[adj_quad_3].dead)
					verts_out[quads_in[q].edge_midpoint_vert_index[3]].anchored = true;
			}
		}
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("   Setting adjacent_quad_index: " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

#ifndef NDEBUG
	// Sanity check quads
	for(size_t q=0; q<quads_out.size(); ++q)
		for(int v=0; v<4; ++v)
		{
			const int adj_quad_i = quads_out[q].adjacent_quad_index[v];
			if(adj_quad_i != -1)
			{
				assert(adj_quad_i >= 0 && adj_quad_i < quads_out.size());
				const DUQuad& adj_quad = quads_out[adj_quad_i];

				// Check that the adjacent quad does indeed have this quad as its adjacent quad along the given edge.
				const int adjacent_quads_edge = quads_out[q].getAdjacentQuadEdgeIndex(v);
				assert(adjacent_quads_edge >= 0 && adjacent_quads_edge < adj_quad.numSides());
				assert(adj_quad.adjacent_quad_index[adjacent_quads_edge] == q);
			}
		}
#endif
	
	//TEMP:
	//const size_t quad_mem = quads_out.size() * sizeof(DUQuad);
	//const size_t vert_mem = verts_out.size() * sizeof(DUVertex);

	//conPrint("quad mem: " + toString(quads_out.size()) + " * " + toString(sizeof(DUQuad)  ) + " B = " + toString(quad_mem) + " B");
	//conPrint("vert mem: " + toString(verts_out.size()) + " * " + toString(sizeof(DUVertex)) + " B = " + toString(vert_mem) + " B");
}


//===================================================== Spline subdiv =================================================================


INDIGO_STRONG_INLINE const Vec3f removeComponentInDir(const Vec3f& v, const Vec3f& dir)
{
	return v - dir * dot(v, dir);
}


static inline float S(float x)
{
	return x*x*x*(x*(x*6 - 15) + 10);
	//return x*x*(3 - 2*x);
}


static const Vec3f curve(const Vec3f& k1, const Vec3f& k2, const Vec3f& d1, const Vec3f& d2, float t)
{
	float s = S(t);
	return (k1 + d1*t) * (1 - s) + (k2 - d2*(1-t)) * s;
}


void DisplacementUtils::splineSubdiv(
		Indigo::TaskManager& task_manager,
		PrintOutput& print_output,
		ThreadContext& context,
		Polygons& polygons_in,
		const VertsAndUVs& verts_and_uvs_in,
		unsigned int num_uv_sets,
		Polygons& polygons_out,
		VertsAndUVs& verts_and_uvs_out
	)
{
	const DUQuadVector& quads_in = polygons_in.quads;
	const DUVertexVector& verts_in = verts_and_uvs_in.verts;
	const UVVector& uvs_in = verts_and_uvs_in.uvs;

	DUQuadVector& quads_out = polygons_out.quads;
	DUVertexVector& verts_out = verts_and_uvs_out.verts;
	UVVector& uvs_out = verts_and_uvs_out.uvs;

	quads_out.resize(0);
	verts_out.resize(0);
	uvs_out = uvs_in; // Copy over uvs

	int N_U = 16;
	int N_V = 16;

	// For each quad
	for(size_t q = 0; q < quads_in.size(); ++q)
	{
		const Vec3f p_0 = verts_in[quads_in[q].vertex_indices[0]].pos;
		const Vec3f p_1 = verts_in[quads_in[q].vertex_indices[1]].pos;
		const Vec3f p_2 = verts_in[quads_in[q].vertex_indices[2]].pos;
		const Vec3f p_3 = verts_in[quads_in[q].vertex_indices[3]].pos;

		const Vec2f uv_0 = getUVs(uvs_in, num_uv_sets, quads_in[q].vertex_indices[0], 0);
		const Vec2f uv_1 = getUVs(uvs_in, num_uv_sets, quads_in[q].vertex_indices[1], 0);
		const Vec2f uv_2 = getUVs(uvs_in, num_uv_sets, quads_in[q].vertex_indices[2], 0);
		const Vec2f uv_3 = getUVs(uvs_in, num_uv_sets, quads_in[q].vertex_indices[3], 0);

		float scale = 0.4;

		const Vec3f k_0 = (removeComponentInDir(p_1 - p_0, verts_in[quads_in[q].vertex_indices[0]].normal)) * scale;
		const Vec3f k_1 = (removeComponentInDir(p_2 - p_1, verts_in[quads_in[q].vertex_indices[1]].normal)) * scale;
		const Vec3f k_2 = (removeComponentInDir(p_3 - p_2, verts_in[quads_in[q].vertex_indices[2]].normal)) * scale;
		const Vec3f k_3 = (removeComponentInDir(p_0 - p_3, verts_in[quads_in[q].vertex_indices[3]].normal)) * scale;

		const Vec3f b_0 = (removeComponentInDir(p_0 - p_3, verts_in[quads_in[q].vertex_indices[0]].normal)) * scale;
		const Vec3f b_1 = (removeComponentInDir(p_1 - p_0, verts_in[quads_in[q].vertex_indices[1]].normal)) * scale;
		const Vec3f b_2 = (removeComponentInDir(p_2 - p_1, verts_in[quads_in[q].vertex_indices[2]].normal)) * scale;
		const Vec3f b_3 = (removeComponentInDir(p_3 - p_2, verts_in[quads_in[q].vertex_indices[3]].normal)) * scale;

		for(int ui=0; ui<N_U; ++ui)
		for(int vi=0; vi<N_V; ++vi)
		{
			float u = (float)ui / N_U;
			float v = (float)vi / N_V;

			Vec3f q_i = curve(p_0, p_1, k_0, b_1, u);
			Vec3f q_i1 = curve(p_0, p_1, k_0, b_1, u + 1.0f/N_U); // q_{i+1}

			Vec3f q_i_primed = curve(p_3, p_2, -b_3, -k_2, u);
			Vec3f q_i1_primed = curve(p_3, p_2, -b_3, -k_2, u + 1.0f/N_U); // q'_{i+1}

			// Get interpolated tangent vector at q_i
			Vec3f k_i = -b_0 + (k_1 - (-b_0)) * S(u);
			Vec3f k_i1 = -b_0 + (k_1 - (-b_0)) * S(u + (1.0f/N_U));

			// Get interpolated tangent vector on other side at q_i'
			Vec3f k_i_primed = -k_3 + (b_2 - (-k_3)) * S(u);
			Vec3f k_i1_primed = -k_3 + (b_2 - (-k_3)) * S(u + 1.0f/N_U);

			Vec3f v_0 = curve(q_i,  q_i_primed,  k_i,  k_i_primed,  v);
			Vec3f v_1 = curve(q_i1, q_i1_primed, k_i1, k_i1_primed, v);
			Vec3f v_2 = curve(q_i1, q_i1_primed, k_i1, k_i1_primed, v + 1.0f/N_V);
			Vec3f v_3 = curve(q_i,  q_i_primed,  k_i,  k_i_primed,  v + 1.0f/N_V);

			int v_base = (int)verts_out.size();
			verts_out.push_back(DUVertex(v_0, Vec3f(0,0,1))); // TEMP HACK NORMAL
			verts_out.push_back(DUVertex(v_1, Vec3f(0,0,1))); // TEMP HACK NORMAL
			verts_out.push_back(DUVertex(v_2, Vec3f(0,0,1))); // TEMP HACK NORMAL
			verts_out.push_back(DUVertex(v_3, Vec3f(0,0,1))); // TEMP HACK NORMAL

			uvs_out.push_back(Maths::lerp(Maths::lerp(uv_0, uv_1, v), Maths::lerp(uv_2, uv_3, v), u));
			uvs_out.push_back(Maths::lerp(Maths::lerp(uv_0, uv_1, v), Maths::lerp(uv_2, uv_3, v), u + 1.0f/N_U));
			uvs_out.push_back(Maths::lerp(Maths::lerp(uv_0, uv_1, v), Maths::lerp(uv_2, uv_3, v + 1.0f/N_V), u));
			uvs_out.push_back(Maths::lerp(Maths::lerp(uv_0, uv_1, v), Maths::lerp(uv_2, uv_3, v + 1.0f/N_V), u + 1.0f/N_U));

			quads_out.push_back(DUQuad(
				v_base, v_base+1, v_base+2, v_base+3, quads_in[q].mat_index
			));

			//Vec3f t_i = curve(p_0, p_3, b_0, -k_3, v);
			//Vec3f t_i1 = curve(p_0, p_3, b_0, -k_3, v + (1.0f/N));

			//Vec3f t_i_primed = curve(p_1, p_2, k_1, -b_2, v);
			//Vec3f t_i1_primed =curve(p_1, p_2, k_1, -b_2, v + (1.0f/N));
		}
	}

}

//===================================================== Drawing code (for debugging) ========================================================


static void drawVertSS(Bitmap& map, const Vec2f& pos_ss, const Colour3f& col)
{
	int px = (int)(pos_ss.x * map.getWidth());
	int py = (int)((1 - pos_ss.y) * map.getHeight());

	const int r = 2;
	for(int x=px-r; x<px+r; ++x)
	for(int y=py-r; y<py+r; ++y)
		if(x >= 0 && y >= 0 && x < map.getWidth() && y < map.getHeight())
		{
			map.setPixelComp(x, y, 0, (uint8)myClamp(col.r * 255.0f, 0.f, 255.f));
			map.setPixelComp(x, y, 1, (uint8)myClamp(col.g * 255.0f, 0.f, 255.f));
			map.setPixelComp(x, y, 2, (uint8)myClamp(col.b * 255.0f, 0.f, 255.f));
		}
}


static void drawLineSS(Bitmap& map, const Colour3f& col, const Vec2f& start_ss, const Vec2f& end_ss)
{
	Vec2f start(start_ss.x * map.getWidth(), (1 - start_ss.y) * map.getHeight());
	Vec2f end  (end_ss.x   * map.getWidth(), (1 - end_ss.y)   * map.getHeight());

	Drawing::drawLine(map, col, start, end);
}


static void drawArrowSS(Bitmap& map, const Colour3f& col, const Vec2f& start_ss, const Vec2f& end_ss)
{
	Vec2f start(start_ss.x * map.getWidth(), (1 - start_ss.y) * map.getHeight());
	Vec2f end  (end_ss.x   * map.getWidth(), (1 - end_ss.y)   * map.getHeight());

	const Vec2f v = end - start;
	const Vec2f right(-v.y, v.x);

	Drawing::drawLine(map, col, start, end);

	// Draw arrow head
	Drawing::drawLine(map, col, end, start + v * 0.95f + right * 0.025f);
	Drawing::drawLine(map, col, end, start + v * 0.95f - right * 0.025f);
	Drawing::drawLine(map, col, start + v * 0.95f + right * 0.025f, start + v * 0.95f - right * 0.025f);
}



static void drawTextSS(Bitmap& map, const Vec2f& pos_ss, const std::string& label, TextDrawer& text_drawer)
{
	int px = (int)(pos_ss.x * map.getWidth());
	int py = (int)((1 - pos_ss.y) * map.getHeight());

	text_drawer.drawText(label, map, px, py - 10);
}


static Vec2f obToSS(const Vec3f& screen_botleft, const Vec3f& screen_right, const Vec3f& screen_up, const Vec3f& pos_os)
{
	return Vec2f(
		dot(pos_os - screen_botleft, screen_right) / screen_right.length2(),
		dot(pos_os - screen_botleft, screen_up) / screen_up.length2()
	);
}


// Draws the mesh, showing edges, verts etc..
void DisplacementUtils::draw(const Polygons& polygons, const VertsAndUVs& verts_and_uvs, int num_uv_sets, const std::string& image_path)
{
	try
	{
		const std::string indigo_base_dir_path = "C:/programming/indigo/output/vs2012/indigo_x64/Debug";//TEMP HACK
		TextDrawer text_drawer(
			indigo_base_dir_path + "/font.png",
			indigo_base_dir_path + "/font.ini"
		);

		const Vec3f screen_botleft(-12.f, -12.f, 0);
		const Vec3f screen_right(24.f, 0, 0);
		const Vec3f screen_up(0, 24.f, 0);

		const int res = 1000;
		Bitmap map(res, res, 3, NULL);
		map.zero();

		// Draw quads
		for(size_t q=0; q<polygons.quads.size(); ++q)
		{
			const int num_sides = polygons.quads[q].isTri() ? 3 : 4;
			const Vec3f quad_centre = 
				(verts_and_uvs.verts[polygons.quads[q].vertex_indices[0]].pos + 
				verts_and_uvs.verts[polygons.quads[q].vertex_indices[1]].pos + 
				verts_and_uvs.verts[polygons.quads[q].vertex_indices[2]].pos + 
				(num_sides == 3 ? Vec3f(0.f) : verts_and_uvs.verts[polygons.quads[q].vertex_indices[3]].pos)) * 0.25f;

			for(int v=0; v<num_sides; ++v)
			{
				const int vi = polygons.quads[q].vertex_indices[v];
				const int vi1 = polygons.quads[q].vertex_indices[num_sides == 3 ? mod3(v + 1) : mod4(v + 1)];

				const Vec3f start = verts_and_uvs.verts[vi].pos;
				const Vec3f end = verts_and_uvs.verts[vi1].pos;

				const Vec3f edge_midpoint = start + (end - start) * 0.45f;

				if(false)//polygons.quads[q].ancestor_edge_indices[v] == INVALID_EDGE_INDEX)
				{
					Colour3f col = Colour3f(0.5f);
					drawLineSS(map, col, obToSS(screen_botleft, screen_right, screen_up, start), obToSS(screen_botleft, screen_right, screen_up, end));
				}
				else
				{
					Colour3f col = Colour3f(0.6f, 0.6f, 0.6f);
					if(polygons.quads[q].isUVEdgeDiscontinuity(v) != 0)
						col = Colour3f(0.0f, 0.6f, 0.2f);
					//Drawing::drawLine(map, col, obToSS(screen_botleft, screen_right, screen_up, start) * (float)map.getWidth(), obToSS(screen_botleft, screen_right, screen_up, end)) * (float)map.getHeight());

					Vec3f use_start = start;
					Vec3f use_end = end;
					//if(polygons.quads[q].ancestor_edge_reversed[v])
					//	mySwap(use_start, use_end);
					drawArrowSS(map, col, obToSS(screen_botleft, screen_right, screen_up, use_start), obToSS(screen_botleft, screen_right, screen_up, use_end));
				}

				// Draw quad label
				const Vec3f q_text_offset(q * 0.0f, q * 0.2f, 0.f);
				drawTextSS(map, obToSS(screen_botleft, screen_right, screen_up, quad_centre + q_text_offset), "q=" + toString(q) /*+ ", c=" + toString(polygons.quads[q].chunk)*/, text_drawer);

				// Draw ancestor_edge_indices
				const Vec3f edge_label_text_offset(q * 0.0f, q * 0.2f, 0.f);
				const Vec3f ancestor_label_pos = edge_midpoint + (quad_centre - edge_midpoint) * 0.2f + edge_label_text_offset;
				//drawTextSS(map, obToSS(screen_botleft, screen_right, screen_up, ancestor_label_pos), "a=" + toString(polygons.quads[q].ancestor_edge_indices[v]), text_drawer);


				std::string edge_info;
				//if(polygons.quads[q].ancestor_edge_indices[v] == INVALID_EDGE_INDEX)
				//	edge_info = "no anc.";
				//else
				{
					edge_info += "e" + toString(v) + ",";
					edge_info += "a=" + toString(polygons.quads[q].adjacent_quad_index[v]);

					edge_info += ",c=" + toString(polygons.quads[q].getAdjacentQuadEdgeIndex(v));

					//edge_info += ",d=" + toString(polygons.quads[q].edge_depth[v]) + ",of=" + toString(polygons.quads[q].edge_offset[v]);
					//if(polygons.quads[q].ancestor_edge_reversed[v])
					//	edge_info += " (rvsd)";
				}
				drawTextSS(map, obToSS(screen_botleft, screen_right, screen_up, ancestor_label_pos), edge_info, text_drawer);

				// Draw vertex v
				const Vec3f v_p = verts_and_uvs.verts[vi].pos;
				const Colour3f vertcol = verts_and_uvs.verts[vi].uv_discontinuity ? Colour3f(0.2f, 0.8f, 0.2f) : Colour3f(0.8f); //verts_and_uvs.verts[vi].anchored ? Colour3f(0.2f, 0.8f, 0.2f) : Colour3f(0.8f);
				drawVertSS(map, obToSS(screen_botleft, screen_right, screen_up, v_p), vertcol);
				
				// Draw vert label
				Vec3f label_pos = v_p + (quad_centre - v_p) * 0.2f;
				if(v == 1 || v == 2)
					label_pos += Vec3f(0, 0.4f, 0.f);
				std::string vert_label = "v_" + toString(vi);
				if(!verts_and_uvs.uvs.empty())
					vert_label += " " + getUVs(verts_and_uvs.uvs, num_uv_sets, vi, 0).toString();
				drawTextSS(map, obToSS(screen_botleft, screen_right, screen_up, label_pos), vert_label, text_drawer);
			}
		}

		//// Draw vertices
		//for(size_t v=0; v<verts_and_uvs.verts.size(); ++v)
		//{
		//	const Vec3f v_p = verts_and_uvs.verts[v].pos;
		//	const Vec3f draw_v_p = v_p + (quad_centre - v_p) * 0.2f;
		//	drawVertSS(map, obToSS(screen_botleft, screen_right, screen_up, ), Colour3f(0.8f), "v_" + toString(v), text_drawer);
		//}

		PNGDecoder::write(map, image_path);
	}
	catch(TextDrawerExcep& e)
	{
		conPrint("TextDrawerExcep: " + e.what());
	}
	catch(ImFormatExcep& e)
	{
		conPrint("Error while writing: " + e.what());
	}
}


//===================================================== End drawing code ========================================================


#if BUILD_TESTS


#include "../graphics/Map2D.h"
#include "../dll/RendererTests.h"


void DisplacementUtils::test(const std::string& indigo_base_dir_path, const std::string& appdata_path, bool run_comprehensive_tests)
{
	conPrint("DisplacementUtils::test()");

#if IS_INDIGO
	if(run_comprehensive_tests)
	{
		// Build and (briefly) render all/most of the displacement + subdiv test scenes.
		const char* scenes[] = {

			"grid_mesh_displacement_test.igs",
			"no_subdivision_cube_gap_test_constant_displacement.igs",

			"subdivision_cube_gap_test.igs",
			"subdivision_cube_gap_test_constant_displacement.igs",
			"subdivision_cube_gap_spike_displacement_test.igs",
			"shading_edge_displacement_test.igs",

			"subdivision_tri_and_quad_test.igs",
			"subdivision_tri_and_quad_test2.igs",
			"subdivision_tri_and_quad_test3.igs",
			"subdivision_tri_and_quad_test4.igs",
			"subdivision_tri_and_quad_test5.igs",
			"subdivision_quad_test2.igs",
			//"adaptive_displacement_shader_test.igs", // hi poly
			"adaptive_quad_shader_displacement_test4.igs",
			"inconsistent_winding_tri_test_consistent_ref.igs",
			"inconsistent_winding_tri_test.igs",
			"inconsistent_winding_tri_and_quad_test.igs",
			"inconsistent_winding_test2.igs",
			"inconsistent_winding_test.igs",
			"subdivision_edge_diff_vert_test2.igs",
			"subdivision_edge_diff_vert_test.igs",
			//"subdivision_curvature_test.igs", // hi poly
			"uv_discontinuity_test.igs",
			"uv_discontinuity_test2.igs",
			"uv_discontinuity_test3.igs",
			"uv_discontinuity_test4.igs",
			"uv_discontinuity_test5.igs",
			//"adaptive_quad_shader_displacement_test.igs",// hi poly
			"adaptive_quad_shader_displacement_test2.igs",
			"adaptive_quad_shader_displacement_test3.igs",
			"adaptive_quad_shader_displacement_test4.igs",
			"adaptive_quad_shader_displacement_test5.igs",
			"adaptive_quad_shader_displacement_test6.igs",
			"adaptive_quad_shader_displacement_test7.igs",
			"adaptive_quad_shader_displacement_test8.igs",
			"tri_uv_discontinuity_test.igs",
			"subdivision_tri_quad_test.igs",
			"bend_test.igs",
			//"adaptive_quad_fbm_shader_displacement_test.igs", // hi poly
			//"adaptive_quad_shader_displacement_test.igs", // hi poly
			"quad_displacement_test.igs",
			"adaptive_tri_blend_displacement_test.igs",
			"adaptive_quad_blend_displacement_test.igs",
			"adaptive_tri_tex_shader_displacement_test.igs",
			"adaptive_quad_tex_shader_displacement_test.igs",
			"adaptive_quad_displacement_test.igs",
			"adaptive_tri_displacement_test.igs",
			"blend_material_root_displacement_test.igs",
			"subdivision_quad_test.igs",
			"zomb_head.igs",
			"subdiv_three_tris_adjacent_to_edge_test.igs",
			"subdiv_three_quads_adjacent_to_edge_test.igs"
		};

		try
		{
			for(size_t i = 0; i<sizeof(scenes) / sizeof(const char*); ++i)
			{
				const std::string fullpath = TestUtils::getIndigoTestReposDir() + "/testscenes/" + scenes[i];
				Indigo::RendererTests::renderTestScene(indigo_base_dir_path, appdata_path, fullpath, /*save_render=*/true);
			}
		}
		catch(Indigo::Exception& e)
		{
			failTest(e.what());
		}
	}
#endif

	/////////////////////	
	//{
	//	std::vector<RayMeshVertex> vertices(4);
	//	std::vector<Vec2f> uvs(4);
	//	js::Vector<RayMeshTriangle, 16> triangles;
	//	js::Vector<RayMeshQuad, 16> quads(1);

	//	js::Vector<RayMeshTriangle, 16> triangles_out;
	//	std::vector<RayMeshVertex> verts_out;
	//	std::vector<Vec2f> uvs_out;


	//	// Quad vertices in CCW order from top right, facing up
	//	vertices[0] = RayMeshVertex(Vec3f( 1,  1, 0), Vec3f(0, 0, 1));
	//	vertices[1] = RayMeshVertex(Vec3f(-1,  1, 0), Vec3f(0, 0, 1));
	//	vertices[2] = RayMeshVertex(Vec3f(-1, -1, 0), Vec3f(0, 0, 1));
	//	vertices[3] = RayMeshVertex(Vec3f( 1, -1, 0), Vec3f(0, 0, 1));

	//	uvs[0] = Vec2f(1, 1);
	//	uvs[1] = Vec2f(0, 1);
	//	uvs[2] = Vec2f(0, 0);
	//	uvs[3] = Vec2f(1, 0);


	//	quads[0] = RayMeshQuad(0, 1, 2, 3, 0);
	//	quads[0].uv_indices[0] = 0;
	//	quads[0].uv_indices[1] = 1;
	//	quads[0].uv_indices[2] = 2;
	//	quads[0].uv_indices[3] = 3;

	//	StandardPrintOutput print_output;
	//	ThreadContext context;

	//	std::vector<Reference<MaterialBinding> > materials;
	//	materials.push_back(Reference<MaterialBinding>(new MaterialBinding(Reference<Material>(new Diffuse(
	//		std::vector<TextureUnit*>(),
	//		Reference<SpectrumMatParameter>(NULL),
	//		Reference<DisplaceMatParameter>(NULL),
	//		Reference<DisplaceMatParameter>(NULL),
	//		Reference<SpectrumMatParameter>(NULL),
	//		Reference<SpectrumMatParameter>(NULL),
	//		0, // layer_index
	//		false // random_triangle_colours
	//	)))));

	//	materials[0]->uv_set_indices.push_back(0);

	//	DUOptions options;
	//	options.object_to_camera = Matrix4f::identity();
	//	options.wrap_u = false;
	//	options.wrap_v = false;
	//	options.view_dependent_subdivision = false;
	//	options.pixel_height_at_dist_one = 1;
	//	options.subdivide_pixel_threshold = 0;
	//	options.subdivide_curvature_threshold = 0;
	//	options.displacement_error_threshold = 0;
	//	options.max_num_subdivisions = 1;

	//	DisplacementUtils::subdivideAndDisplace(
	//		print_output,
	//		context,
	//		materials,
	//		true, // smooth
	//		triangles,
	//		quads,
	//		vertices,
	//		uvs,
	//		1, // num uv sets
	//		options,
	//		triangles_out,
	//		verts_out,
	//		uvs_out
	//	);

	//	for(size_t i=0; i<verts_out.size(); ++i)
	//	{
	//		conPrint(toString(verts_out[i].pos.x) + ", " + toString(verts_out[i].pos.y) + ", " + toString(verts_out[i].pos.z));
	//	}

	//	for(size_t i=0; i<triangles_out.size(); ++i)
	//	{
	//		conPrint(toString(triangles_out[i].vertex_indices[0]) + ", " + toString(triangles_out[i].vertex_indices[1]) + ", " + toString(triangles_out[i].vertex_indices[2]));

	//		for(int v=0; v<3; ++v)
	//		{
	//			conPrint("uv " + toString(uvs_out[triangles_out[i].uv_indices[v]].x) + ", " + toString(uvs_out[triangles_out[i].uv_indices[v]].y));
	//		}
	//	}
	//}
	Indigo::TaskManager task_manager(1);

#if 0
	//=========================== Test subdivision of quads ==========================
	{
		RayMesh::VertexVectorType vertices(6);
		std::vector<Vec2f> uvs(8);
		RayMesh::TriangleVectorType triangles;
		RayMesh::QuadVectorType quads(2);

		RayMesh::TriangleVectorType triangles_out;
		RayMesh::VertexVectorType verts_out;
		std::vector<Vec2f> uvs_out;


		// Quad vertices in CCW order from top right, facing up
		vertices[0] = RayMeshVertex(Vec3f(0.9f,  1, 0), Vec3f(0, 0, 1), 0);
		vertices[1] = RayMeshVertex(Vec3f(1,  1, 0), Vec3f(0, 0, 1), 0);
		vertices[2] = RayMeshVertex(Vec3f(1.1f, 1, 0), Vec3f(0, 0, 1), 0);
		vertices[3] = RayMeshVertex(Vec3f(0.9f,  0, 0), Vec3f(0, 0, 1), 0);
		vertices[4] = RayMeshVertex(Vec3f(1,  0, 0), Vec3f(0, 0, 1), 0);
		vertices[5] = RayMeshVertex(Vec3f(1.1f, 0, 0), Vec3f(0, 0, 1), 0);

		uvs[0] = Vec2f(0.9f, 0.6f);
		uvs[1] = Vec2f(1.0f, 0.6f);
		uvs[2] = Vec2f(0, 0.6f);
		uvs[3] = Vec2f(0.1f, 0.6f);
		uvs[4] = Vec2f(0.9f, 0.4f);
		uvs[5] = Vec2f(1.0f, 0.4f);
		uvs[6] = Vec2f(0, 0.4f);
		uvs[7] = Vec2f(0.1f, 0.4f);


		quads[0] = RayMeshQuad(0, 3, 4, 1, 0, RayMesh_NoShadingNormals);
		quads[0].uv_indices[0] = 0;
		quads[0].uv_indices[1] = 4;
		quads[0].uv_indices[2] = 5;
		quads[0].uv_indices[3] = 1;

		quads[1] = RayMeshQuad(1, 4, 5, 2, 0, RayMesh_NoShadingNormals);
		quads[1].uv_indices[0] = 2;
		quads[1].uv_indices[1] = 6;
		quads[1].uv_indices[2] = 7;
		quads[1].uv_indices[3] = 3;

		StandardPrintOutput print_output;
		ThreadContext context;

		std::vector<Reference<Material> > materials;
		materials.push_back(Reference<Material>(new Diffuse(
			Reference<SpectrumMatParameter>(NULL),
			Reference<DisplaceMatParameter>(NULL),
			Reference<DisplaceMatParameter>(NULL),
			Reference<SpectrumMatParameter>(NULL),
			Reference<SpectrumMatParameter>(NULL),
			Reference<Vec3MatParameter>(),
			0, // layer_index
			false, // random_triangle_colours
			false // backface_emit
		)));

		//materials[0]->uv_set_indices.push_back(0);

		DUOptions options;
		options.object_to_camera = Matrix4f::identity();
		options.view_dependent_subdivision = false;
		options.pixel_height_at_dist_one = 1;
		options.subdivide_pixel_threshold = 0;
		options.subdivide_curvature_threshold = 0;
		options.displacement_error_threshold = 0;
		options.max_num_subdivisions = 1;

		DisplacementUtils::subdivideAndDisplace(
			"test mesh",
			task_manager,
			print_output,
			context,
			materials,
			true, // smooth
			triangles,
			quads,
			vertices,
			uvs,
			1, // num uv sets
			options,
			false, // use_shading_normals
			triangles_out,
			verts_out,
			uvs_out
		);

		conPrint("Vertex positions");
		for(size_t i = 0; i < verts_out.size(); ++i)
		{
			conPrint("vert " + toString((uint64)i) + ": " + toString(verts_out[i].pos.x) + ", " + toString(verts_out[i].pos.y) + ", " + toString(verts_out[i].pos.z));
		}

		conPrint("UVs");
		for(size_t i = 0; i < uvs_out.size(); ++i)
		{
			conPrint("UV " + toString((uint64)i) + ": " + toString(uvs_out[i].x) + ", " + toString(uvs_out[i].y));
		}

		for(size_t i = 0; i < triangles_out.size(); ++i)
		{
			conPrint(toString(triangles_out[i].vertex_indices[0]) + ", " + toString(triangles_out[i].vertex_indices[1]) + ", " + toString(triangles_out[i].vertex_indices[2]));

			for(size_t v = 0; v < 3; ++v)
			{
				conPrint("uv " + toString(uvs_out[triangles_out[i].uv_indices[v]].x) + ", " + toString(uvs_out[triangles_out[i].uv_indices[v]].y));
			}
		}
	}

	//=========================== Test subdivision of a mesh composed of both tris and quads ==========================
	{
		RayMesh::VertexVectorType vertices(6);
		std::vector<Vec2f> uvs(8);
		RayMesh::TriangleVectorType triangles(2);
		RayMesh::QuadVectorType quads(1);

		RayMesh::TriangleVectorType triangles_out;
		RayMesh::VertexVectorType verts_out;
		std::vector<Vec2f> uvs_out;


		// Quad vertices in CCW order from top right, facing up
		vertices[0] = RayMeshVertex(Vec3f(0.9f,  1, 0), Vec3f(0, 0, 1), 0);
		vertices[1] = RayMeshVertex(Vec3f(1,  1, 0), Vec3f(0, 0, 1), 0);
		vertices[2] = RayMeshVertex(Vec3f(1.1f, 1, 0), Vec3f(0, 0, 1), 0);
		vertices[3] = RayMeshVertex(Vec3f(0.9f,  0, 0), Vec3f(0, 0, 1), 0);
		vertices[4] = RayMeshVertex(Vec3f(1,  0, 0), Vec3f(0, 0, 1), 0);
		vertices[5] = RayMeshVertex(Vec3f(1.1f, 0, 0), Vec3f(0, 0, 1), 0);

		uvs[0] = Vec2f(0.9f, 0.6f);
		uvs[1] = Vec2f(1.0f, 0.6f);
		uvs[2] = Vec2f(0, 0.6f);
		uvs[3] = Vec2f(0.1f, 0.6f);
		uvs[4] = Vec2f(0.9f, 0.4f);
		uvs[5] = Vec2f(1.0f, 0.4f);
		uvs[6] = Vec2f(0, 0.4f);
		uvs[7] = Vec2f(0.1f, 0.4f);


		quads[0] = RayMeshQuad(0, 3, 4, 1, 0, RayMesh_NoShadingNormals);
		quads[0].uv_indices[0] = 0;
		quads[0].uv_indices[1] = 4;
		quads[0].uv_indices[2] = 5;
		quads[0].uv_indices[3] = 1;

		// NOTE: not sure if these indices make sense.
		triangles[0] = RayMeshTriangle(1, 4, 5, 0, RayMesh_NoShadingNormals);
		triangles[0].uv_indices[0] = 2;
		triangles[0].uv_indices[1] = 6;
		triangles[0].uv_indices[2] = 7;

		triangles[1] = RayMeshTriangle(2, 4, 5, 0, RayMesh_NoShadingNormals);
		triangles[1].uv_indices[0] = 2;
		triangles[1].uv_indices[1] = 6;
		triangles[1].uv_indices[2] = 7;

		StandardPrintOutput print_output;
		ThreadContext context;

		std::vector<Reference<Material> > materials;
		materials.push_back(Reference<Material>(new Diffuse(
			Reference<SpectrumMatParameter>(NULL),
			Reference<DisplaceMatParameter>(NULL),
			Reference<DisplaceMatParameter>(NULL),
			Reference<SpectrumMatParameter>(NULL),
			Reference<SpectrumMatParameter>(NULL),
			Reference<Vec3MatParameter>(),
			0, // layer_index
			false, // random_triangle_colours
			false // backface_emit
		)));

		DUOptions options;
		options.object_to_camera = Matrix4f::identity();
		options.view_dependent_subdivision = false;
		options.pixel_height_at_dist_one = 1;
		options.subdivide_pixel_threshold = 0;
		options.subdivide_curvature_threshold = 0;
		options.displacement_error_threshold = 0;
		options.max_num_subdivisions = 1;

		DisplacementUtils::subdivideAndDisplace(
			"test mesh",
			task_manager,
			print_output,
			context,
			materials,
			true, // smooth
			triangles,
			quads,
			vertices,
			uvs,
			1, // num uv sets
			options,
			false, // use_shading_normals
			triangles_out,
			verts_out,
			uvs_out
		);

		// Each tri gets subdivided into 3 quads, for a total of 2*3 = 6 quads = 12 tris
		// Plus 4 quads * 2 tris/quad for the subdivided quad gives 12 + 8 = 20 tris.
		testAssert(triangles_out.size() == 20);
	}
#endif
}


#endif // BUILD_TESTS
