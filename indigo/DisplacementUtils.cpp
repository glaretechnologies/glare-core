/*=====================================================================
DisplacementUtils.cpp
---------------------
File created by ClassTemplate on Thu May 15 20:31:26 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "DisplacementUtils.h"

/*

The subdivision scheme used here is based on the paper
'A Factored Approach to Subdivision Surfaces'
by Joe Warren and Scott Schaefer

*/

#include "../maths/Rect2.h"
#include "../maths/mathstypes.h"
#include "../graphics/TriBoxIntersection.h"
#include "ScalarMatParameter.h"
#include "Vec3MatParameter.h"
#include "VoidMedium.h"
#include "TestUtils.h"
#include "globals.h"
#include "../utils/StringUtils.h"
#include "PrintOutput.h"
#include "StandardPrintOutput.h"
#include "ThreadContext.h"
#include "Diffuse.h"
#include "SpectrumMatParameter.h"
#include "DisplaceMatParameter.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../dll/include/IndigoMap.h"
#include "../utils/TaskManager.h"
#include "../utils/IndigoAtomic.h"
#include <unordered_map>
#include <sparsehash/dense_hash_map>


static const bool PROFILE = true;
#define DISPLACEMENT_UTILS_STATS 1
#if DISPLACEMENT_UTILS_STATS
#define DISPLACEMENT_CREATE_TIMER(timer) Timer (timer)
#define DISPLACEMENT_RESET_TIMER(timer) ((timer).reset())
#define DISPLACEMENT_PRINT_RESULTS(expr) (expr)
#else
#define DISPLACEMENT_CREATE_TIMER(timer)
#define DISPLACEMENT_RESET_TIMER(timer)
#define DISPLACEMENT_PRINT_RESULTS(expr)
#endif


DisplacementUtils::DisplacementUtils()
{	
}


DisplacementUtils::~DisplacementUtils()
{	
}


inline static uint32 mod2(uint32 x)
{
	return x & 0x1;
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


static inline const Vec3f triGeometricNormal(const std::vector<DUVertex>& verts, uint32_t v0, uint32_t v1, uint32_t v2)
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


static inline uint32_t uvIndex(uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	return uv_index * num_uv_sets + set_index;
}


static inline Vec2f& getUVs(std::vector<Vec2f>& uvs, uint32_t num_uv_sets, uint32_t uv_index, uint32_t set_index)
{
	assert(num_uv_sets > 0);
	assert(set_index < num_uv_sets);
	return uvs[uv_index * num_uv_sets + set_index];
}


static void computeVertexNormals(const std::vector<DUTriangle>& triangles,
								 const std::vector<DUQuad>& quads,
								 std::vector<DUVertex>& vertices)
{
	// Initialise vertex normals to null vector
	for(size_t i = 0; i < vertices.size(); ++i) vertices[i].normal = Vec3f(0, 0, 0);

	// Add quad face normals to constituent vertices' normals
	for(size_t q = 0; q < quads.size(); ++q)
	{
		const Vec3f quad_normal = triGeometricNormal(vertices, quads[q].vertex_indices[0], quads[q].vertex_indices[1], quads[q].vertex_indices[2]);

		for(uint32_t i = 0; i < 4; ++i)
			vertices[quads[q].vertex_indices[i]].normal += quad_normal;
	}

	// Add triangle face normals to constituent vertices' normals
	for(size_t t = 0; t < triangles.size(); ++t)
	{
		const Vec3f tri_normal = triGeometricNormal(
				vertices, 
				triangles[t].vertex_indices[0], 
				triangles[t].vertex_indices[1], 
				triangles[t].vertex_indices[2]
			);

		for(size_t i = 0; i < 3; ++i)
			vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
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
typedef std::unordered_map<RayMeshVertex, unsigned int, RayMeshVertexHash> VertToIndexMap;


struct EdgeInfo
{
	EdgeInfo() : num_adjacent_polys(0), uv_discontinuity(false) {}
	EdgeInfo(const DUVertIndexPair& old_edge_, int num_adjacent_polys_) : old_edge(old_edge_), num_adjacent_polys(num_adjacent_polys_), uv_discontinuity(false) {}

	DUVertIndexPair old_edge;
	int num_adjacent_polys;

	Vec2f start_uv;
	Vec2f end_uv;
	bool uv_discontinuity;
};


class DUEdgeInfo
{
public:
	DUEdgeInfo()
	:	midpoint_vert_index(0), 
		num_adjacent_subdividing_polys(0), 
		border(false) 
	{}
	~DUEdgeInfo(){}
	unsigned int midpoint_vert_index;
	unsigned int num_adjacent_subdividing_polys;
	bool border;

	// If the edge endpoints have these uv indices, then the new midpoint UV index is given by prim_a_midpoint_uv_index, otherwise prim_b_midpoint_uv_index.
	uint32 prim_a_uv_i_start;
	uint32 prim_a_uv_i_end;
	unsigned int prim_a_midpoint_uv_index;
	unsigned int prim_b_midpoint_uv_index;
};



//std::map<DUVertIndexPair, DUEdgeInfo> edge_info_map;
//std::map<DUVertIndexPair, unsigned int> uv_edge_map;
typedef google::dense_hash_map<DUVertIndexPair, DUEdgeInfo, DUVertIndexPairHash> EdgeInfoMapType;
typedef google::dense_hash_map<DUVertIndexPair, unsigned int, DUVertIndexPairHash> UVEdgeMapType;
//typedef std::tr1::unordered_map<DUVertIndexPair, DUEdgeInfo, DUVertIndexPairHash> EdgeInfoMapType;
//typedef std::tr1::unordered_map<DUVertIndexPair, unsigned int, DUVertIndexPairHash> UVEdgeMapType;


struct DUScratchInfo
{
	EdgeInfoMapType edge_info_map;
};


void DisplacementUtils::subdivideAndDisplace(
	const std::string& mesh_name,
	Indigo::TaskManager& task_manager,
	PrintOutput& print_output,
	ThreadContext& context,
	const std::vector<Reference<Material> >& materials,
	bool subdivision_smoothing,
	const RayMesh::TriangleVectorType& triangles_in, 
	const RayMesh::QuadVectorType& quads_in,
	const RayMesh::VertexVectorType& vertices_in,
	const std::vector<Vec2f>& uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	bool use_shading_normals,
	RayMesh::TriangleVectorType& tris_out, 
	RayMesh::VertexVectorType& verts_out,
	std::vector<Vec2f>& uvs_out
	)
{
	if(PROFILE) conPrint("\n-----------subdivideAndDisplace-----------");
	if(PROFILE) conPrint("mesh: " + mesh_name);

	Timer total_timer; // This one is used to create a message that is always printed.
	DISPLACEMENT_CREATE_TIMER(timer);

	// We will maintain two sets of geometry information, and 'ping-pong' between them.
	Polygons temp_polygons_1;
	Polygons temp_polygons_2;

	VertsAndUVs temp_verts_uvs_1;
	VertsAndUVs temp_verts_uvs_2;
	

	// Copying incoming data to temp_polygons_1, temp_verts_uvs_1.
	{ 
		std::vector<DUTriangle>& temp_tris = temp_polygons_1.tris;
		std::vector<DUQuad>& temp_quads = temp_polygons_1.quads;
		std::vector<DUEdge>& temp_edges = temp_polygons_1.edges;
		std::vector<DUVertexPolygon>& temp_vert_polygons = temp_polygons_1.vert_polygons;

		std::vector<DUVertex>& temp_verts = temp_verts_uvs_1.verts;
		std::vector<Vec2f>& temp_uvs = temp_verts_uvs_1.uvs;

		temp_tris.resize(triangles_in.size());
		temp_quads.resize(quads_in.size());
		temp_verts.reserve(vertices_in.size());
		temp_uvs.resize((temp_polygons_1.tris.size() * 3 + temp_polygons_1.quads.size() * 4) * num_uv_sets);
		uint32 temp_uvs_next_i = 0; // Next index in temp_uvs to write to.
		uint32 new_uv_index = 0; // = temp_uvs_next_i / num_uv_sets


		const float UV_DIST2_THRESHOLD = 0.001f * 0.001f;

		// Map from new edge (smaller new vert index, larger new vert index) to first old edge: (smaller old vert index, larger old vert index).
		// If there are different old edges, then this edge is a discontinuity.

		std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash> new_edge_info;


		VertToIndexMap new_vert_indices;

		const size_t triangles_in_size = triangles_in.size();
		for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
		{
			const RayMeshTriangle& tri = triangles_in[t];

			// Explode UVs
			for(unsigned int i = 0; i < 3; ++i)
			{
				// Create new UV
				for(unsigned int z = 0; z < num_uv_sets; ++z)
					temp_uvs[temp_uvs_next_i++] = getUVs(uvs_in, num_uv_sets, tri.uv_indices[i], z);
			
				// Set new tri UV index
				temp_tris[t].uv_indices[i] = new_uv_index;
				new_uv_index++;
			}

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
					new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
				}
				else
					new_vert_index = (*result).second; // Use existing vertex index.

				temp_tris[t].vertex_indices[i] = new_vert_index;
				tri_new_vert_indices[i] = new_vert_index;
			}

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
							temp_edges.push_back(DUEdge(tri_new_vert_indices[i], tri_new_vert_indices[i1], temp_tris[t].uv_indices[i], temp_tris[t].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
					}

					// Check for uv discontinuity:
					if((num_uv_sets > 0) && ((start_uv.getDist2(result->second.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(result->second.end_uv) > UV_DIST2_THRESHOLD)))
						result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity

					result->second.num_adjacent_polys++;
				}
			}

			temp_tris[t].tri_mat_index = tri.getTriMatIndex();
			temp_tris[t].dead = false;
		}


		const size_t quads_in_size = quads_in.size();
		for(size_t q = 0; q < quads_in_size; ++q) // For each quad
		{
			const RayMeshQuad& quad = quads_in[q];

			// Explode UVs
			for(unsigned int i = 0; i < 4; ++i)
			{
				// Create new UV
				for(unsigned int z = 0; z < num_uv_sets; ++z)
					temp_uvs[temp_uvs_next_i++] = getUVs(uvs_in, num_uv_sets, quad.uv_indices[i], z);
			
				// Set new quad UV index
				temp_quads[q].uv_indices[i] = new_uv_index;
				new_uv_index++;
			}

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
					new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
				}
				else
					new_vert_index = (*result).second; // Use existing vertex index.

				temp_quads[q].vertex_indices[i] = new_vert_index;
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

						if((edge_1_smaller_n != edge_2_smaller_n) || (edge_1_greater_n != edge_2_greater_n)) // If shading normals are different on the old edge vertices:
							temp_edges.push_back(DUEdge(quad_new_vert_indices[i], quad_new_vert_indices[i1], temp_quads[q].uv_indices[i], temp_quads[q].uv_indices[i1])); // Add crease edge.  Using new vert and UV indices here.
					}

					// Check for uv discontinuity:
					if((num_uv_sets > 0) && ((start_uv.getDist2(result->second.start_uv) > UV_DIST2_THRESHOLD) || (end_uv.getDist2(result->second.end_uv) > UV_DIST2_THRESHOLD)))
						result->second.uv_discontinuity = true; // Mark edge as having a UV discontinuity

					result->second.num_adjacent_polys++;
				}
			}

			temp_quads[q].mat_index = quad.getMatIndex();
			temp_quads[q].dead = false;
		}

		DISPLACEMENT_PRINT_RESULTS(conPrint("Creating temp verts/tris/quads: " + timer.elapsedStringNPlaces(5)));
		DISPLACEMENT_RESET_TIMER(timer);

		//========================== Create Boundary edge polygons, mark UV discontinuities ==========================
		// Now that we have marked all edges that have UV discontinuities, do a pass over the (temp) primitives to put that information in the primitives themselves.
		// Create boundary edge polygons while we are doing this as well.

		for(size_t t = 0; t < triangles_in_size; ++t) // For each tri
		{
			DUTriangle& tri = temp_tris[t];
			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const unsigned int i1 = mod3(i + 1); // Next vert
				const DUVertIndexPair edge_key(myMin(tri.vertex_indices[i], tri.vertex_indices[i1]), myMax(tri.vertex_indices[i], tri.vertex_indices[i1]));

				const std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = new_edge_info.find(edge_key);
				assert(result != new_edge_info.end());

				tri.edge_uv_discontinuity[i] = result->second.uv_discontinuity;

				if(result->second.num_adjacent_polys == 1) // If this is a boundary edge (has only 1 adjacent poly):
					temp_edges.push_back(DUEdge(tri.vertex_indices[i], tri.vertex_indices[i1], tri.uv_indices[i], tri.uv_indices[i1]));
			}
		}
	
		for(size_t q = 0; q < quads_in_size; ++q) // For each quad
		{
			DUQuad& quad = temp_quads[q];
			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const unsigned int i1 = mod4(i + 1); // Next vert
				const DUVertIndexPair edge_key(myMin(quad.vertex_indices[i], quad.vertex_indices[i1]), myMax(quad.vertex_indices[i], quad.vertex_indices[i1]));

				const std::unordered_map<DUVertIndexPair, EdgeInfo, DUVertIndexPairHash>::iterator result = new_edge_info.find(edge_key);
				assert(result != new_edge_info.end());

				//conPrint("!!! Quad " + toString(q) + " has UV disc. on edge " + toString(i));
				quad.edge_uv_discontinuity[i] = result->second.uv_discontinuity;

				if(result->second.num_adjacent_polys == 1) // If this is a boundary edge (has only 1 adjacent poly):
					temp_edges.push_back(DUEdge(quad.vertex_indices[i], quad.vertex_indices[i1], quad.uv_indices[i], quad.uv_indices[i1]));
			}
		}

		assert(temp_uvs_next_i == (uint32)temp_uvs.size());

		//conPrint("num boundary edges: " + toString(temp_edges.size()));

		DISPLACEMENT_PRINT_RESULTS(conPrint("Create Boundary edge polygons, mark UV discontinuities: " + timer.elapsedStringNPlaces(5)));
		DISPLACEMENT_RESET_TIMER(timer);


		// Count number of edges adjacent to each vertex.
		// If the number of edges adjacent to the vertex is > 2, then make this a fixed vertex.
		std::vector<unsigned int> num_edges_adjacent(temp_verts.size(), 0);

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
		}

	} // End init

	//conPrint("num fixed verts created: " + toString(temp_vert_polygons.size()));

	DISPLACEMENT_PRINT_RESULTS(conPrint("Adding vertex polygons: " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	DUScratchInfo scratch_info;
	scratch_info.edge_info_map.set_empty_key(DUVertIndexPair(std::numeric_limits<unsigned int>::max(), std::numeric_limits<unsigned int>::max()));

	if(PROFILE)
	{
		conPrint("---Subdiv options---");
		printVar(subdivision_smoothing);
		printVar(options.displacement_error_threshold);
		printVar(options.max_num_subdivisions);
		printVar(options.view_dependent_subdivision);
		printVar(options.pixel_height_at_dist_one);
		printVar(options.subdivide_curvature_threshold);
		printVar(options.subdivide_pixel_threshold);
	}

	Polygons* current_polygons = &temp_polygons_1;
	Polygons* next_polygons    = &temp_polygons_2;

	VertsAndUVs* current_verts_and_uvs = &temp_verts_uvs_1;
	VertsAndUVs* next_verts_and_uvs    = &temp_verts_uvs_2;

	// Pre and post-condition: The most recent information is in *current_polygons, *current_verts_and_uvs.
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
			options,
			scratch_info,
			*next_polygons, // polygons_out
			*next_verts_and_uvs // verts_and_uvs_out
		);

		// Updated information is now in *next_polygons, *next_verts_and_uvs

		DISPLACEMENT_PRINT_RESULTS(conPrint("linearSubdivision took " + linear_timer.elapsedString()));

		if(subdivision_smoothing)
		{
			// Compute average pass, storing averaged resuls in *current_verts_and_uvs.
			DISPLACEMENT_CREATE_TIMER(avpass_timer);
			averagePass(task_manager, *next_polygons, *next_verts_and_uvs, num_uv_sets, options, 
				*current_verts_and_uvs // verts_and_uvs_out
				);
			DISPLACEMENT_PRINT_RESULTS(conPrint("averagePass took " + avpass_timer.elapsedString()));

			mySwap(next_verts_and_uvs, current_verts_and_uvs);

			// Averaged results are now in *next_verts_and_uvs
		}

		// Recompute vertex normals.  (Updates *next_verts_and_uvs)
		DISPLACEMENT_CREATE_TIMER(recompute_vertex_normals);
		computeVertexNormals(next_polygons->tris, next_polygons->quads, next_verts_and_uvs->verts);
		DISPLACEMENT_PRINT_RESULTS(conPrint("computeVertexNormals took " + recompute_vertex_normals.elapsedString()));

		print_output.print("\t\tresulting num vertices: " + toString((unsigned int)next_verts_and_uvs->verts.size()));
		print_output.print("\t\tresulting num triangles: " + toString((unsigned int)next_polygons->tris.size()));
		print_output.print("\t\tresulting num quads: " + toString((unsigned int)next_polygons->quads.size()));
		print_output.print("\t\tDone.");

		// Swap next_polygons, current_polygons etc..
		mySwap(next_polygons, current_polygons);
		mySwap(next_verts_and_uvs, current_verts_and_uvs);

		// The most recently updated information is now in current_polygons, current_verts_and_uvs
	}

	// Apply the final displacement
	DISPLACEMENT_RESET_TIMER(timer);
	displace(
		task_manager,
		context,
		materials,
		true, // use anchoring
		current_polygons->tris, // tris in
		current_polygons->quads, // quads in
		current_verts_and_uvs->verts, // verts in
		current_verts_and_uvs->uvs, // uvs in
		num_uv_sets,
		next_verts_and_uvs->verts // verts out
	);
	DISPLACEMENT_PRINT_RESULTS(conPrint("final displace took " + timer.elapsedString()));
	DISPLACEMENT_RESET_TIMER(timer);

	current_verts_and_uvs->verts = next_verts_and_uvs->verts; // TODO: optimise this away.

	const RayMesh_ShadingNormals use_s_n = use_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals;

	std::vector<DUTriangle>& temp_tris = current_polygons->tris;
	std::vector<DUQuad>& temp_quads = current_polygons->quads;

	std::vector<DUVertex>& temp_verts = current_verts_and_uvs->verts;

	// Build tris_out from temp_tris and temp_quads
	const size_t temp_tris_size = temp_tris.size();
	const size_t temp_quads_size = temp_quads.size();
	tris_out.resize(temp_tris_size + temp_quads_size * 2); // Pre-allocate space

	DISPLACEMENT_PRINT_RESULTS(conPrint("tris_out alloc took " + timer.elapsedString()));
	DISPLACEMENT_RESET_TIMER(timer);
	
	for(size_t i = 0; i < temp_tris_size; ++i)
	{
		tris_out[i] = RayMeshTriangle(temp_tris[i].vertex_indices[0], temp_tris[i].vertex_indices[1], temp_tris[i].vertex_indices[2], temp_tris[i].tri_mat_index, use_s_n);

		for(size_t c = 0; c < 3; ++c)
			tris_out[i].uv_indices[c] = temp_tris[i].uv_indices[c];
	}

	size_t tri_write_i = temp_tris_size;
	for(size_t i = 0; i < temp_quads_size; ++i)
	{
		// Split the quad into two triangles
		RayMeshTriangle& tri_a = tris_out[tri_write_i];

		tri_a.vertex_indices[0] = temp_quads[i].vertex_indices[0];
		tri_a.vertex_indices[1] = temp_quads[i].vertex_indices[1];
		tri_a.vertex_indices[2] = temp_quads[i].vertex_indices[2];

		tri_a.uv_indices[0] = temp_quads[i].uv_indices[0];
		tri_a.uv_indices[1] = temp_quads[i].uv_indices[1];
		tri_a.uv_indices[2] = temp_quads[i].uv_indices[2];

		tri_a.setTriMatIndex(temp_quads[i].mat_index);
		tri_a.setUseShadingNormals(use_s_n);

		RayMeshTriangle& tri_b = tris_out[tri_write_i + 1];

		tri_b.vertex_indices[0] = temp_quads[i].vertex_indices[0];
		tri_b.vertex_indices[1] = temp_quads[i].vertex_indices[2];
		tri_b.vertex_indices[2] = temp_quads[i].vertex_indices[3];

		tri_b.uv_indices[0] = temp_quads[i].uv_indices[0];
		tri_b.uv_indices[1] = temp_quads[i].uv_indices[2];
		tri_b.uv_indices[2] = temp_quads[i].uv_indices[3];

		tri_b.setTriMatIndex(temp_quads[i].mat_index);
		tri_b.setUseShadingNormals(use_s_n);

		tri_write_i += 2;
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("Writing to tris_out took " + timer.elapsedString()));


	// Recompute all vertex normals, as they will be completely wrong by now due to any displacement.
	DISPLACEMENT_RESET_TIMER(timer);
	computeVertexNormals(temp_tris, temp_quads, temp_verts);
	DISPLACEMENT_PRINT_RESULTS(conPrint("final computeVertexNormals() took " + timer.elapsedString()));


	// Convert DUVertex's back into RayMeshVertex and store in verts_out.
	DISPLACEMENT_RESET_TIMER(timer);
	const size_t temp_verts_size = temp_verts.size();
	verts_out.resize(temp_verts_size);
	for(size_t i = 0; i < temp_verts_size; ++i)
		verts_out[i] = RayMeshVertex(temp_verts[i].pos, temp_verts[i].normal,
			0 // H - mean curvature - just set to zero, we will recompute it later.
		);

	uvs_out = current_verts_and_uvs->uvs;

	DISPLACEMENT_PRINT_RESULTS(conPrint("creating verts_out and uvs_out: " + timer.elapsedString()));

	print_output.print("Subdivision and displacement took " + total_timer.elapsedStringNPlaces(5));
	DISPLACEMENT_PRINT_RESULTS(conPrint("Total time elapsed: " + total_timer.elapsedString()));
}


/*
Apply displacement to the given vertices, storing the displaced vertices in verts_out
*/
struct DisplaceTaskClosure
{
	unsigned int num_uv_sets;
	const std::vector<Vec2f>* vert_uvs;
	const std::vector<DUVertex>* verts_in;
	std::vector<DUVertex>* verts_out;
	const std::vector<const Material*>* vert_materials;
};


class DisplaceTask : public Indigo::Task
{
public:
	DisplaceTask(const DisplaceTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		ThreadContext context;
		DUUVCoordEvaluator du_texcoord_evaluator;
		du_texcoord_evaluator.texcoords.resize(closure.num_uv_sets);
		const int num_uv_sets = closure.num_uv_sets;
		const std::vector<Vec2f>& vert_uvs = *closure.vert_uvs;
		const std::vector<DUVertex>& verts_in = *closure.verts_in;
		std::vector<DUVertex>& verts_out = *closure.verts_out;

		for(int v_i = begin; v_i < end; ++v_i)
		{
			const Material* const vert_material = (*closure.vert_materials)[v_i];

			if(vert_material != NULL && vert_material->displacing())
			{
				HitInfo hitinfo(std::numeric_limits<unsigned int>::max(), HitInfo::SubElemCoordsType(-666, -666));

				for(int z = 0; z < num_uv_sets; ++z)
					du_texcoord_evaluator.texcoords[z] = vert_uvs[v_i*num_uv_sets + z];

				du_texcoord_evaluator.pos_os = verts_in[v_i].pos.toVec4fPoint();

				EvalDisplaceArgs args(
					context,
					hitinfo,
					du_texcoord_evaluator,
					Vec4f(0), // dp_dalpha  TEMP HACK
					Vec4f(0), // dp_dbeta  TEMP HACK
					Vec4f(0,0,1,0) // pre-bump N_s_ws TEMP HACK
				);

				const float displacement = vert_material->evaluateDisplacement(args);
					
				//TEMPassert((*closure.verts_in)[v_i].normal.isUnitLength());

				verts_out[v_i].displacement = displacement;
				verts_out[v_i].pos = verts_in[v_i].pos + verts_in[v_i].normal * displacement;
			}
			else
			{
				// No tri or quad references this vert, or the applied material is not displacing.
				verts_out[v_i].displacement = 0;
				verts_out[v_i].pos = verts_in[v_i].pos;
			}
		}
	}

	const DisplaceTaskClosure& closure;
	int begin, end;
};


void DisplacementUtils::displace(Indigo::TaskManager& task_manager,
								 ThreadContext& context,
								 const std::vector<Reference<Material> >& materials,
								 bool use_anchoring,
								 const std::vector<DUTriangle>& triangles,
								 const std::vector<DUQuad>& quads,
								 const std::vector<DUVertex>& verts_in,
								 const std::vector<Vec2f>& uvs,
								 unsigned int num_uv_sets,
								 std::vector<DUVertex>& verts_out
								 )
{
	verts_out = verts_in;

	// Work out which material applies to each vertex.
	// Also get UVs for each vertex.
	// NOTE: this ignores the case when multiple materials are assigned to a single vertex.  The 'last' material assigned is used.

	std::vector<const Material*> vert_materials(verts_in.size(), NULL);
	std::vector<Vec2f> vert_uvs(verts_in.size() * num_uv_sets);

	for(size_t t = 0; t < triangles.size(); ++t)
	{
		const Material* material = materials[triangles[t].tri_mat_index].getPointer(); // Get the material assigned to this triangle

		for(size_t i = 0; i < 3; ++i)
		{
			vert_materials[triangles[t].vertex_indices[i]] = material;

			for(unsigned int z=0; z<num_uv_sets; ++z)
				vert_uvs[triangles[t].vertex_indices[i] * num_uv_sets + z] = getUVs(uvs, num_uv_sets, triangles[t].uv_indices[i], z);
		}
	}

	for(size_t q = 0; q < quads.size(); ++q)
	{
		const Material* material = materials[quads[q].mat_index].getPointer(); // Get the material assigned to this triangle

		for(size_t i = 0; i < 4; ++i)
		{
			vert_materials[quads[q].vertex_indices[i]] = material;

			for(unsigned int z=0; z<num_uv_sets; ++z)
				vert_uvs[quads[q].vertex_indices[i] * num_uv_sets + z] = getUVs(uvs, num_uv_sets, quads[q].uv_indices[i], z);
		}
	}

	DisplaceTaskClosure closure;
	closure.num_uv_sets = num_uv_sets;
	closure.vert_uvs = &vert_uvs;
	closure.verts_in = &verts_in;
	closure.verts_out = &verts_out;
	closure.vert_materials = &vert_materials;
	task_manager.runParallelForTasks<DisplaceTask, DisplaceTaskClosure>(closure, 0, verts_in.size());

	// If any vertex is anchored, then set its position to the average of its 'parent' vertices
	for(size_t v = 0; v < verts_out.size(); ++v)
		if(verts_out[v].anchored)
			verts_out[v].pos = (verts_out[verts_out[v].adjacent_vert_0].pos + verts_out[verts_out[v].adjacent_vert_1].pos) * 0.5f;
}


static float triangleMaxCurvature(const Vec3f& v0_normal, const Vec3f& v1_normal, const Vec3f& v2_normal)
{
	const float curvature_u = ::angleBetweenNormalized(v0_normal, v1_normal);// / v0.pos.getDist(v1.pos);
	const float curvature_v = ::angleBetweenNormalized(v0_normal, v2_normal);// / v0.pos.getDist(v2.pos);
	const float curvature_uv = ::angleBetweenNormalized(v1_normal, v2_normal);// / v1.pos.getDist(v2.pos);

	return myMax(curvature_u, curvature_v, curvature_uv);
}


static float quadMaxCurvature(const Vec3f& v0_normal, const Vec3f& v1_normal, const Vec3f& v2_normal, const Vec3f& v3_normal)
{
	const float curvature_0 = ::angleBetweenNormalized(v0_normal, v1_normal);
	const float curvature_1 = ::angleBetweenNormalized(v1_normal, v2_normal);
	const float curvature_2 = ::angleBetweenNormalized(v2_normal, v3_normal);
	const float curvature_3 = ::angleBetweenNormalized(v3_normal, v0_normal);

	return myMax(curvature_0, curvature_1, myMax(curvature_2, curvature_3));
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


static int getDispErrorSamplingResForTris(size_t num_tris)
{
	return getDispErrorSamplingResForQuads(num_tris / 2);
}


struct BuildSubdividingPrimitiveTaskClosure
{
	const DUOptions* options;
	int num_subdivs_done;
	int min_num_subdivisions;
	unsigned int num_uv_sets;
	std::vector<int>* subdividing_tri;
	std::vector<int>* subdividing_quad;

	const std::vector<DUVertex>* displaced_in_verts;
	const std::vector<DUTriangle>* tris_in;
	const std::vector<DUQuad>* quads_in;
	const std::vector<Vec2f>* uvs_in;
	const std::vector<Reference<Material> >* materials;
};


class BuildSubdividingTriTask : public Indigo::Task
{
public:
	BuildSubdividingTriTask(const BuildSubdividingPrimitiveTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const std::vector<DUVertex>& displaced_in_verts = *closure.displaced_in_verts;
		const std::vector<DUTriangle>& tris_in = *closure.tris_in;

		// Create some temporary buffers
		std::vector<Vec3f> tri_verts_pos_os(3);

		std::vector<Vec3f> clipped_poly_verts_os;
		clipped_poly_verts_os.reserve(32);

		std::vector<Vec3f> clipped_poly_verts_cs;
		clipped_poly_verts_cs.reserve(32);

		std::vector<Vec3f> temp_vert_buffer;
		temp_vert_buffer.reserve(32);

		DUUVCoordEvaluator temp_du_texcoord_evaluator;
		temp_du_texcoord_evaluator.texcoords.resize(closure.num_uv_sets);

		ThreadContext context;

		std::vector<Vec2f> temp_uv_buffer;
		temp_uv_buffer.resize(closure.num_uv_sets * 3);

		for(int t = begin; t < end; ++t)
		{
			// Decide if we are going to subdivide the triangle
			bool subdivide_triangle = false;

			if(tris_in[t].dead)
			{
				subdivide_triangle = false;
			}
			else
			{
				if(closure.num_subdivs_done < closure.min_num_subdivisions)
				{
					subdivide_triangle = true;
				}
				else
				{
					if(closure.options->view_dependent_subdivision)
					{
						// Build vector of displaced triangle vertex positions. (in object space)
						for(uint32_t i = 0; i < 3; ++i)
							tri_verts_pos_os[i] = displaced_in_verts[tris_in[t].vertex_indices[i]].pos;

						// Fast path to see if all verts are completely inside all clipping planes
						bool completely_unclipped = true;
						for(size_t p=0; p<closure.options->camera_clip_planes_os.size(); ++p)
							for(uint32_t i = 0; i < 3; ++i)
								if(closure.options->camera_clip_planes_os[p].signedDistToPoint(tri_verts_pos_os[i]) > 0)
								{
									completely_unclipped = false;
									goto done_unclipped_check;
								}
done_unclipped_check:
						if(completely_unclipped)
							clipped_poly_verts_os = tri_verts_pos_os;
						else
						{
							// Clip triangle against frustrum planes
							TriBoxIntersection::clipPolyToPlaneHalfSpaces(closure.options->camera_clip_planes_os, tri_verts_pos_os, temp_vert_buffer, clipped_poly_verts_os);
						}

						if(clipped_poly_verts_os.size() > 0) // If the triangle has not been completely clipped away
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

							// Compute 2D bounding box of clipped triangle in screen space
							const Vec2f v0_ss = screenSpacePosForCameraSpacePos(clipped_poly_verts_cs[0]);
							Rect2f rect_ss(v0_ss, v0_ss);

							for(size_t i = 1; i < clipped_poly_verts_cs.size(); ++i)
								rect_ss.enlargeToHoldPoint(
									screenSpacePosForCameraSpacePos(clipped_poly_verts_cs[i])
									);

							const float max_rect_pixels = myMax(rect_ss.getWidths().x, rect_ss.getWidths().y);

							// Subdivide only if the width of height of the screen space triangle bounding rectangle is bigger than the pixel height threshold
							const bool exceeds_pixel_threshold = max_rect_pixels > closure.options->pixel_height_at_dist_one * closure.options->subdivide_pixel_threshold;

							if(exceeds_pixel_threshold)
							{
								const float tri_curvature = triangleMaxCurvature(
									displaced_in_verts[tris_in[t].vertex_indices[0]].normal, 
									displaced_in_verts[tris_in[t].vertex_indices[1]].normal, 
									displaced_in_verts[tris_in[t].vertex_indices[2]].normal);

								if(tri_curvature >= (float)closure.options->subdivide_curvature_threshold)
									subdivide_triangle = true;
								else
								{
									// Test displacement error
									
									// Eval UVs
									for(uint32_t z = 0; z < closure.num_uv_sets; ++z)
									{
										temp_uv_buffer[z*closure.num_uv_sets + 0] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[0], z);
										temp_uv_buffer[z*closure.num_uv_sets + 1] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[1], z);
										temp_uv_buffer[z*closure.num_uv_sets + 2] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[2], z);
									}

									EvalDispBoundsInfo eval_info;
									for(int i=0; i<3; ++i)
										eval_info.vert_pos_os[i] = (*closure.displaced_in_verts)[tris_in[t].vertex_indices[i]].pos.toVec4fPoint();
									eval_info.context = &context;
									eval_info.temp_uv_coord_evaluator = &temp_du_texcoord_evaluator;
									eval_info.uvs = &temp_uv_buffer[0];
									eval_info.num_verts = 3;
									eval_info.num_uv_sets = closure.num_uv_sets;
									for(int i=0; i<3; ++i)
										eval_info.vert_displacements[i] = (*closure.displaced_in_verts)[tris_in[t].vertex_indices[i]].displacement;
									eval_info.error_threshold = (float)closure.options->displacement_error_threshold;
								
									eval_info.suggested_sampling_res = getDispErrorSamplingResForTris(closure.tris_in->size());
								
									subdivide_triangle = (*closure.materials)[tris_in[t].tri_mat_index]->doesDisplacementErrorExceedThreshold(eval_info);
								}
							}
						}
					}
					else // Else if not view_dependent_subdivision:
					{
						if(closure.options->subdivide_curvature_threshold <= 0)
						{
							// If subdivide_curvature_threshold is <= 0, then since tri_curvature is always >= 0, then we will always subdivide the triangle,
							// so avoid computing the tri_curvature.
							subdivide_triangle = true;
						}
						else
						{
							const float tri_curvature = triangleMaxCurvature(
										displaced_in_verts[tris_in[t].vertex_indices[0]].normal, 
										displaced_in_verts[tris_in[t].vertex_indices[1]].normal, 
										displaced_in_verts[tris_in[t].vertex_indices[2]].normal);

							if(tri_curvature >= (float)closure.options->subdivide_curvature_threshold)
								subdivide_triangle = true;
							else
							{
								if(closure.options->displacement_error_threshold <= 0)
								{
									// If displacement_error_threshold is <= then since displacment_error is always >= 0, then we will always subdivide the triangle.
									// So avoid computing the displacment_error.
									subdivide_triangle = true;
								}
								else
								{
									// Test displacement error

									// Eval UVs
									for(uint32_t z = 0; z < closure.num_uv_sets; ++z)
									{
										temp_uv_buffer[z*closure.num_uv_sets + 0] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[0], z);
										temp_uv_buffer[z*closure.num_uv_sets + 1] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[1], z);
										temp_uv_buffer[z*closure.num_uv_sets + 2] = getUVs(*closure.uvs_in, closure.num_uv_sets, tris_in[t].uv_indices[2], z);
									}

									EvalDispBoundsInfo eval_info;
									for(int i=0; i<3; ++i)
										eval_info.vert_pos_os[i] = (*closure.displaced_in_verts)[tris_in[t].vertex_indices[i]].pos.toVec4fPoint();
									eval_info.context = &context;
									eval_info.temp_uv_coord_evaluator = &temp_du_texcoord_evaluator;
									eval_info.uvs = &temp_uv_buffer[0];
									eval_info.num_verts = 3;
									eval_info.num_uv_sets = closure.num_uv_sets;
									for(int i=0; i<3; ++i)
										eval_info.vert_displacements[i] = (*closure.displaced_in_verts)[tris_in[t].vertex_indices[i]].displacement;
									eval_info.error_threshold = (float)closure.options->displacement_error_threshold;

									eval_info.suggested_sampling_res = getDispErrorSamplingResForTris(closure.tris_in->size());
								
									subdivide_triangle = (*closure.materials)[tris_in[t].tri_mat_index]->doesDisplacementErrorExceedThreshold(eval_info);
								
								}
							}
						}
					}
				}
			}

			(*closure.subdividing_tri)[t] = subdivide_triangle;
		}
	}

	const BuildSubdividingPrimitiveTaskClosure& closure;
	int begin, end;
};


class BuildSubdividingQuadTask : public Indigo::Task
{
public:
	BuildSubdividingQuadTask(const BuildSubdividingPrimitiveTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const std::vector<DUVertex>& displaced_in_verts = *closure.displaced_in_verts;
		const std::vector<DUQuad>& quads_in = *closure.quads_in;

		// Create some temporary buffers
		std::vector<Vec3f> quad_verts_pos_os(4);

		std::vector<Vec3f> clipped_poly_verts_os;
		clipped_poly_verts_os.reserve(32);

		std::vector<Vec3f> clipped_poly_verts_cs;
		clipped_poly_verts_cs.reserve(32);

		std::vector<Vec3f> temp_vert_buffer;
		temp_vert_buffer.reserve(32);

		DUUVCoordEvaluator temp_du_texcoord_evaluator;
		temp_du_texcoord_evaluator.texcoords.resize(closure.num_uv_sets);

		ThreadContext context;

		std::vector<Vec2f> temp_uv_buffer;
		temp_uv_buffer.resize(closure.num_uv_sets * 4);

		for(int q = begin; q < end; ++q)
		{
			// Decide if we are going to subdivide the quad
			bool subdivide_quad = false;

			if(quads_in[q].dead)
			{
				subdivide_quad = false;
			}
			else
			{
				if(closure.num_subdivs_done < closure.min_num_subdivisions)
				{
					subdivide_quad = true;
				}
				else
				{
					if(closure.options->view_dependent_subdivision)
					{
						// Build vector of displaced quad vertex positions. (in object space)
						for(uint32_t i = 0; i < 4; ++i)
							quad_verts_pos_os[i] = displaced_in_verts[quads_in[q].vertex_indices[i]].pos;

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

							if(exceeds_pixel_threshold)
							{
								const float curvature = quadMaxCurvature(
									displaced_in_verts[quads_in[q].vertex_indices[0]].normal, 
									displaced_in_verts[quads_in[q].vertex_indices[1]].normal, 
									displaced_in_verts[quads_in[q].vertex_indices[2]].normal,
									displaced_in_verts[quads_in[q].vertex_indices[3]].normal);

								if(curvature >= (float)closure.options->subdivide_curvature_threshold)
									subdivide_quad = true;
								else
								{
									// Test displacement error

									// Eval UVs
									for(uint32_t z = 0; z < closure.num_uv_sets; ++z)
									{
										temp_uv_buffer[z*closure.num_uv_sets + 0] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[0], z);
										temp_uv_buffer[z*closure.num_uv_sets + 1] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[1], z);
										temp_uv_buffer[z*closure.num_uv_sets + 2] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[2], z);
										temp_uv_buffer[z*closure.num_uv_sets + 3] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[3], z);
									}

									EvalDispBoundsInfo eval_info;
									for(int i=0; i<4; ++i)
										eval_info.vert_pos_os[i] = (*closure.displaced_in_verts)[quads_in[q].vertex_indices[i]].pos.toVec4fPoint();
									eval_info.context = &context;
									eval_info.temp_uv_coord_evaluator = &temp_du_texcoord_evaluator;
									eval_info.uvs = &temp_uv_buffer[0];
									eval_info.num_verts = 4;
									eval_info.num_uv_sets = closure.num_uv_sets;
									for(int i=0; i<4; ++i)
										eval_info.vert_displacements[i] = (*closure.displaced_in_verts)[quads_in[q].vertex_indices[i]].displacement;
									eval_info.error_threshold = (float)closure.options->displacement_error_threshold;

									eval_info.suggested_sampling_res = getDispErrorSamplingResForQuads(closure.quads_in->size());
								
									subdivide_quad = (*closure.materials)[quads_in[q].mat_index]->doesDisplacementErrorExceedThreshold(eval_info);
								}
							}
						}
					}
					else
					{
						if(closure.options->subdivide_curvature_threshold <= 0)
						{
							// If subdivide_curvature_threshold is <= 0, then since quad_curvature is always >= 0, then we will always subdivide the quad,
							// so avoid computing the curvature.
							subdivide_quad = true;
						}
						else
						{
							const float curvature = quadMaxCurvature(
										displaced_in_verts[quads_in[q].vertex_indices[0]].normal, 
										displaced_in_verts[quads_in[q].vertex_indices[1]].normal, 
										displaced_in_verts[quads_in[q].vertex_indices[2]].normal,
										displaced_in_verts[quads_in[q].vertex_indices[3]].normal);

							if(curvature >= (float)closure.options->subdivide_curvature_threshold)
								subdivide_quad = true;
							else
							{
								if(closure.options->displacement_error_threshold <= 0)
								{
									// If displacement_error_threshold is <= then since displacment_error is always >= 0, then we will always subdivide the quad.
									// So avoid computing the displacment_error.
									subdivide_quad = true;
								}
								else
								{
									// Test displacement error
									
									// Eval UVs
									for(uint32_t z = 0; z < closure.num_uv_sets; ++z)
									{
										temp_uv_buffer[z*closure.num_uv_sets + 0] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[0], z);
										temp_uv_buffer[z*closure.num_uv_sets + 1] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[1], z);
										temp_uv_buffer[z*closure.num_uv_sets + 2] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[2], z);
										temp_uv_buffer[z*closure.num_uv_sets + 3] = getUVs(*closure.uvs_in, closure.num_uv_sets, quads_in[q].uv_indices[3], z);
									}

									EvalDispBoundsInfo eval_info;
									for(int i=0; i<4; ++i)
										eval_info.vert_pos_os[i] = (*closure.displaced_in_verts)[quads_in[q].vertex_indices[i]].pos.toVec4fPoint();
									eval_info.context = &context;
									eval_info.temp_uv_coord_evaluator = &temp_du_texcoord_evaluator;
									eval_info.uvs = &temp_uv_buffer[0];
									eval_info.num_verts = 4;
									eval_info.num_uv_sets = closure.num_uv_sets;
									for(int i=0; i<4; ++i)
										eval_info.vert_displacements[i] = (*closure.displaced_in_verts)[quads_in[q].vertex_indices[i]].displacement;
									eval_info.error_threshold = (float)closure.options->displacement_error_threshold;

									eval_info.suggested_sampling_res = getDispErrorSamplingResForQuads(closure.quads_in->size());
								
									subdivide_quad = (*closure.materials)[quads_in[q].mat_index]->doesDisplacementErrorExceedThreshold(eval_info);
								}
							}
						}
					}
				}
			}

			(*closure.subdividing_quad)[q] = subdivide_quad;
		}
	}

	const BuildSubdividingPrimitiveTaskClosure& closure;
	int begin, end;
};


struct SubdividePrimitiveTaskClosure
{
	const std::vector<int>* subdividing_tri;
	const std::vector<int>* subdividing_quad;
	const std::vector<int>* tri_write_index;
	const std::vector<int>* quad_write_index;

	unsigned int num_uv_sets;
	const std::vector<DUTriangle>* tris_in;
	std::vector<DUTriangle>* tris_out;
	const std::vector<DUQuad>* quads_in;
	std::vector<DUQuad>* quads_out;
	const std::vector<std::pair<uint32_t, uint32_t> >* quad_centre_data;
	const EdgeInfoMapType* edge_info_map;
};


class SubdivideTriTask : public Indigo::Task
{
public:
	SubdivideTriTask(const SubdividePrimitiveTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const std::vector<int>& subdividing_tri = *closure.subdividing_tri;
		const std::vector<int>& tri_write_index = *closure.tri_write_index;
		const int num_uv_sets = closure.num_uv_sets;
		const std::vector<DUTriangle>& tris_in = *closure.tris_in;
		std::vector<DUTriangle>& tris_out = *closure.tris_out;
		const EdgeInfoMapType& edge_info_map = *closure.edge_info_map;

		for(size_t t = begin; t < end; ++t)
		{
			const int64 write_index = tri_write_index[t];

			if(subdividing_tri[t]) // If we are subdividing this triangle...
			{
				uint32_t midpoint_vert_indices[3]; // Indices of edge midpoint vertices in verts_out
				uint32_t midpoint_uv_indices[3] = { 0, 0, 0 };

				for(uint32_t v = 0; v < 3; ++v) // For each edge (v_i, v_i+1)
				{
					const uint32_t v0 = tris_in[t].vertex_indices[v];
					const uint32_t v1 = tris_in[t].vertex_indices[mod3(v + 1)];
					const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

					const EdgeInfoMapType::const_iterator result = edge_info_map.find(edge);
					assert(result != edge_info_map.end());

					midpoint_vert_indices[v] = (*result).second.midpoint_vert_index;

					if(num_uv_sets > 0)
					{
						// Work out if we're using the 'a' or 'b' tri uvs
						const uint32_t uv0 = tris_in[t].uv_indices[v];
						const uint32_t uv1 = tris_in[t].uv_indices[mod3(v + 1)];
						const DUVertIndexPair edge_uv_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

						if((*result).second.prim_a_uv_i_start == edge_uv_key.v_a && (*result).second.prim_a_uv_i_end == edge_uv_key.v_b)
							midpoint_uv_indices[v] = (*result).second.prim_a_midpoint_uv_index;
						else
						{
							//assert((*result).second.left_uv_i_start == edge_key.v_a && (*result).second.left_uv_i_end == edge_key.v_b);
							midpoint_uv_indices[v] = (*result).second.prim_b_midpoint_uv_index;
						}
					}
				}

				// left triangle
				tris_out[write_index + 0] = DUTriangle(
						tris_in[t].vertex_indices[0],
						midpoint_vert_indices[0],
						midpoint_vert_indices[2],
						tris_in[t].uv_indices[0],
						midpoint_uv_indices[0],
						midpoint_uv_indices[2],
						tris_in[t].tri_mat_index
				);
				tris_out[write_index + 0].edge_uv_discontinuity[0] = tris_in[t].edge_uv_discontinuity[0];
				tris_out[write_index + 0].edge_uv_discontinuity[2] = tris_in[t].edge_uv_discontinuity[2];

				// Centre triangle
				tris_out[write_index + 1] = DUTriangle(
						midpoint_vert_indices[1],
						midpoint_vert_indices[2],
						midpoint_vert_indices[0],
						midpoint_uv_indices[1],
						midpoint_uv_indices[2],
						midpoint_uv_indices[0],
						tris_in[t].tri_mat_index
				);

				// right triangle
				tris_out[write_index + 2] = DUTriangle(
						midpoint_vert_indices[0],
						tris_in[t].vertex_indices[1],
						midpoint_vert_indices[1],
						midpoint_uv_indices[0],
						tris_in[t].uv_indices[1],
						midpoint_uv_indices[1],
						tris_in[t].tri_mat_index
				);
				tris_out[write_index + 2].edge_uv_discontinuity[0] = tris_in[t].edge_uv_discontinuity[0];
				tris_out[write_index + 2].edge_uv_discontinuity[1] = tris_in[t].edge_uv_discontinuity[1];

				// top triangle
				tris_out[write_index + 3] = DUTriangle(
						midpoint_vert_indices[2],
						midpoint_vert_indices[1],
						tris_in[t].vertex_indices[2],
						midpoint_uv_indices[2],
						midpoint_uv_indices[1],
						tris_in[t].uv_indices[2],
						tris_in[t].tri_mat_index
				);
				tris_out[write_index + 3].edge_uv_discontinuity[1] = tris_in[t].edge_uv_discontinuity[1];
				tris_out[write_index + 3].edge_uv_discontinuity[2] = tris_in[t].edge_uv_discontinuity[2];
			}
			else
			{
				// Else don't subdivide triangle.
				// Original vertices are already copied over, so just copy the triangle
				tris_out[write_index] = tris_in[t];
				tris_out[write_index].dead = true;
			}
		}
	}

	const SubdividePrimitiveTaskClosure& closure;
	int begin, end;
};

	
class SubdivideQuadTask : public Indigo::Task
{
public:
	SubdivideQuadTask(const SubdividePrimitiveTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const std::vector<int>& subdividing_quad = *closure.subdividing_quad;
		const std::vector<int>& quad_write_index = *closure.quad_write_index;
		const int num_uv_sets = closure.num_uv_sets;
		const std::vector<DUQuad>& quads_in = *closure.quads_in;
		std::vector<DUQuad>& quads_out = *closure.quads_out;
		const std::vector<std::pair<uint32_t, uint32_t> >& quad_centre_data = *closure.quad_centre_data;
		const EdgeInfoMapType& edge_info_map = *closure.edge_info_map;

		for(size_t q = begin; q < end; ++q)
		{
			const int64 write_index = quad_write_index[q];

			if(subdividing_quad[q]) // If we are subdividing this quad...
			{
				uint32_t midpoint_vert_indices[4]; // Indices of edge midpoint vertices in verts_out
				uint32_t midpoint_uv_indices[4] = { 0, 0, 0, 0 };
				const uint32_t centre_vert_idx = quad_centre_data[q].first;
				const uint32_t centre_uv_idx   = quad_centre_data[q].second;

				for(uint32_t v = 0; v < 4; ++v) // For each edge
				{
					const uint32_t v0 = quads_in[q].vertex_indices[v];
					const uint32_t v1 = quads_in[q].vertex_indices[mod4(v + 1)];
					const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

					const EdgeInfoMapType::const_iterator result = edge_info_map.find(edge);
					assert(result != edge_info_map.end());

					midpoint_vert_indices[v] = (*result).second.midpoint_vert_index;

					if(num_uv_sets > 0)
					{
						// Work out if we're using the 'a' or 'b' quad uvs
						const uint32_t uv0 = quads_in[q].uv_indices[v];
						const uint32_t uv1 = quads_in[q].uv_indices[mod4(v + 1)];
						const DUVertIndexPair edge_uv_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

						if((*result).second.prim_a_uv_i_start == edge_uv_key.v_a && (*result).second.prim_a_uv_i_end == edge_uv_key.v_b)
							midpoint_uv_indices[v] = (*result).second.prim_a_midpoint_uv_index;
						else
						{
							//assert((*result).second.left_uv_i_start == edge_key.v_a && (*result).second.left_uv_i_end == edge_key.v_b);
							midpoint_uv_indices[v] = (*result).second.prim_b_midpoint_uv_index;
						}
					}
				}

				quads_out[write_index + 0] = DUQuad(
					quads_in[q].vertex_indices[0], midpoint_vert_indices[0], centre_vert_idx, midpoint_vert_indices[3],
					quads_in[q].uv_indices[0],     midpoint_uv_indices[0],   centre_uv_idx,   midpoint_uv_indices[3],
					quads_in[q].mat_index
				);
				quads_out[write_index + 0].edge_uv_discontinuity[0] = quads_in[q].edge_uv_discontinuity[0];
				quads_out[write_index + 0].edge_uv_discontinuity[3] = quads_in[q].edge_uv_discontinuity[3];


				quads_out[write_index + 1] = DUQuad(
					midpoint_vert_indices[0], quads_in[q].vertex_indices[1], midpoint_vert_indices[1], centre_vert_idx,
					midpoint_uv_indices[0],   quads_in[q].uv_indices[1],     midpoint_uv_indices[1],   centre_uv_idx,
					quads_in[q].mat_index
				);
				quads_out[write_index + 1].edge_uv_discontinuity[0] = quads_in[q].edge_uv_discontinuity[0];
				quads_out[write_index + 1].edge_uv_discontinuity[1] = quads_in[q].edge_uv_discontinuity[1];


				quads_out[write_index + 2] = DUQuad(
					centre_vert_idx, midpoint_vert_indices[1], quads_in[q].vertex_indices[2], midpoint_vert_indices[2],
					centre_uv_idx,   midpoint_uv_indices[1],   quads_in[q].uv_indices[2],     midpoint_uv_indices[2],
					quads_in[q].mat_index
				);
				quads_out[write_index + 2].edge_uv_discontinuity[1] = quads_in[q].edge_uv_discontinuity[1];
				quads_out[write_index + 2].edge_uv_discontinuity[2] = quads_in[q].edge_uv_discontinuity[2];


				quads_out[write_index + 3] = DUQuad(
					midpoint_vert_indices[3], centre_vert_idx, midpoint_vert_indices[2], quads_in[q].vertex_indices[3],
					midpoint_uv_indices[3],   centre_uv_idx,   midpoint_uv_indices[2],   quads_in[q].uv_indices[3],
					quads_in[q].mat_index
				);
				quads_out[write_index + 3].edge_uv_discontinuity[2] = quads_in[q].edge_uv_discontinuity[2];
				quads_out[write_index + 3].edge_uv_discontinuity[3] = quads_in[q].edge_uv_discontinuity[3];
			}
			else
			{
				// Else don't subdivide quad.
				// Original vertices are already copied over, so just copy the quad.
				quads_out[write_index] = quads_in[q];
				quads_out[write_index].dead = true;
			}
		}
	}

	const SubdividePrimitiveTaskClosure& closure;
	int begin, end;
};



void DisplacementUtils::linearSubdivision(
	Indigo::TaskManager& task_manager,
	PrintOutput& print_output,
	ThreadContext& context,
	const std::vector<Reference<Material> >& materials,
	const Polygons& polygons_in,
	const VertsAndUVs& verts_and_uvs_in,
	unsigned int num_uv_sets,
	unsigned int num_subdivs_done,
	const DUOptions& options,
	DUScratchInfo& scratch_info,
	Polygons& polygons_out,
	VertsAndUVs& verts_and_uvs_out
	)
{
	DISPLACEMENT_CREATE_TIMER(timer);

	const std::vector<DUVertexPolygon>& vert_polygons_in = polygons_in.vert_polygons;
	const std::vector<DUEdge>& edges_in = polygons_in.edges;
	const std::vector<DUTriangle>& tris_in = polygons_in.tris;
	const std::vector<DUQuad>& quads_in = polygons_in.quads;
	const std::vector<DUVertex>& verts_in = verts_and_uvs_in.verts;
	const std::vector<Vec2f>& uvs_in = verts_and_uvs_in.uvs;

	std::vector<DUVertexPolygon>& vert_polygons_out = polygons_out.vert_polygons;
	std::vector<DUEdge>& edges_out = polygons_out.edges;
	std::vector<DUTriangle>& tris_out = polygons_out.tris;
	std::vector<DUQuad>& quads_out = polygons_out.quads;
	std::vector<DUVertex>& verts_out = verts_and_uvs_out.verts;
	std::vector<Vec2f>& uvs_out = verts_and_uvs_out.uvs;


	verts_out = verts_in; // Copy over original vertices
	uvs_out = uvs_in; // Copy over uvs
	
	edges_out.resize(0);


	// Get displaced vertices, which are needed for testing if subdivision is needed, in some cases.
	std::vector<DUVertex> displaced_in_verts;
	if(options.view_dependent_subdivision || options.subdivide_curvature_threshold > 0) // Only generate if needed.
	{
		displace(
			task_manager,
			context,
			materials,
			true, // use anchoring
			tris_in,
			quads_in,
			verts_in,
			uvs_in,
			num_uv_sets,
			displaced_in_verts // verts out
		);

		// Calculate normals of the displaced vertices
		computeVertexNormals(tris_in, quads_in, displaced_in_verts);
	}

	EdgeInfoMapType& edge_info_map = scratch_info.edge_info_map;
	edge_info_map.clear_no_resize();

	// Do a pass to decide whether or not to subdivide each triangle, and create new vertices if subdividing.

	// If we are doing view-dependent subdivisions, then do a few levels of subdivision, without the fine detail.
	// Otherwise we get situations like view-dependent subdivision done to create mountains, where outside of the camera frustum the mountains
	// fall away immediately to zero height.  This is bad for several reasons - including shading and lighting artifacts.
	const unsigned int min_num_subdivisions = options.view_dependent_subdivision ? options.max_num_subdivisions / 2 : 0;

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

	//========================== Work out if we are subdividing each triangle ==========================
	DISPLACEMENT_RESET_TIMER(timer);
	const size_t tris_in_size = tris_in.size();
	const size_t quads_in_size = quads_in.size();

	std::vector<int> subdividing_tri(tris_in_size); // NOTE: can't use bool here, as vector<bool> does bitpacking, so each word is not independent across each thread.
	std::vector<int> subdividing_quad(quads_in_size);
	
	BuildSubdividingPrimitiveTaskClosure sub_prim_closure;
	sub_prim_closure.options = &options;
	sub_prim_closure.num_subdivs_done = num_subdivs_done;
	sub_prim_closure.min_num_subdivisions = min_num_subdivisions;
	sub_prim_closure.num_uv_sets = num_uv_sets;
	sub_prim_closure.subdividing_tri = &subdividing_tri;
	sub_prim_closure.subdividing_quad = &subdividing_quad;
	sub_prim_closure.displaced_in_verts = &displaced_in_verts;
	sub_prim_closure.tris_in = &tris_in;
	sub_prim_closure.quads_in = &quads_in;
	sub_prim_closure.uvs_in = &uvs_in;
	sub_prim_closure.materials = &materials;

	task_manager.runParallelForTasks<BuildSubdividingTriTask, BuildSubdividingPrimitiveTaskClosure>(sub_prim_closure, 0, tris_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building subdividing_tri[]: " + timer.elapsedStringNPlaces(5)));

	
	//========================== Build tri_write_index array ==========================
	// This gives the index that the subdivided triangles generated from triangle i will be written to.
	// Note that we maintain the original triangle order here, e.g. new triangles will have a 
	// similar relative position in the triangle list to the old triangles.
	// This is important for coherency reasons.  (gave a noticable slowdown with an interleaved order)
	// Generating this array is a serial operation, but it's pretty fast.
	DISPLACEMENT_RESET_TIMER(timer);
	std::vector<int> tri_write_index(tris_in.size());

	int current_tri_write_index = 0;
	for(size_t i=0; i<tris_in_size; ++i)
	{
		tri_write_index[i] = current_tri_write_index;
		current_tri_write_index += subdividing_tri[i] ? 4 : 1;
	}
	const int total_new_tris = current_tri_write_index;

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building tri_write_index[]: " + timer.elapsedStringNPlaces(5)));


	//========================== Create edge midpoint vertices for triangles ==========================
	
	if(PROFILE) conPrint("   Creating edge midpoint verts, tris_in.size(): " + toString(tris_in.size()));

	// For each triangle
	
	for(size_t t = 0; t < tris_in_size; ++t)
	{
		if(subdividing_tri[t])
		{
			for(uint32 v = 0; v < 3; ++v)
			{
				const unsigned int v0 = tris_in[t].vertex_indices[v];
				const unsigned int v1 = tris_in[t].vertex_indices[mod3(v + 1)];

				const unsigned int uv0 = tris_in[t].uv_indices[v];
				const unsigned int uv1 = tris_in[t].uv_indices[mod3(v + 1)];

				// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

				const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge
				const DUVertIndexPair edge_uv_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

				DUEdgeInfo& edge_info = edge_info_map[edge]; // Look up edge in map

				if(edge_info.num_adjacent_subdividing_polys == 0)
				{
					// Create the edge midpoint vertex
					const unsigned int new_vert_index = (unsigned int)verts_out.size();

					verts_out.push_back(DUVertex(
						(verts_in[v0].pos + verts_in[v1].pos) * 0.5f,
						(verts_in[v0].normal + verts_in[v1].normal)// * 0.5f
						));

					verts_out.back().adjacent_vert_0 = edge.v_a;
					verts_out.back().adjacent_vert_1 = edge.v_b;

					edge_info.midpoint_vert_index = new_vert_index;

					// Create the edge midpoint UV
					const int uv_index = (int)uvs_out.size() / num_uv_sets;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
					{
						const Vec2f uv_a = getUVs(uvs_in, num_uv_sets, uv0, z);
						const Vec2f uv_b = getUVs(uvs_in, num_uv_sets, uv1, z);
						uvs_out.push_back((uv_a + uv_b) * 0.5f);
					}

					edge_info.prim_a_uv_i_start = edge_uv_key.v_a;
					edge_info.prim_a_uv_i_end   = edge_uv_key.v_b;

					edge_info.prim_a_midpoint_uv_index = uv_index;
				}
				else
				{
					if(edge_info.prim_a_uv_i_start != edge_uv_key.v_a || edge_info.prim_a_uv_i_end != edge_uv_key.v_b)
					{
						// UVs differ, so create some new UVs and set as the 'b' quad UVs.
						const int uv_index = (int)uvs_out.size() / num_uv_sets;
						for(uint32_t z = 0; z < num_uv_sets; ++z)
						{
							const Vec2f uv_a = getUVs(uvs_in, num_uv_sets, uv0, z);
							const Vec2f uv_b = getUVs(uvs_in, num_uv_sets, uv1, z);
							uvs_out.push_back((uv_a + uv_b) * 0.5f);
						}

						edge_info.prim_b_midpoint_uv_index = uv_index;
					}
				}

				edge_info.num_adjacent_subdividing_polys++;
			}
		}
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building tri edge mid point vertices: " + timer.elapsedStringNPlaces(5)));
	
	//========================== Work out if we are subdividing each quad ==========================
	DISPLACEMENT_RESET_TIMER(timer);

	task_manager.runParallelForTasks<BuildSubdividingQuadTask, BuildSubdividingPrimitiveTaskClosure>(sub_prim_closure, 0, quads_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building subdividing_quad[]: " + timer.elapsedStringNPlaces(5)));


	//========================== Build quad_write_index array ==========================
	DISPLACEMENT_RESET_TIMER(timer);
	std::vector<int> quad_write_index(quads_in.size());

	int current_quad_write_index = 0;
	for(size_t i=0; i<quads_in_size; ++i)
	{
		quad_write_index[i] = current_quad_write_index;
		current_quad_write_index += subdividing_quad[i] ? 4 : 1;
	}
	const int total_new_quads = current_quad_write_index;

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building quad_write_index[]: " + timer.elapsedStringNPlaces(5)));


	//========================== Create edge midpoint and centroid vertices for quads ==========================
	DISPLACEMENT_RESET_TIMER(timer);
	std::vector<std::pair<uint32_t, uint32_t> > quad_centre_data(quads_in.size()); // (vertex index, uv set index)

	// For each quad
	
	for(size_t q = 0; q < quads_in_size; ++q)
	{
		if(subdividing_quad[q])
		{
			const uint32_t v[4] = { quads_in[q].vertex_indices[0], quads_in[q].vertex_indices[1],
									quads_in[q].vertex_indices[2], quads_in[q].vertex_indices[3] };

			// Create the quad's centroid vertex
			const uint32_t centroid_vert_index = (uint32_t)verts_out.size();
			const uint32_t centroid_uv_index = num_uv_sets > 0 ? (uint32_t)uvs_out.size() / num_uv_sets : 0;

			quad_centre_data[q].first  = centroid_vert_index;
			quad_centre_data[q].second = centroid_uv_index;

			verts_out.push_back(DUVertex(
				(verts_in[v[0]].pos + verts_in[v[1]].pos + verts_in[v[2]].pos + verts_in[v[3]].pos) * 0.25f,
				(verts_in[v[0]].normal + verts_in[v[1]].normal + verts_in[v[2]].normal + verts_in[v[3]].normal)// * 0.25f
				));

			for(uint32_t z = 0; z < num_uv_sets; ++z)
				uvs_out.push_back((
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[0], z) +
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[1], z) +
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[2], z) + 
					getUVs(uvs_in, num_uv_sets, quads_in[q].uv_indices[3], z)) * 0.25f);


			// Create the quad's edge vertices
			for(uint32_t vi = 0; vi < 4; ++vi)
			{
				const unsigned int v0 = quads_in[q].vertex_indices[vi];
				const unsigned int v1 = quads_in[q].vertex_indices[mod4(vi + 1)];
				const unsigned int uv0 = quads_in[q].uv_indices[vi];
				const unsigned int uv1 = quads_in[q].uv_indices[mod4(vi + 1)];

				// Get vertex at the midpoint of this edge, or create it if it doesn't exist yet.

				const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge
				const DUVertIndexPair edge_uv_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge

				DUEdgeInfo& edge_info = edge_info_map[edge]; // NOTE: SLOW!!

				if(edge_info.num_adjacent_subdividing_polys == 0)
				{
					// Create the edge midpoint vertex
					const uint32_t new_vert_index = (uint32)verts_out.size();

					verts_out.push_back(DUVertex(
						(verts_in[v0].pos + verts_in[v1].pos) * 0.5f,
						(verts_in[v0].normal + verts_in[v1].normal)// * 0.5f
						));

					verts_out.back().adjacent_vert_0 = edge.v_a;
					verts_out.back().adjacent_vert_1 = edge.v_b;

					edge_info.midpoint_vert_index = new_vert_index;

					// Create the edge midpoint UV
					const int uv_index = (int)uvs_out.size() / num_uv_sets;
					for(uint32_t z = 0; z < num_uv_sets; ++z)
					{
						const Vec2f uv_a = getUVs(uvs_in, num_uv_sets, uv0, z);
						const Vec2f uv_b = getUVs(uvs_in, num_uv_sets, uv1, z);
						uvs_out.push_back((uv_a + uv_b) * 0.5f);
					}

					edge_info.prim_a_uv_i_start = edge_uv_key.v_a;
					edge_info.prim_a_uv_i_end   = edge_uv_key.v_b;

					edge_info.prim_a_midpoint_uv_index = uv_index;
				}
				else
				{
					if(edge_info.prim_a_uv_i_start != edge_uv_key.v_a || edge_info.prim_a_uv_i_end != edge_uv_key.v_b)
					{
						// UVs differ, so create some new UVs and set as the 'b' quad UVs.
						const int uv_index = (int)uvs_out.size() / num_uv_sets;
						for(uint32_t z = 0; z < num_uv_sets; ++z)
						{
							const Vec2f uv_a = getUVs(uvs_in, num_uv_sets, uv0, z);
							const Vec2f uv_b = getUVs(uvs_in, num_uv_sets, uv1, z);
							uvs_out.push_back((uv_a + uv_b) * 0.5f);
						}

						edge_info.prim_b_midpoint_uv_index = uv_index;
					}
				}

				edge_info.num_adjacent_subdividing_polys++;
			}
		}
	}

	DISPLACEMENT_PRINT_RESULTS(conPrint("   building quad edge mid point vertices: " + timer.elapsedStringNPlaces(5)));
	DISPLACEMENT_RESET_TIMER(timer);

	// Mark border edges
	for(size_t i = 0; i < edges_in.size(); ++i)
	{
		const uint32 v0 = edges_in[i].vertex_indices[0];
		const uint32 v1 = edges_in[i].vertex_indices[1];
		edge_info_map[DUVertIndexPair(myMin(v0, v1), myMax(v0, v1))].border = true;
	}


	// Mark any new edge midpoint vertices as anchored that need to be
	for(EdgeInfoMapType::const_iterator i = edge_info_map.begin(); i != edge_info_map.end(); ++i)
		if(!(*i).second.border && (*i).second.num_adjacent_subdividing_polys == 1)
		{
			// This edge has a subdivided triangle on one side, and a non-subdivided triangle on the other side.
			// So we will 'anchor' the midpoint vertex to the average position of it's neighbours to avoid splitting.
			verts_out[(*i).second.midpoint_vert_index].anchored = true;
		}


	//========================== Subdivide polygons ==========================

	// Counters
	uint32 num_tris_subdivided = 0;
	uint32 num_tris_unchanged = 0;
	uint32 num_quads_subdivided = 0;
	uint32 num_quads_unchanged = 0;

	DISPLACEMENT_RESET_TIMER(timer);

	// Vertex polygons can't be subdivided, so just copy over
	vert_polygons_out = vert_polygons_in;

	//========================== Subdivide edges ==========================
	for(size_t i = 0; i < edges_in.size(); ++i)
	{
		const DUEdge& edge_in = edges_in[i];

		// We need to determine if this edge is being subdivided.
		// It will be subdivided if any adjacent polygons are being subdivided.

		const uint32_t v0 = edge_in.vertex_indices[0];
		const uint32_t v1 = edge_in.vertex_indices[1];
		const DUVertIndexPair edge(myMin(v0, v1), myMax(v0, v1)); // Key for the edge

		assert(edge_info_map.find(edge) != edge_info_map.end());

		const EdgeInfoMapType::iterator result = edge_info_map.find(edge);
		const DUEdgeInfo& edge_info = (*result).second;
		if(edge_info.num_adjacent_subdividing_polys > 0)
		{
			// A triangle adjacent to this edge is being subdivided.  So subdivide this edge 1-D polygon as well.
			assert(edge_info.midpoint_vert_index > 0);

			// Get the midpoint uv index
			/*unsigned int midpoint_uv_index = 0;
			if(num_uv_sets > 0)
			{
				const uint32_t uv0 = edge_in.uv_indices[0];
				const uint32_t uv1 = edge_in.uv_indices[1];
				const DUVertIndexPair uv_edge_key(myMin(uv0, uv1), myMax(uv0, uv1)); // Key for the edge
				//assert(uv_edge_map.find(uv_edge_key) != uv_edge_map.end());
				//TEMP midpoint_uv_index = uv_edge_map[uv_edge_key];
			}*/

			// NOTE TODO: We have up to 2 UVs to choose from here, UV from primitive 'a' and UV from primitive 'b'.
			// Just use prim 'a' UV for now. Might be wrong, look into this.
			unsigned int midpoint_uv_index = edge_info.prim_a_midpoint_uv_index;

			// Create new edges
			edges_out.push_back(DUEdge(
				edge_in.vertex_indices[0],
				edge_info.midpoint_vert_index,
				edge_in.uv_indices[0],
				midpoint_uv_index
			));

			edges_out.push_back(DUEdge(
				edge_info.midpoint_vert_index,
				edge_in.vertex_indices[1],
				midpoint_uv_index,
				edge_in.uv_indices[1]
			));
		}
		else
		{
			// Not being subdivided, so just copy over polygon
			edges_out.push_back(edge_in);
		}
	}

	//========================== Subdivide triangles ==========================
	DISPLACEMENT_RESET_TIMER(timer);

	tris_out.resize(total_new_tris);

	SubdividePrimitiveTaskClosure subdiv_closure;
	subdiv_closure.subdividing_tri = &subdividing_tri;
	subdiv_closure.subdividing_quad = &subdividing_quad;
	subdiv_closure.tri_write_index = &tri_write_index;
	subdiv_closure.quad_write_index = &quad_write_index;
	subdiv_closure.num_uv_sets = num_uv_sets;
	subdiv_closure.tris_in = &tris_in;
	subdiv_closure.tris_out = &tris_out;
	subdiv_closure.quads_in = &quads_in;
	subdiv_closure.quads_out = &quads_out;
	subdiv_closure.quad_centre_data = &quad_centre_data;
	subdiv_closure.edge_info_map = &edge_info_map;

	task_manager.runParallelForTasks<SubdivideTriTask, SubdividePrimitiveTaskClosure>(subdiv_closure, 0, tris_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   Making new subdivided tris: " + timer.elapsedStringNPlaces(5)));

	//========================== Subdivide quads ==========================
	DISPLACEMENT_RESET_TIMER(timer);

	quads_out.resize(total_new_quads);

	task_manager.runParallelForTasks<SubdivideQuadTask, SubdividePrimitiveTaskClosure>(subdiv_closure, 0, quads_in.size());

	DISPLACEMENT_PRINT_RESULTS(conPrint("   Making new subdivided quads: " + timer.elapsedStringNPlaces(5)));



	print_output.print("\t\tnum triangles subdivided: " + toString(num_tris_subdivided));
	print_output.print("\t\tnum triangles unchanged: " + toString(num_tris_unchanged));
	print_output.print("\t\tnum quads subdivided: " + toString(num_quads_subdivided));
	print_output.print("\t\tnum quads unchanged: " + toString(num_quads_unchanged));
}


static inline float evalW(unsigned int n_t, unsigned int n_q)
{
	if(n_q == 0 && n_t == 3)
		return 1.5f;
	else
		return 12.0f / (3.0f * (float)n_q + 2.0f * (float)n_t);
}


void DisplacementUtils::averagePass(
	Indigo::TaskManager& task_manager,
	const Polygons& polygons_in,
	const VertsAndUVs& verts_and_uvs_in,
	unsigned int num_uv_sets,
	const DUOptions& options,
	VertsAndUVs& verts_and_uvs_out
	)
{
	const std::vector<DUVertexPolygon>& vert_polygons = polygons_in.vert_polygons;
	const std::vector<DUEdge>& edges= polygons_in.edges;
	const std::vector<DUTriangle>& tris= polygons_in.tris;
	const std::vector<DUQuad>& quads = polygons_in.quads;
	const std::vector<DUVertex>& verts= verts_and_uvs_in.verts;
	const std::vector<Vec2f>& uvs_in = verts_and_uvs_in.uvs;

	std::vector<DUVertex>& new_verts_out = verts_and_uvs_out.verts;
	std::vector<Vec2f>& uvs_out = verts_and_uvs_out.uvs;

	// Init vertex positions to (0,0,0)
	const size_t verts_size = verts.size();
	new_verts_out = verts;
	for(size_t v = 0; v < verts_size; ++v)
		new_verts_out[v].pos = new_verts_out[v].normal = Vec3f(0.f, 0.f, 0.f);

	// Init vertex UVs to (0,0)
	uvs_out.resize(uvs_in.size());
	const bool has_uvs = uvs_in.size() > 0;
	for(size_t v = 0; v < uvs_out.size(); ++v)
		uvs_out[v] = Vec2f(0.f, 0.f);

	const size_t tris_size = tris.size();
	const size_t quads_size = quads.size();

	std::vector<uint32_t> dim(verts_size, 2); // array containing dimension of each vertex
	std::vector<float> total_weight(verts_size, 0.0f); // total weights per vertex
	std::vector<uint32_t> n_t(verts_size, 0); // array containing number of triangles touching each vertex
	std::vector<uint32_t> n_q(verts_size, 0); // array containing number of quads touching each vertex

	std::vector<Vec2f> old_vert_uvs(verts_size * num_uv_sets, Vec2f(0.0f, 0.0f));
	std::vector<Vec2f> new_vert_uvs(verts_size * num_uv_sets, Vec2f(0.0f, 0.0f));


	// Initialise dim
	for(size_t i = 0; i < edges.size(); ++i)
	{
		dim[edges[i].vertex_indices[0]] = 1;
		dim[edges[i].vertex_indices[1]] = 1;
	}

	for(size_t i = 0; i < vert_polygons.size(); ++i)
		dim[vert_polygons[i].vertex_index] = 0;

	// Initialise n_t
	for(size_t t = 0; t < tris_size; ++t)
		for(uint32_t v = 0; v < 3; ++v)
			n_t[tris[t].vertex_indices[v]]++;

	// Initialise n_q
	for(size_t q = 0; q < quads_size; ++q)
		for(uint32_t v = 0; v < 4; ++v)
			n_q[quads[q].vertex_indices[v]]++;


	// Set old_vert_uvs

	if(has_uvs)
	{
		for(size_t t = 0; t < tris_size; ++t)
			for(size_t i = 0; i < 3; ++i)
			{
				const uint32 v_i = tris[t].vertex_indices[i];
				old_vert_uvs[v_i] = uvs_in[tris[t].uv_indices[i]];
			}

		for(size_t q = 0; q < quads_size; ++q)
			for(size_t i = 0; i < 4; ++i)
			{
				const uint32 v_i = quads[q].vertex_indices[i];
				old_vert_uvs[v_i] = uvs_in[quads[q].uv_indices[i]];
			}
	}


	// For each vertex polygon
	for(size_t q = 0; q < vert_polygons.size(); ++q)
	{
		const DUVertexPolygon& v = vert_polygons[q];

		// i == 0 as there is only one vertex in this polygon.

		const uint32_t v_i = v.vertex_index;
		const uint32_t uv_i = v.uv_index;

		if(dim[v_i] == 0) // Dimension of Vertex polygon == 0
		{
			const float weight = 1.0f;

			const Vec3f cent = verts[v_i].pos;

			total_weight[v_i] += weight;
			new_verts_out[v_i].pos += cent * weight;

			for(uint32_t z = 0; z < num_uv_sets; ++z)
				new_vert_uvs[v_i * num_uv_sets + z] += getUVs(uvs_in, num_uv_sets, uv_i, z) * weight; //uv_cent[z] = getUVs(uvs_in, num_uv_sets, uv_i, z);
		}
	}

	// For each edge polygon
	for(size_t e = 0; e < edges.size(); ++e)
	{
		const DUEdge& edge = edges[e];

		for(uint32_t i = 0; i < 2; ++i) // For each vertex on the edge
		{
			const uint32_t v_i = edge.vertex_indices[i];
			const uint32_t v_i1 = edge.vertex_indices[mod2(i + 1)];

			const uint32_t uv_i = edge.uv_indices[i];
			const uint32_t uv_i1 = edge.uv_indices[mod2(i + 1)];

			if(dim[v_i] == 1) // Dimension of Edge polygon == 1
			{
				const float weight = 1.0f;

				const Vec3f cent = (verts[v_i].pos + verts[v_i1].pos) * 0.5f;

				for(uint32_t z = 0; z < num_uv_sets; ++z)
				{
					const Vec2f uv_cent = (
						getUVs(uvs_in, num_uv_sets, uv_i, z) + 
						getUVs(uvs_in, num_uv_sets, uv_i1, z)
					) * 0.5f;

					new_vert_uvs[v_i * num_uv_sets + z] += uv_cent * weight; //uv_cent[z] = getUVs(uvs_in, num_uv_sets, uv_i, z);
				}

				total_weight[v_i] += weight;
				new_verts_out[v_i].pos += cent * weight;
			}
		}
	}

	
	for(size_t t = 0; t < tris_size; ++t) // For each triangle
	{
		const DUTriangle& tri = tris[t];

		for(uint32_t i = 0; i < 3; ++i) // For each vertex
		{
			const uint32_t v_i = tri.vertex_indices[i];

			if(dim[v_i] == 2) // Only add centroid if vertex has same dimension as polygon
			{
				const uint32_t v_i_plus_1  = tri.vertex_indices[mod3(i + 1)];
				const uint32_t v_i_minus_1 = tri.vertex_indices[mod3(i + 2)];

				const float weight = (float)(NICKMATHS_PI / 3.0);
	
				const Vec3f cent = verts[v_i].pos * (1.0f / 4.0f) + (verts[v_i_plus_1].pos + verts[v_i_minus_1].pos) * (3.0f / 8.0f);

				for(uint32_t z = 0; z < num_uv_sets; ++z)
				{
					const Vec2f uv_cent =
						getUVs(uvs_in, num_uv_sets, tri.uv_indices[i], z) * (1.0f / 4.0f) + 
						(getUVs(uvs_in, num_uv_sets, tri.uv_indices[mod3(i + 1)], z) + getUVs(uvs_in, num_uv_sets, tri.uv_indices[mod3(i + 2)], z)) * (3.0f / 8.0f);

					new_vert_uvs[v_i * num_uv_sets + z] += uv_cent * weight;
				}

			
				total_weight[v_i] += weight;
				
				// Add cent to new vertex position
				new_verts_out[v_i].pos += cent * weight;
			}
		}
	}

	
	for(size_t q = 0; q < quads_size; ++q) // For each quad
	{
		const DUQuad& quad = quads[q];

		for(uint32_t i = 0; i < 4; ++i) // For each vertex
		{
			const uint32_t v_i = quad.vertex_indices[i];

			if(dim[v_i] == 2) // Only add centroid if vertex has same dimension as polygon
			{
				const uint32_t v_i1 = quads[q].vertex_indices[mod4(i + 1)];
				const uint32_t v_i2 = quads[q].vertex_indices[mod4(i + 2)];
				const uint32_t v_i3 = quads[q].vertex_indices[mod4(i + 3)];

				const float weight = (float)(NICKMATHS_PI / 2.0);

				const Vec3f cent = (verts[v_i].pos + verts[v_i1].pos + verts[v_i2].pos + verts[v_i3].pos) * 0.25f;

				for(uint32_t z = 0; z < num_uv_sets; ++z)
				{
					const Vec2f uv_cent = (
						getUVs(uvs_in, num_uv_sets, quad.uv_indices[i], z) + 
						getUVs(uvs_in, num_uv_sets, quad.uv_indices[mod4(i + 1)], z) + 
						getUVs(uvs_in, num_uv_sets, quad.uv_indices[mod4(i + 2)], z) + 
						getUVs(uvs_in, num_uv_sets, quad.uv_indices[mod4(i + 3)], z)
						) * 0.25f;

					new_vert_uvs[v_i * num_uv_sets + z] += uv_cent * weight;
				}


				total_weight[v_i] += weight;

				// Add cent to new vertex position
				new_verts_out[v_i].pos += cent * weight;
			}
		}
	}


	// Do 'normalize vertices by the weights' and 'apply correction only for quads and triangles' step.
	assert(verts_size == new_verts_out.size());
	for(size_t v = 0; v < verts_size; ++v)
	{
		const float weight = total_weight[v];

		new_verts_out[v].pos /= weight; // Normalise vertex positions by the weights

		for(uint32 z = 0; z < num_uv_sets; ++z)
			new_vert_uvs[v * num_uv_sets + z] /= weight; // Normalise vertex UVs by the weights
		
		if(dim[v] == 2) // Apply correction only for quads and triangles
		{
			const float w = evalW(n_t[v], n_q[v]);

			new_verts_out[v].pos = Maths::uncheckedLerp(
				verts[v].pos, 
				new_verts_out[v].pos, 
				w
			);

			for(uint32_t z = 0; z < num_uv_sets; ++z)
				new_vert_uvs[v * num_uv_sets + z] = Maths::uncheckedLerp(
					old_vert_uvs[v * num_uv_sets + z], 
					new_vert_uvs[v * num_uv_sets + z], 
					w
				);
		} // end if dim == 2
	}

	// Set all anchored vertex positions back to the midpoint between the vertex's 'parent positions'
	for(size_t v = 0; v < verts_size; ++v)
		if(verts[v].anchored)
			new_verts_out[v].pos = (new_verts_out[verts[v].adjacent_vert_0].pos + new_verts_out[verts[v].adjacent_vert_1].pos) * 0.5f;


	if(has_uvs)
	{
		for(size_t q = 0; q < quads_size; ++q)
			for(uint32 i = 0; i < 4; ++i)
			{
				// If edge i has a UV discontinuity, this means we should use the un-averaged UVs at vertex i and vertex i + 1.
				// So a vertex i should use the un-averaged UVs if edge i or edge i-1 have UV discontinuities.
				const uint32 v_i = quads[q].vertex_indices[i];
				const uint32 uv_i = quads[q].uv_indices[i];

				if(quads[q].edge_uv_discontinuity[i] || quads[q].edge_uv_discontinuity[mod4(i + 3)])
					uvs_out[uv_i] = uvs_in[uv_i];
				else
					uvs_out[uv_i] = new_vert_uvs[v_i];
			}

		for(size_t t = 0; t < tris_size; ++t)
			for(uint32 i = 0; i < 3; ++i)
			{
				const uint32 v_i = tris[t].vertex_indices[i];
				const uint32 uv_i = tris[t].uv_indices[i];

				if(tris[t].edge_uv_discontinuity[i] || tris[t].edge_uv_discontinuity[mod3(i + 2)])
					uvs_out[uv_i] = uvs_in[uv_i];
				else
					uvs_out[uv_i] = new_vert_uvs[v_i];
			}
	}
}




#if BUILD_TESTS


#include "../graphics/Map2D.h"


void DisplacementUtils::test()
{
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

		// Each tri gets subdivided into 4 tris, for a total of 2*4 = 8 tris
		// Plus 4 quads * 2 tris/quad for the subdivided quad gives 8 + 8 = 16 tris.
		testAssert(triangles_out.size() == 16);
	}
}


#endif // BUILD_TESTS
