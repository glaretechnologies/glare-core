/*=====================================================================
UVUnwrapper.cpp
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "UVUnwrapper.h"


#include "../dll/include/IndigoException.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/SmallVector.h"
#include "../utils/HashMapInsertOnly2.h"
#include <unordered_set>

//TEMP:
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/TaskManager.h"
#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"
#include "../graphics/Drawing.h"
#include "../graphics/ShelfPack.h"
#include "../maths/PCG32.h"
#include "../maths/Matrix4f.h"
#include "../maths/Matrix2.h"


UVUnwrapper::UVUnwrapper()
{
}


UVUnwrapper::~UVUnwrapper()
{
}


struct Patch
{
	std::vector<size_t> poly_indices;
	Vec2f min_bound;
	Vec2f max_bound;
};


struct VertUVs
{
	VertUVs()
	{
		for(int i=0; i<4; ++i)
			set_uvs[i] = Indigo::Vec2f(0.f);
	}

	Indigo::Vec2f set_uvs[4];
};

struct VertUVsLessThan
{
	bool operator() (const VertUVs& a, const VertUVs& b)
	{
		for(int i=0; i<num_sets; ++i)
		{
			if(a.set_uvs[i] < b.set_uvs[i])
				return true;
			else if(b.set_uvs[i] < a.set_uvs[i])
				return false;
		}
		return false; // a and b are equal
	}

	int num_sets;
};



struct UnwrapperEdgeKey
{
	UnwrapperEdgeKey(){}
	UnwrapperEdgeKey(const Indigo::Vec3f& a, const Indigo::Vec3f& b) : start(a), end(b), start_normal(Indigo::Vec3f(0.f)), end_normal(Indigo::Vec3f(0.f)) {}

	Indigo::Vec3f start;
	Indigo::Vec3f end;

	Indigo::Vec3f start_normal;
	Indigo::Vec3f end_normal;

	inline bool operator == (const UnwrapperEdgeKey& other) const { return start == other.start && end == other.end && start_normal == other.start_normal && end_normal == other.end_normal; }
	inline bool operator != (const UnwrapperEdgeKey& other) const { return start != other.start || end != other.end || start_normal != other.start_normal || end_normal != other.end_normal; }
};


class UnwrapperEdgeKeyHash
{
public:
	inline size_t operator()(const UnwrapperEdgeKey& v) const
	{
		return hashBytes((const uint8*)&v, sizeof(UnwrapperEdgeKey));
	}
};


/*
A line parallel to one of the axes.

For edges pointing along the x axis:
start.x will be zero.
*/
struct AxisAlignedEdgeKey
{
	AxisAlignedEdgeKey() {}
	AxisAlignedEdgeKey(const Indigo::Vec3f& start_, int axis_) : start(start_), axis(axis_) {}

	Indigo::Vec3f start; // Point on axis-aligned line where it intersects the plane formed by the other two axes
	int axis; // axis this line is aligned with.

	inline bool operator == (const AxisAlignedEdgeKey& other) const { return start == other.start && axis == other.axis; }
	inline bool operator != (const AxisAlignedEdgeKey& other) const { return start != other.start || axis != other.axis; }
};


class AxisAlignedEdgeKeyHash
{
public:
	inline size_t operator()(const AxisAlignedEdgeKey& v) const
	{
		return hashBytes((const uint8*)&v.start, sizeof(Indigo::Vec3f));
	}
};


struct UnwrapperEdgeInfo
{
	SmallVector<uint32, 2> adjacent_polys;
};


// Used in UnwrapperAlignedEdgeInfo:
struct AdjacentPolyRef
{
	uint32 poly_index; // Index of the adjacent polygon
	uint32 edge_index; // Index of the edge lying on the line, in the adjacent polygon.
};

struct UnwrapperAlignedEdgeInfo
{
	SmallVector<AdjacentPolyRef, 2> axis_adjacent_polys; // Polygons with edges aligned along an axis-aligned line.
	Indigo::Vec3f start; // Point on axis-aligned line where it intersects the plane formed by the other two axes
	int axis; // axis this line is aligned with.
};

static const uint32_t mod3_table[] = { 0, 1, 2, 0, 1, 2 };

inline static uint32 mod3(uint32 x)
{
	return mod3_table[x];
}

inline static uint32 mod4(uint32 x)
{
	return x & 0x3;
}


struct UnwrapperPoly
{
	Vec2f vert_uvs[4]; // Lightmap UVs for the vertex, in patch space.
	int edge_i[4];
	int aligned_edge_i[4];
	int num_edges;
};



static Vec4f toVec4fVector(const Indigo::Vec3f& v)
{
	return Vec4f(v.x, v.y, v.z, 0.f);
}


static inline ::Vec2f toVec2f(const Indigo::Vec2f& v)
{
	return Vec2f(v.x, v.y);
}


#ifndef NDEBUG
static Indigo::Vec3f zeroComponent(const Indigo::Vec3f& v, int i)
{
	Indigo::Vec3f res = v;
	res[i] = 0.f;
	return res;
}
#endif


UVUnwrapper::Results UVUnwrapper::build(Indigo::Mesh& mesh, const Matrix4f& ob_to_world, PrintOutput& print_output, float normed_margin)
{
	// Built topology info (adjacency etc..)
	// DisplacementUtils::initAndBuildAdjacencyInfo isn't quite the right fit for this, since it fails to handle
	// more than 2 polygons adjacent to an edge, which we want to handle (think adjacent voxels)

	Results results;

	const Indigo::Vec3f*    const vert_pos_in = mesh.vert_positions.data();
	const Indigo::Vec3f*    const vert_normal_in = mesh.vert_normals.data();
	const Indigo::Triangle* const tris_in  = mesh.triangles.data();
	const Indigo::Quad*     const quads_in = mesh.quads.data();
	const Indigo::Vec2f*    const uvs_in   = mesh.uv_pairs.data();
	const size_t triangles_in_size = mesh.triangles.size();
	const size_t quads_in_size     = mesh.quads.size();

	std::vector<UnwrapperPoly> polys(triangles_in_size + quads_in_size);

	std::vector<UnwrapperEdgeInfo> edges;
	edges.reserve(polys.size() * 4);

	std::vector<UnwrapperAlignedEdgeInfo> aligned_edges;
	aligned_edges.reserve(polys.size() * 4);

	// Build adjacency info, build polys vector.
	{
		const Indigo::Vec3f infv(std::numeric_limits<float>::infinity());
		HashMapInsertOnly2<UnwrapperEdgeKey, int, UnwrapperEdgeKeyHash> edge_indices_map(UnwrapperEdgeKey(infv, infv),
			(triangles_in_size + quads_in_size) * 2 // expected_num_items
		);

		const AxisAlignedEdgeKey empty_key(infv, 0);
		HashMapInsertOnly2<AxisAlignedEdgeKey, int, AxisAlignedEdgeKeyHash> axis_aligned_edges_map(empty_key,
			(triangles_in_size + quads_in_size) * 2 // expected_num_items
		);

		for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
		{
			const Indigo::Triangle& tri_in = tris_in[t];

			for(unsigned int i = 0; i < 3; ++i) // For each edge
			{
				const unsigned int i1 = mod3(i + 1); // Next vert
				const uint32 vi  = tri_in.vertex_indices[i];
				const uint32 vi1 = tri_in.vertex_indices[i1];

				UnwrapperEdgeKey edge_key;
				const Indigo::Vec3f vi_pos    = vert_pos_in[vi ];
				const Indigo::Vec3f vi1_pos   = vert_pos_in[vi1];
				const Indigo::Vec3f normal_i  = vert_normal_in ? vert_normal_in[vi ] : Indigo::Vec3f(0.f);
				const Indigo::Vec3f normal_i1 = vert_normal_in ? vert_normal_in[vi1] : Indigo::Vec3f(0.f);
				if(vi_pos < vi1_pos)
				{
					edge_key.start = vi_pos;
					edge_key.end = vi1_pos;
					edge_key.start_normal = normal_i;
					edge_key.end_normal = normal_i1;
				}
				else
				{
					edge_key.start = vi1_pos;
					edge_key.end = vi_pos;
					edge_key.start_normal = normal_i1;
					edge_key.end_normal = normal_i;
				}

				auto result = edge_indices_map.find(edge_key); // Lookup edge
				if(result == edge_indices_map.end()) // If edge not added yet:
				{
					const size_t edge_i = edges.size();
					edges.resize(edge_i + 1);

					edges[edge_i].adjacent_polys.push_back((uint32)t); // Add this tri to the list of polys adjacent to edge.

					edge_indices_map.insert(std::make_pair(edge_key, (int)edge_i)); // Add edge index to map

					polys[t].edge_i[i] = (int)edge_i;
				}
				else
				{
					edges[result->second].adjacent_polys.push_back((uint32)t); // Add this tri to the list of polys adjacent to edge.

					polys[t].edge_i[i] = (int)result->second;
				}


				// Process any axis-aligned edges
				const Indigo::Vec3f edge_vec = vi1_pos - vi_pos;

				// Work out which axis, if any, this edge is parallel to.
				int axis = -1;
				if(edge_vec.y == 0 && edge_vec.z == 0)
					axis = 0;
				else if(edge_vec.x == 0 && edge_vec.z == 0)
					axis = 1;
				else if(edge_vec.x == 0 && edge_vec.y == 0)
					axis = 2;
				
				if(axis >= 0) // If edge was axis-aligned:
				{
					// Get point on line where it intersects the plane formed by the other two axes
					Indigo::Vec3f start = vi_pos;
					start[axis] = 0;

					const AxisAlignedEdgeKey aligned_edge_key(start, axis);

					AdjacentPolyRef ref;
					ref.poly_index = (uint32)t;
					ref.edge_index = i;

					auto edge_res = axis_aligned_edges_map.find(aligned_edge_key);
					if(edge_res == axis_aligned_edges_map.end()) // If aligned edge not added yet:
					{
						const size_t edge_i = aligned_edges.size();
						aligned_edges.resize(edge_i + 1);

						aligned_edges[edge_i].axis_adjacent_polys.push_back(ref); // Add the current tri to the list of adjacent polygons.
						aligned_edges[edge_i].start = start;
						aligned_edges[edge_i].axis = axis;

						axis_aligned_edges_map.insert(std::make_pair(aligned_edge_key, (int)edge_i)); // Add edge index to map

						polys[t].aligned_edge_i[i] = (int)edge_i;
					}
					else
					{
						aligned_edges[edge_res->second].axis_adjacent_polys.push_back(ref);

						polys[t].aligned_edge_i[i] = (int)edge_res->second;
					}
				}
				else
				{
					polys[t].aligned_edge_i[i] = -1;
				}

				polys[t].num_edges = 3;
			}
		}

		for(size_t q = 0; q < quads_in_size; ++q) // For each quad
		{
			const Indigo::Quad& quad_in = quads_in[q];
			
			const size_t poly_i = triangles_in_size + q;

			for(unsigned int i = 0; i < 4; ++i) // For each edge
			{
				const unsigned int i1 = mod4(i + 1); // Next vert
				const uint32 vi  = quad_in.vertex_indices[i];
				const uint32 vi1 = quad_in.vertex_indices[i1];

				UnwrapperEdgeKey edge_key;
				const Indigo::Vec3f vi_pos    = vert_pos_in[vi ];
				const Indigo::Vec3f vi1_pos   = vert_pos_in[vi1];
				const Indigo::Vec3f normal_i  = vert_normal_in ? vert_normal_in[vi ] : Indigo::Vec3f(0.f);
				const Indigo::Vec3f normal_i1 = vert_normal_in ? vert_normal_in[vi1] : Indigo::Vec3f(0.f);
				if(vi_pos < vi1_pos)
				{
					edge_key.start = vi_pos;
					edge_key.end = vi1_pos;
					edge_key.start_normal = normal_i;
					edge_key.end_normal = normal_i1;
				}
				else
				{
					edge_key.start = vi1_pos;
					edge_key.end = vi_pos;
					edge_key.start_normal = normal_i1;
					edge_key.end_normal = normal_i;
				}

				auto result = edge_indices_map.find(edge_key); // Lookup edge
				if(result == edge_indices_map.end()) // If edge not added yet:
				{
					const size_t edge_i = edges.size();
					edges.resize(edge_i + 1);

					edges[edge_i].adjacent_polys.push_back((uint32)poly_i);

					edge_indices_map.insert(std::make_pair(edge_key, (int)edge_i)); // Add edge index to map

					polys[poly_i].edge_i[i] = (int)edge_i;
				}
				else
				{
					edges[result->second].adjacent_polys.push_back((uint32)poly_i);

					polys[poly_i].edge_i[i] = (int)result->second;
				}
				
				// Process axis-aligned edge lines
				const Indigo::Vec3f edge_vec = vi1_pos - vi_pos;

				int axis = -1;
				if(edge_vec.y == 0 && edge_vec.z == 0)
					axis = 0;
				else if(edge_vec.x == 0 && edge_vec.z == 0)
					axis = 1;
				else if(edge_vec.x == 0 && edge_vec.y == 0)
					axis = 2;

				if(axis >= 0)
				{
					Indigo::Vec3f start = vi_pos;
					start[axis] = 0;

					const AxisAlignedEdgeKey aligned_edge_key(start, axis);

					AdjacentPolyRef ref;
					ref.poly_index = (uint32)poly_i;
					ref.edge_index = i;

					auto edge_res = axis_aligned_edges_map.find(aligned_edge_key);
					if(edge_res == axis_aligned_edges_map.end()) // If aligned edge not added yet:
					{
						const size_t edge_i = aligned_edges.size();
						aligned_edges.resize(edge_i + 1);

						aligned_edges[edge_i].axis_adjacent_polys.push_back(ref);
						aligned_edges[edge_i].start = start;
						aligned_edges[edge_i].axis = axis;

						axis_aligned_edges_map.insert(std::make_pair(aligned_edge_key, (int)edge_i)); // Add edge index to map

						polys[poly_i].aligned_edge_i[i] = (int)edge_i;
					}
					else
					{
						aligned_edges[edge_res->second].axis_adjacent_polys.push_back(ref);

						polys[poly_i].aligned_edge_i[i] = (int)edge_res->second;
					}
				}
				else
				{
					polys[poly_i].aligned_edge_i[i] = -1;
				}

				polys[poly_i].num_edges = 4;
			}
		}
	}

	// For each polygon, create a chart of adjacent unprocessed triangles
	std::vector<bool> poly_processed(polys.size()); // Has this polygon been added to a patch yet?

	std::vector<Patch> patches;
	patches.reserve(polys.size() / 8);

	std::vector<size_t> patch_polys_to_process; // for patch

	for(size_t initial_i=0; initial_i<poly_processed.size(); ++initial_i)
	{
		if(!poly_processed[initial_i])
		{
			// Start a patch
			const size_t patch_i = patches.size();
			patches.push_back(Patch());
			Patch& patch = patches[patch_i];

			poly_processed[initial_i] = true;
			patch_polys_to_process.clear();

			Vec4f edge_0_1;
			Vec4f patch_normal_ws;
			if(initial_i < triangles_in_size)
			{
				const Indigo::Triangle& tri = tris_in[initial_i];

							edge_0_1 = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[tri.vertex_indices[1]]) - toVec4fVector(vert_pos_in[tri.vertex_indices[0]]));
				const Vec4f edge_0_2 = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[tri.vertex_indices[2]]) - toVec4fVector(vert_pos_in[tri.vertex_indices[0]]));
				patch_normal_ws = normalise(crossProduct(edge_0_1, edge_0_2));
			}
			else
			{
				const Indigo::Quad& quad = quads_in[initial_i - triangles_in_size];

				            edge_0_1 = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[quad.vertex_indices[1]]) - toVec4fVector(vert_pos_in[quad.vertex_indices[0]]));
				const Vec4f edge_0_2 = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[quad.vertex_indices[2]]) - toVec4fVector(vert_pos_in[quad.vertex_indices[0]]));
				patch_normal_ws = normalise(crossProduct(edge_0_1, edge_0_2));
			}
			
			const Vec4f basis_i = normalise(edge_0_1);
			const Vec4f basis_j = crossProduct(patch_normal_ws, basis_i);

			patch_polys_to_process.push_back(initial_i);

			while(!patch_polys_to_process.empty())
			{
				const size_t poly_i = patch_polys_to_process.back();
				UnwrapperPoly& poly = polys[poly_i];
				patch_polys_to_process.pop_back();

				Indigo::Vec3f poly_vertpos[4];
				if(poly_i < triangles_in_size)
				{
					const Indigo::Triangle& tri = tris_in[poly_i];
					
					// Compute patch UV coords for poly_i
					for(int z=0; z<3; ++z)
					{
						poly_vertpos[z] = vert_pos_in[tri.vertex_indices[z]];
						const Vec4f v_pos = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[tri.vertex_indices[z]]));
						const float u = dot(v_pos, basis_i);
						const float v = dot(v_pos, basis_j);

						poly.vert_uvs[z].x = u;
						poly.vert_uvs[z].y = v;
					}
				}
				else
				{
					const Indigo::Quad& quad = quads_in[poly_i - triangles_in_size];

					// Compute patch UV coords for poly_i
					for(int z=0; z<4; ++z)
					{
						poly_vertpos[z] = vert_pos_in[quad.vertex_indices[z]];
						const Vec4f v_pos = ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[quad.vertex_indices[z]]));
						const float u = dot(v_pos, basis_i);
						const float v = dot(v_pos, basis_j);

						poly.vert_uvs[z].x = u;
						poly.vert_uvs[z].y = v;
					}
				}

				patch.poly_indices.push_back(poly_i); // Add to list of polys in patch


				// Add adjacent polygons to poly_i to polys_to_process, if applicable
				for(int e=0; e<polys[poly_i].num_edges; ++e)
				{
					// Check for adjacent polygons where the other polygon shares the edge with this polygon.
					bool found_adjacent_poly = false;
					{
						const int edge_i = poly.edge_i[e];
						const UnwrapperEdgeInfo& edge = edges[edge_i];

						for(size_t q=0; q<edge.adjacent_polys.size(); ++q)
						{
							const size_t adj_poly_i = edge.adjacent_polys[q];
							if((adj_poly_i == poly_i) || poly_processed[adj_poly_i])
								continue;

							// See if normal of adjacent polygon is sufficiently similar
							Vec4f adj_normal;
							if(adj_poly_i < triangles_in_size)
							{
								const Indigo::Triangle& adj_tri = tris_in[adj_poly_i];
								adj_normal = normalise(crossProduct(
									ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_tri.vertex_indices[1]]) - toVec4fVector(vert_pos_in[adj_tri.vertex_indices[0]])),
									ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_tri.vertex_indices[2]]) - toVec4fVector(vert_pos_in[adj_tri.vertex_indices[0]]))
								));
							}
							else
							{
								const Indigo::Quad& adj_quad = quads_in[adj_poly_i - triangles_in_size];
								adj_normal = normalise(crossProduct(
									ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_quad.vertex_indices[1]]) - toVec4fVector(vert_pos_in[adj_quad.vertex_indices[0]])),
									ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_quad.vertex_indices[2]]) - toVec4fVector(vert_pos_in[adj_quad.vertex_indices[0]]))
								));
							}

							if(dot(patch_normal_ws, adj_normal) > 0.9f)
							{
								patch_polys_to_process.push_back(adj_poly_i); // Add the adjacent poly to set of polys to add to patch.

								poly_processed[adj_poly_i] = true;
								found_adjacent_poly = true;
							}
						}
					}

					if(!found_adjacent_poly)
					{
						// Check for adjacent polygons where the other polygon shares part of an aligned edge with this polygon.
						const int aligned_edge_i = poly.aligned_edge_i[e];
						if(aligned_edge_i >= 0) // If this edge has an adjacent axis-aligned edge:
						{
							const unsigned int e1 = (poly.num_edges == 4) ? mod4(e + 1) : mod3(e + 1); // Next vert
							const Indigo::Vec3f ve_pos  = poly_vertpos[e];
							const Indigo::Vec3f ve1_pos = poly_vertpos[e1];

							const UnwrapperAlignedEdgeInfo& edge = aligned_edges[aligned_edge_i];
							const float edge_interval_a = myMin(ve_pos[edge.axis], ve1_pos[edge.axis]);
							const float edge_interval_b = myMax(ve_pos[edge.axis], ve1_pos[edge.axis]);

							// Iterate over polygons adjacent to this axis-aligned line
							for(size_t q = 0; q < edge.axis_adjacent_polys.size(); ++q)
							{
								const size_t adj_poly_i      = edge.axis_adjacent_polys[q].poly_index;
								const uint32 adj_poly_edge_i = edge.axis_adjacent_polys[q].edge_index;
								if((adj_poly_i == poly_i) || poly_processed[adj_poly_i])
									continue;

								// See if normal of adjacent polygon is sufficiently similar
								Vec4f adj_normal;
								if(adj_poly_i < triangles_in_size) // if the adjacent poly is a triangle:
								{
									const Indigo::Triangle& adj_tri = tris_in[adj_poly_i];
									adj_normal = normalise(crossProduct(
										ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_tri.vertex_indices[1]]) - toVec4fVector(vert_pos_in[adj_tri.vertex_indices[0]])),
										ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_tri.vertex_indices[2]]) - toVec4fVector(vert_pos_in[adj_tri.vertex_indices[0]]))
									));

									if(dot(patch_normal_ws, adj_normal) > 0.9f)
									{
										const unsigned int z = adj_poly_edge_i;
										const unsigned int z1 = mod3(z + 1); // Next vert
										const Indigo::Vec3f vz_pos  = vert_pos_in[adj_tri.vertex_indices[z]];
										const Indigo::Vec3f vz1_pos = vert_pos_in[adj_tri.vertex_indices[z1]];

										assert(zeroComponent(vz1_pos - vz_pos, edge.axis) == Indigo::Vec3f(0.f)); // Adj tri edge should point along axis.
										assert(zeroComponent(vz_pos, edge.axis) == edge.start); // Adj tri edge should lie on edge line.

										const float interval_a = myMin(vz_pos[edge.axis], vz1_pos[edge.axis]);
										const float interval_b = myMax(vz_pos[edge.axis], vz1_pos[edge.axis]);

										// See if the intervals overlap at all
										if(interval_a < edge_interval_b && interval_b > edge_interval_a) // if(!(interval_a >= edge_interval_b || interval_b <= edge_interval_a))
										{
											// Edges overlap along line.
											patch_polys_to_process.push_back(adj_poly_i); // Add the adjacent poly to set of polys to add to patch.
											poly_processed[adj_poly_i] = true;
										}
									}
								}
								else // else the adjacent poly is a quad:
								{
									const Indigo::Quad& adj_quad = quads_in[adj_poly_i - triangles_in_size];
									adj_normal = normalise(crossProduct(
										ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_quad.vertex_indices[1]]) - toVec4fVector(vert_pos_in[adj_quad.vertex_indices[0]])),
										ob_to_world.mul3Vector(toVec4fVector(vert_pos_in[adj_quad.vertex_indices[2]]) - toVec4fVector(vert_pos_in[adj_quad.vertex_indices[0]]))
									));

									if(dot(patch_normal_ws, adj_normal) > 0.9f)
									{
										const unsigned int z = adj_poly_edge_i;
										const unsigned int z1 = mod4(z + 1); // Next vert
										const Indigo::Vec3f vz_pos  = vert_pos_in[adj_quad.vertex_indices[z]];
										const Indigo::Vec3f vz1_pos = vert_pos_in[adj_quad.vertex_indices[z1]];

										assert(zeroComponent(vz1_pos - vz_pos, edge.axis) == Indigo::Vec3f(0.f)); // Adj tri edge should point along axis.
										assert(zeroComponent(vz_pos, edge.axis) == edge.start); // Adj tri edge should lie on edge line.
										
										const float interval_a = myMin(vz_pos[edge.axis], vz1_pos[edge.axis]);
										const float interval_b = myMax(vz_pos[edge.axis], vz1_pos[edge.axis]);

										// See if the intervals overlap at all
										if(interval_a < edge_interval_b && interval_b > edge_interval_a) // if(!(interval_a >= edge_interval_b || interval_b <= edge_interval_a))
										{
											// Edges overlap along line.
											patch_polys_to_process.push_back(adj_poly_i); // Add the adjacent poly to set of polys to add to patch.
											poly_processed[adj_poly_i] = true;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Compute bounding boxes of patches
	for(size_t i=0; i<patches.size(); ++i)
	{
		Vec2f min_bound(std::numeric_limits<float>::infinity());
		Vec2f max_bound(-std::numeric_limits<float>::infinity());
		for(size_t z=0; z<patches[i].poly_indices.size(); ++z)
		{
			const UnwrapperPoly& poly = polys[patches[i].poly_indices[z]];
			for(int v=0; v<poly.num_edges; ++v)
			{
				min_bound = min(min_bound, poly.vert_uvs[v]);
				max_bound = max(max_bound, poly.vert_uvs[v]);
			}
		}
		// conPrint("patch " + toString(i) + " has " + toString(patches[i].poly_indices.size()) + " polys");
		// printVar(min_bound);
		// printVar(max_bound);

		patches[i].min_bound = min_bound;
		patches[i].max_bound = max_bound;
	}

	/*printVar(patches.size());
	for(size_t i=0; i<patches.size(); ++i)
	{
		conPrint("patch " + toString(i) + " has " + toString(patches[i].poly_indices.size()) + " polys");
	}*/

	// Create BinRects, one for each patch
	std::vector<BinRect> rects(patches.size());
	for(size_t i=0; i<rects.size(); ++i)
	{
		rects[i].w = patches[i].max_bound.x - patches[i].min_bound.x;
		rects[i].h = patches[i].max_bound.y - patches[i].min_bound.y;
		//rects[i].index = (int)i;
	}


	// Get sum area of rectangles
	float sum_A = 0;
	for(size_t i=0; i<rects.size(); ++i)
		sum_A += rects[i].area();

	// Choose a maximum width (x value) based on the sum of rectangle areas.
	const float max_x = std::sqrt(sum_A) * 1.2f;

	const float normed_margins = normed_margin; // 2.f / 1000;
	const float use_margin = normed_margins * max_x;

	// Make rects slightly larger so they have margins
	for(size_t i=0; i<rects.size(); ++i)
	{
		rects[i].w += 2 * use_margin;
		rects[i].h += 2 * use_margin;
	}

	// Do bin packing
	ShelfPack::shelfPack(rects);

	// Shrink rectangles to remove margins,
	// and adjust position so margins are on all sides.
	for(size_t i=0; i<rects.size(); ++i)
	{
		rects[i].w -= 2 * use_margin;
		rects[i].h -= 2 * use_margin;

		rects[i].pos.x += normed_margins;
		rects[i].pos.y += normed_margins;
	}

	//TEMP
	// Draw results
	/*try
	{
		PCG32 rng(1);
		const int W = 1000;
		Bitmap bitmap(W, W, 3, NULL);
		bitmap.zero();
		for(size_t i=0; i<rects.size(); ++i)
		{
			int r = (int)(rng.unitRandom() * 255);
			int g = (int)(rng.unitRandom() * 255);
			int b = (int)(100 + rng.unitRandom() * 155);

			const int sx = (int)(rects[i].pos.x * W);
			const int sy = (int)(rects[i].pos.y * W);
			const int endx = sx + (int)((rects[i].rotatedWidth())  * rects[i].scale.x * W);
			const int endy = sy + (int)((rects[i].rotatedHeight()) * rects[i].scale.y * W);
			for(int y=sy; y<endy; ++y)
				for(int x=sx; x<endx; ++x)
				{
					if(x >= 0 && x < W && y >= 0 && y < W)
					{
						bitmap.getPixelNonConst(x, y)[0] = (uint8)r;
						bitmap.getPixelNonConst(x, y)[1] = (uint8)g;
						bitmap.getPixelNonConst(x, y)[2] = (uint8)b;
					}
				}
		}

		PNGDecoder::write(bitmap, "binpacking.png");
	}
	catch(glare::Exception& e)
	{
		conPrint("Warning: " + e.what());
	}*/


	// Make a new UV set for the RayMesh.
	const size_t old_num_sets = mesh.num_uv_mappings;
	const size_t new_num_sets = mesh.num_uv_mappings + 1;
	const size_t new_set_index = old_num_sets;
	mesh.num_uv_mappings = (unsigned int)new_num_sets;

	//const std::vector<Vec2f> old_uvs = mesh->uvs;

	



	// Create space for the new vertex UVs, one VertUV object per polygon-vertex.

	std::vector<VertUVs> new_uvs(mesh.triangles.size() * 4 + mesh.quads.size() * 4);

	
	// Copy over old UVs
	/*if(old_num_sets == 0) // If old mesh had no UVs:
	{
		const size_t new_num_uvs = mesh->getTriangles().size() * 3 + mesh->getQuads().size() * 4; // Create one per vert. NOTE: not merged for now.
		mesh->uvs.resize(new_num_uvs);
	}
	else
	{
		mesh->uvs.resize(num_uvs * new_num_sets);
		for(size_t i=0; i<num_uvs; ++i)
			for(size_t s=0; s<old_num_sets; ++s)
				mesh->uvs[i*new_num_sets + s] = old_uvs[i*old_num_sets + s];
	}*/

	/*for(size_t i=0; i<polygons.quads.size(); ++i)
	{
		const DUQuad& poly = polygons.quads[i];
		for(p*/



	// Work out UV coords for triangles
	// uint32 uv_write_i = 0;
	for(size_t i=0; i<rects.size(); ++i)
	{
		const BinRect& rect = rects[i];
		const Patch& patch = patches[i];

		// Work out mapping from Patch UV coords to final, shared UV coords
		Matrix2f patch_to_uv_matrix;
		if(rect.rotated)
			patch_to_uv_matrix = Matrix2f(0, 1, 1, 0); // x and y are swapped
		else
			patch_to_uv_matrix = Matrix2f(1, 0, 0, 1);
		patch_to_uv_matrix = Matrix2f(rect.scale.x, 0, 0, rect.scale.y) * patch_to_uv_matrix;

		Vec2f patch_to_uv_offset = rect.pos;

		for(size_t z=0; z<patch.poly_indices.size(); ++z)
		{
			const size_t poly_i = patch.poly_indices[z];
			const UnwrapperPoly& poly = polys[poly_i];

			if(poly.num_edges == 3) // if poly is a tri:
			{
				const size_t tri_index = poly_i;

				for(int v=0; v<3; ++v) // For each vert
				{
					// Copy any existing non-lightmap UVS to new_uvs.
					for(size_t s=0; s<old_num_sets; ++s)
						new_uvs[poly_i * 4 + v].set_uvs[s] = uvs_in[tris_in[tri_index].uv_indices[v] * old_num_sets + s];

					// We will assign the new UV at the same index as old UV(s), if there were existing UVs
					//uint32 uv_i;
					//if(old_num_sets == 0)
					//{
					//	uv_i = uv_write_i;
					//	mesh->getTriangles()[tri_index].uv_indices[v] = uv_i; // Set the triangle UV index
					//	uv_write_i++;
					//}
					//else
					//	uv_i = mesh->getTriangles()[tri_index].uv_indices[v];

					const Vec2f vert_uv_patch = poly.vert_uvs[v] - patch.min_bound;
					const Vec2f vert_uv = patch_to_uv_matrix * vert_uv_patch + patch_to_uv_offset;

					//mesh->uvs[uv_i*new_num_sets + new_set_index] = vert_uv;

					new_uvs[poly_i * 4 + v].set_uvs[new_set_index] = Indigo::Vec2f(vert_uv.x, vert_uv.y);
				}
			}
			else // else poly is a quad:
			{
				const size_t quad_index = poly_i - mesh.triangles.size();

				for(int v=0; v<4; ++v) // For each vert
				{
					// Copy any existing non-lightmap UVS to new_uvs.
					for(size_t s=0; s<old_num_sets; ++s)
						new_uvs[poly_i * 4 + v].set_uvs[s] = uvs_in[quads_in[quad_index].uv_indices[v] * old_num_sets + s];

					// Get old UV index.  We will assign the new UV at the same index, if there were existing UVs
					/*const uint32 old_uv_i = mesh->getQuads()[quad_index].uv_indices[v];
					const uint32 uv_i = (old_num_sets != 0) ? old_uv_i : uv_write_i;
					uv_write_i++;*/
					/*uint32 uv_i;
					if(old_num_sets == 0)
					{
						uv_i = uv_write_i;
						mesh->getQuads()[quad_index].uv_indices[v] = uv_i; // Set the triangle UV index
						uv_write_i++;
					}
					else
						uv_i = mesh->getQuads()[quad_index].uv_indices[v];*/

					const Vec2f vert_uv_patch = poly.vert_uvs[v] - patch.min_bound;
					const Vec2f vert_uv = patch_to_uv_matrix * vert_uv_patch + patch_to_uv_offset;

					//mesh->uvs[uv_i*new_num_sets + new_set_index] = vert_uv;
					new_uvs[poly_i * 4 + v].set_uvs[new_set_index] = Indigo::Vec2f(vert_uv.x, vert_uv.y);
				}
			}
		}
	}

	// TODO: Now that we have our new UVs, merge them
	//VertUVsLessThan less_than;
	//less_than.num_sets = (int)new_num_sets;
	//std::map<VertUVs, int, VertUVsLessThan> vert_uvs_to_index_map(less_than);


	// TEMP: just copy new UVs
	mesh.uv_pairs.resize(new_uvs.size() * new_num_sets);
	for(size_t v_i=0; v_i<new_uvs.size(); ++v_i)
		for(size_t s=0; s<new_num_sets; ++s)
			mesh.uv_pairs[v_i*new_num_sets + s] = new_uvs[v_i].set_uvs[s];

	// Update triangle/quad uv indices
	for(size_t poly_i=0; poly_i<polys.size(); ++poly_i)
	{
		const UnwrapperPoly& poly = polys[poly_i];
		if(poly.num_edges == 3) // if poly is a tri:
		{
			const size_t tri_index = poly_i;
			for(int v=0; v<3; ++v) // For each vert
				mesh.triangles[tri_index].uv_indices[v] = (uint32)(poly_i * 4 + v);
		}
		else
		{
			const size_t quad_index = poly_i - mesh.triangles.size();
			for(int v=0; v<4; ++v) // For each vert
				mesh.quads[quad_index].uv_indices[v] = (uint32)(poly_i * 4 + v);
		}
	}


	// Draw results
	if(false)
	{
		try
		{
			PCG32 rng(1);
			const int W = 2000;
			Bitmap bitmap(W, W, 3, NULL);
			bitmap.zero();
			for(size_t i=0; i<rects.size(); ++i)
			{
				int r = (int)(rng.unitRandom() * 255);
				int g = (int)(rng.unitRandom() * 255);
				int b = (int)(100 + rng.unitRandom() * 155);

				const int sx = (int)(rects[i].pos.x * W);
				const int sy = (int)(rects[i].pos.y * W);
				const int endx = sx + (int)((rects[i].rotatedWidth())  * rects[i].scale.x * W);
				const int endy = sy + (int)((rects[i].rotatedHeight()) * rects[i].scale.y * W);
				for(int y=sy; y<endy; ++y)
					for(int x=sx; x<endx; ++x)
					{
						if(x >= 0 && x < W && y >= 0 && y < W)
						{
							bitmap.getPixelNonConst(x, y)[0] = (uint8)r;
							bitmap.getPixelNonConst(x, y)[1] = (uint8)g;
							bitmap.getPixelNonConst(x, y)[2] = (uint8)b;
						}
					}
			}

			// Draw triangles and quads
			const int uv_set_index = (int)new_num_sets - 1;
			for(size_t i = 0; i < mesh.triangles.size(); ++i)
			{
				const int uv_i_0 = mesh.triangles[i].uv_indices[0];
				const int uv_i_1 = mesh.triangles[i].uv_indices[1];
				const int uv_i_2 = mesh.triangles[i].uv_indices[2];
				const Indigo::Vec2f uv0 = mesh.uv_pairs[uv_i_0 * mesh.num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv1 = mesh.uv_pairs[uv_i_1 * mesh.num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv2 = mesh.uv_pairs[uv_i_2 * mesh.num_uv_mappings + uv_set_index] * (float)W;

				const Colour3f col(rng.unitRandom(), rng.unitRandom(), 0.5f + 0.5f * rng.unitRandom());

				Drawing::drawLine(bitmap, col, toVec2f(uv0), toVec2f(uv1));
				Drawing::drawLine(bitmap, col, toVec2f(uv1), toVec2f(uv2));
				Drawing::drawLine(bitmap, col, toVec2f(uv2), toVec2f(uv0));
			}
			for(size_t i = 0; i < mesh.quads.size(); ++i)
			{
				const int uv_i_0 = mesh.quads[i].uv_indices[0];
				const int uv_i_1 = mesh.quads[i].uv_indices[1];
				const int uv_i_2 = mesh.quads[i].uv_indices[2];
				const int uv_i_3 = mesh.quads[i].uv_indices[3];
				const Indigo::Vec2f uv0 = mesh.uv_pairs[uv_i_0 * mesh.num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv1 = mesh.uv_pairs[uv_i_1 * mesh.num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv2 = mesh.uv_pairs[uv_i_2 * mesh.num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv3 = mesh.uv_pairs[uv_i_3 * mesh.num_uv_mappings + uv_set_index] * (float)W;

				const Colour3f col(rng.unitRandom(), rng.unitRandom(), 0.5f + 0.5f * rng.unitRandom());

				Drawing::drawLine(bitmap, col, toVec2f(uv0), toVec2f(uv1));
				Drawing::drawLine(bitmap, col, toVec2f(uv1), toVec2f(uv2));
				Drawing::drawLine(bitmap, col, toVec2f(uv2), toVec2f(uv3));
				Drawing::drawLine(bitmap, col, toVec2f(uv3), toVec2f(uv0));
			}

			PNGDecoder::write(bitmap, "binpacking.png");
		}
		catch(glare::Exception& e)
		{
			conPrint("Warning: " + e.what());
		}
	}

	results.num_patches = patches.size();
	return results;
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "../dll/include/IndigoMesh.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/TaskManager.h"
#include "../graphics/ImageMap.h"
#include "../graphics/PNGDecoder.h"
#include "../graphics/bitmap.h"
#include "../graphics/Drawing.h"
#include "../graphics/BatchedMesh.h"
#include "../maths/PCG32.h"


static UVUnwrapper::Results testUnwrappingWithMesh(Indigo::MeshRef mesh, const Matrix4f& ob_to_world)
{
	try
	{
		StandardPrintOutput print_output;

		conPrint("Initial num UV vec2s: " + toString(mesh->uv_pairs.size()));
		const float normed_margin = 2.f / 1024;
		UVUnwrapper::Results results = UVUnwrapper::build(*mesh, ob_to_world, print_output, normed_margin);
		conPrint("Unwrapped num UV vec2s: " + toString(mesh->uv_pairs.size()));

		// mesh->writeToFile("unwrapped_mesh.igmesh", *mesh, true);

		// Draw triangles on UV map
		{
			PCG32 rng(1);
			const uint32 uv_set_index = mesh->num_uv_mappings - 1;
			// Draw results
			const int W = 1000;
			Bitmap bitmap(W, W, 3, NULL);
			bitmap.zero();
			for(size_t i=0; i<mesh->triangles.size(); ++i)
			{
				const int uv_i_0 = mesh->triangles[i].uv_indices[0];
				const int uv_i_1 = mesh->triangles[i].uv_indices[1];
				const int uv_i_2 = mesh->triangles[i].uv_indices[2];
				const Indigo::Vec2f uv0 = mesh->uv_pairs[uv_i_0 * mesh->num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv1 = mesh->uv_pairs[uv_i_1 * mesh->num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv2 = mesh->uv_pairs[uv_i_2 * mesh->num_uv_mappings + uv_set_index] * (float)W;

				const Colour3f col(rng.unitRandom(), rng.unitRandom(), 0.5f + 0.5f*rng.unitRandom());

				Drawing::drawLine(bitmap, col, toVec2f(uv0), toVec2f(uv1));
				Drawing::drawLine(bitmap, col, toVec2f(uv1), toVec2f(uv2));
				Drawing::drawLine(bitmap, col, toVec2f(uv2), toVec2f(uv0));
			}
			for(size_t i=0; i<mesh->quads.size(); ++i)
			{
				const int uv_i_0 = mesh->quads[i].uv_indices[0];
				const int uv_i_1 = mesh->quads[i].uv_indices[1];
				const int uv_i_2 = mesh->quads[i].uv_indices[2];
				const int uv_i_3 = mesh->quads[i].uv_indices[3];
				const Indigo::Vec2f uv0 =  mesh->uv_pairs[uv_i_0 * mesh->num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv1 =  mesh->uv_pairs[uv_i_1 * mesh->num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv2 =  mesh->uv_pairs[uv_i_2 * mesh->num_uv_mappings + uv_set_index] * (float)W;
				const Indigo::Vec2f uv3 =  mesh->uv_pairs[uv_i_3 * mesh->num_uv_mappings + uv_set_index] * (float)W;

				const Colour3f col(rng.unitRandom(), rng.unitRandom(), 0.5f + 0.5f*rng.unitRandom());

				Drawing::drawLine(bitmap, col, toVec2f(uv0), toVec2f(uv1));
				Drawing::drawLine(bitmap, col, toVec2f(uv1), toVec2f(uv2));
				Drawing::drawLine(bitmap, col, toVec2f(uv2), toVec2f(uv3));
				Drawing::drawLine(bitmap, col, toVec2f(uv3), toVec2f(uv0));
			}

			PNGDecoder::write(bitmap, "uvmapping.png");
		}

		return results;
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
		return UVUnwrapper::Results();
	}
}


static UVUnwrapper::Results testUnwrappingWithMesh(const std::string& path)
{
	// Load mesh
	Indigo::MeshRef indigo_mesh;
	//MeshLoader::loadMesh(path, *mesh, 1.f);

	try
	{
		if(hasExtension(path, "igmesh"))
		{
			try
			{
				indigo_mesh = new Indigo::Mesh();
				Indigo::Mesh::readFromFile(toIndigoString(path), *indigo_mesh);
			}
			catch(Indigo::IndigoException& e)
			{
				throw glare::Exception(toStdString(e.what()));
			}
		}
		else if(hasExtension(path, "bmesh"))
		{
			BatchedMeshRef batched_mesh = BatchedMesh::readFromFile(path, NULL);

			indigo_mesh = batched_mesh->buildIndigoMesh();
		}
		else
			throw glare::Exception("unhandled model format: " + path);
	}
	catch(Indigo::IndigoException& e)
	{
		throw glare::Exception("Error while reading mesh '" + path + ": " + toStdString(e.what()));
	}

	return testUnwrappingWithMesh(indigo_mesh, Matrix4f::identity());
}


void UVUnwrapper::test()
{
	conPrint("UVUnwrapper::test()");

	// Test a single quad, with no existing UVs
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		mesh->addVertex(Indigo::Vec3f(0, 0, 0));
		mesh->addVertex(Indigo::Vec3f(1, 0, 0));
		mesh->addVertex(Indigo::Vec3f(1, 1, 0));
		mesh->addVertex(Indigo::Vec3f(0, 1, 0));

		const uint32 vertex_indices[4] = { 0, 1, 2, 3};
		const uint32 uv_indices[4] = { 0, 0, 0, 0 };
		mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		mesh->endOfModel();
		testUnwrappingWithMesh(mesh, Matrix4f::identity());
	}

	// Test a single quad, with existing UVs
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		mesh->addVertex(Indigo::Vec3f(0, 0, 0));
		mesh->addVertex(Indigo::Vec3f(1, 0, 0));
		mesh->addVertex(Indigo::Vec3f(1, 1, 0));
		mesh->addVertex(Indigo::Vec3f(0, 1, 0));

		mesh->setMaxNumTexcoordSets(1);
		mesh->addUVs(Indigo::Vector<Indigo::Vec2f>(1, Indigo::Vec2f(0, 0)));
		mesh->addUVs(Indigo::Vector<Indigo::Vec2f>(1, Indigo::Vec2f(1, 0)));
		mesh->addUVs(Indigo::Vector<Indigo::Vec2f>(1, Indigo::Vec2f(1, 1)));
		mesh->addUVs(Indigo::Vector<Indigo::Vec2f>(1, Indigo::Vec2f(0, 1)));

		const uint32 vertex_indices[4] = { 0, 1, 2, 3};
		const uint32 uv_indices[4] = { 0, 1, 2, 3 };
		mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		mesh->endOfModel();
		testUnwrappingWithMesh(mesh, Matrix4f::identity());
	}


	// Test adjacent quads
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0, 0));
			mesh->addVertex(Indigo::Vec3f(2, 1, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		testUnwrappingWithMesh(mesh, Matrix4f::identity());
	}


	/*
	Test adjacent quads where only part of one edge is shared

	y
	^
	|________________
	|               |_______________
	|               |               |
	|               |_______________|
	|_______________|                         ----> x
	*/
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 0.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.75, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0.75, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		UVUnwrapper::Results results = testUnwrappingWithMesh(mesh, Matrix4f::identity());
		testAssert(results.num_patches == 1);
	}

	/*
	Test adjacent quads where only part of one edge is shared (along y-axis)

	y
	^
	|  ___________
	|  |         |
	|  |         |
	|  |         |
	|__|_________|__
	|               |
	|               |
	|               |
	|_______________|     ----> x
	*/
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(0.25, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0.75, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0.75, 2, 0));
			mesh->addVertex(Indigo::Vec3f(0.25, 2, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		UVUnwrapper::Results results = testUnwrappingWithMesh(mesh, Matrix4f::identity());
		testAssert(results.num_patches == 1);
	}

	/*
	Test adjacent quads where only part of one edge is shared

	y
	^               ________________
	|_______________|               |
	|               |               |
	|               |               |
	|               |_______________|
	|_______________|                         ----> x
	*/
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 0.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 1.25, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1.25, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		UVUnwrapper::Results results = testUnwrappingWithMesh(mesh, Matrix4f::identity());
		testAssert(results.num_patches == 1);
	}

	/*
	Test multiple adjacent quads where only part of one edge is shared

	y
	^               ________________
	|_______________|      q1       |
	|               |_______________|
	|     q0        |_______________
	|               |      q2       |
	|_______________|_______________|         ----> x
	*/
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 0.75, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.75, 0));
			mesh->addVertex(Indigo::Vec3f(2, 1.25, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1.25, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 0.0, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.0, 0));
			mesh->addVertex(Indigo::Vec3f(2, 0.5, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0.5, 0));

			const uint32 vertex_indices[4] = { 8, 9, 10, 11 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		UVUnwrapper::Results results = testUnwrappingWithMesh(mesh, Matrix4f::identity());
		testAssert(results.num_patches == 1);
	}

	/*
	Test quads with edges that share a line, but are not adjacent

	                __________________
	                |                |
	                |                |
	y               |                |
	^               |________________|
	|_______________
	|               |
	|               |
	|               |
	|_______________|                         ----> x
	*/
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();

		{
			mesh->addVertex(Indigo::Vec3f(0, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 0, 0));
			mesh->addVertex(Indigo::Vec3f(1, 1, 0));
			mesh->addVertex(Indigo::Vec3f(0, 1, 0));

			const uint32 vertex_indices[4] = { 0, 1, 2, 3 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		{
			mesh->addVertex(Indigo::Vec3f(1, 1.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 1.25, 0));
			mesh->addVertex(Indigo::Vec3f(2, 2.25, 0));
			mesh->addVertex(Indigo::Vec3f(1, 2.25, 0));

			const uint32 vertex_indices[4] = { 4, 5, 6, 7 };
			const uint32 uv_indices[4] = { 0, 0, 0, 0 };
			mesh->addQuad(vertex_indices, uv_indices, /*mat index=*/0);
		}
		mesh->endOfModel();
		UVUnwrapper::Results results = testUnwrappingWithMesh(mesh, Matrix4f::identity());
		testAssert(results.num_patches == 2);
	}

	// testUnwrappingWithMesh("C:\\programming\\cv_lightmapper\\cv_baking_meshes\\mesh_5876229332140735852.igmesh"); // CV Parcel #1 (z-up)

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/bmesh/Cube_obj_11907297875084081315.bmesh");
	
	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/quad_and_two_tris.igmesh");

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/cornell_box.igmesh"); // Cornell box

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/teapot.igmesh"); // Teapot

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/person_silhouette.igmesh"); // Solid silhouette from SketchUp

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/tilted_prism.igmesh"); // A tilted prism

	// testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testscenes/cv_baking_meshes/mesh_11825449379137060526.igmesh"); // CV parcel #1
		
	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/cuboid.igmesh"); // A cuboid

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/tall_cuboid.igmesh"); // A tall cuboid

	testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/single_quad.igmesh"); // A single quad

	{
		UVUnwrapper::Results results = testUnwrappingWithMesh(TestUtils::getTestReposDir() + "/testfiles/igmesh/tesellated_quad.igmesh"); // Tesselated quad (400 quads)
		testAssert(results.num_patches == 1);
	}

	conPrint("UVUnwrapper::test() done.");
}


#endif // BUILD_TESTS
