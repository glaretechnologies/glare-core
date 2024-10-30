/*=====================================================================
MeshSimplification.cpp
----------------------
Copyright Glare Technologies Limited 2024
=====================================================================*/
#include "MeshSimplification.h"


#include "BatchedMesh.h"
#include "../meshoptimizer/src/meshoptimizer.h"
#include "../maths/PCG32.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"
#include "../utils/TaskManager.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/HashMapInsertOnly2.h"
#include "../simpleraytracer/raymesh.h"
#include <algorithm>


namespace MeshSimplification
{

// Build new vertex data, that consists of vertices that are actually indexed by indices.
// Copies the (used) vertex data from mesh->vertex_data.
// Also updates simplified_indices to use new indices.
void discardUnusedVertices(const BatchedMesh* mesh, js::Vector<uint32, 16>& simplified_indices, 
	glare::AllocatorVector<uint8, 16>& new_vertex_data_out)
{
	new_vertex_data_out.resizeNoCopy(mesh->vertex_data.size());
	const size_t vertex_size_B = mesh->vertexSize();

	std::vector<unsigned int> new_vert_index(mesh->numVerts(), 0xFFFFFFFFu); // Map from old vert index to new vert index, or -1 if not used

	uint32 new_vertex_data_write_i = 0;
	for(size_t i=0; i<simplified_indices.size(); ++i)
	{
		const uint32 v_i = simplified_indices[i];
		uint32 new_v_i = new_vert_index[v_i];
		if(new_v_i == 0xFFFFFFFFu)
		{
			new_v_i = new_vertex_data_write_i++;
			new_vert_index[v_i] = new_v_i;

			// Copy vert data to new_vertex_data
			std::memcpy(&new_vertex_data_out[new_v_i * vertex_size_B], &mesh->vertex_data[v_i * vertex_size_B], vertex_size_B);
		}

		simplified_indices[i] = new_v_i;
	}
	const uint32 new_num_verts = new_vertex_data_write_i;
	new_vertex_data_out.resize(new_num_verts * vertex_size_B); // Trim down to verts actually used.
}


// target_error represents the error relative to mesh extents that can be tolerated, e.g. 0.01 = 1% deformation
BatchedMeshRef buildSimplifiedMesh(const BatchedMesh& mesh, float target_reduction_ratio, float target_error, bool sloppy)
{
	assert(target_reduction_ratio >= 1);
	//assert(target_error >= 0 && target_error <= 1);
	Timer timer;

	BatchedMeshRef simplified_mesh = new BatchedMesh();
	simplified_mesh->vert_attributes = mesh.vert_attributes;
	simplified_mesh->aabb_os = mesh.aabb_os;

	const size_t vertex_size = mesh.vertexSize(); // In bytes
	const size_t num_verts = mesh.numVerts();
	

	const bool use_attributes = false;

	//------------------------ Build vertex_attributes ------------------------
	float attribute_weights[5];
	int attr_offset = 0;
	size_t vert_data_num_attributes = 0;
	int dest_normal_attr_offset = -1;
	const BatchedMesh::VertAttribute* normal_attr = mesh.findAttribute(BatchedMesh::VertAttribute_Normal);
	if(normal_attr && 
		(normal_attr->component_type == BatchedMesh::ComponentType::ComponentType_PackedNormal || 
		normal_attr->component_type == BatchedMesh::ComponentType::ComponentType_Float))
	{
		attribute_weights[attr_offset] = attribute_weights[attr_offset + 1] = attribute_weights[attr_offset + 2] = 1.f;
		dest_normal_attr_offset = attr_offset;
		attr_offset += 3;
		vert_data_num_attributes += 3;
	}

	// NOTE: might not be a good idea to include UVs in our attributes: see https://github.com/zeux/meshoptimizer/discussions/572#discussioncomment-6358486
	const BatchedMesh::VertAttribute* uv_attr = mesh.findAttribute(BatchedMesh::VertAttribute_UV_0);
	int dest_uv_attr_offset = -1;
	if(uv_attr &&
		(uv_attr->component_type == BatchedMesh::ComponentType::ComponentType_Half || 
		uv_attr->component_type == BatchedMesh::ComponentType::ComponentType_Float))
	{
		attribute_weights[attr_offset] = attribute_weights[attr_offset + 1] = 1.f;
		dest_uv_attr_offset = attr_offset;
		attr_offset += 2;
		vert_data_num_attributes += 2;
	}

	js::Vector<float, 16> vertex_attributes(use_attributes ? (num_verts * vert_data_num_attributes) : 0);
	
	if(use_attributes)
		for(size_t i=0; i<num_verts; ++i)
		{
			// Copy normal
			if(dest_normal_attr_offset >= 0)
			{
				if(normal_attr->component_type == BatchedMesh::ComponentType::ComponentType_PackedNormal)
				{
					uint32 packed_n;
					std::memcpy(&packed_n, &mesh.vertex_data[i * vertex_size + normal_attr->offset_B], sizeof(uint32));
					const Vec4f unpacked_N = batchedMeshUnpackNormal(packed_n);
					std::memcpy(&vertex_attributes[i * vert_data_num_attributes + dest_normal_attr_offset], &unpacked_N.x, sizeof(float)*3);
				}
				else if(normal_attr->component_type == BatchedMesh::ComponentType::ComponentType_Float)
				{
					std::memcpy(&vertex_attributes[i * vert_data_num_attributes + dest_normal_attr_offset], 
						&mesh.vertex_data[i * vertex_size + normal_attr->offset_B], sizeof(float)*3);
				}
				else
				{
					runtimeCheck(false);
				}
			}
			// Copy UV0
			if(dest_uv_attr_offset >= 0)
			{
				if(uv_attr->component_type == BatchedMesh::ComponentType::ComponentType_Half)
				{
					std::memcpy(&vertex_attributes[i * vert_data_num_attributes + dest_uv_attr_offset], 
						&mesh.vertex_data[i * vertex_size + uv_attr->offset_B], 2*2);
				}
				else if(uv_attr->component_type == BatchedMesh::ComponentType::ComponentType_Float)
				{
					std::memcpy(&vertex_attributes[i * vert_data_num_attributes + dest_uv_attr_offset], 
						&mesh.vertex_data[i * vertex_size + uv_attr->offset_B], sizeof(float)*2);
				}
				else
				{
					runtimeCheck(false);
				}
			}
		}


	// There are a few options for handling meshes with multiple batches with different materials.  
	// See https://github.com/zeux/meshoptimizer/discussions/690#discussioncomment-9452590

#if 0 //-------------------------- Do simplification for all batches at once: --------------------------

	std::vector<BatchedMesh::IndicesBatch> new_batches;

	const BatchedMesh::VertAttribute& pos_attr = mesh.getAttribute(BatchedMesh::VertAttribute_Position);
	if(pos_attr.component_type != BatchedMesh::ComponentType_Float)
		throw glare::Exception("Mesh simplification needs float position type.");

	const size_t num_indices = mesh.numIndices();

	js::Vector<uint32, 16> temp_indices(num_indices);
	js::Vector<uint32, 16> simplified_indices(num_indices);
	for(size_t i=0; i<num_indices; ++i)
		temp_indices[i] = mesh.getIndexAsUInt32(i);

	// Work out the material assigned to each vertex.
	js::Vector<int, 16> vert_mat_index(mesh.numVerts(), 0);
	js::Vector<int, 16> new_vert_mat_index(mesh.numVerts(), 0);

	// Iterate over batches of vertex indices, 'splat' the batch material to the corresponding vertices in vert_mat_index.
	for(size_t b=0; b<mesh.batches.size(); ++b)
	{
		const BatchedMesh::IndicesBatch& batch = mesh.batches[b];
		for(uint32 q=batch.indices_start; q<batch.indices_start + batch.num_indices; ++q)
		{
			const uint32 vert_index = temp_indices[q]; // Index of the vertex in mesh
			vert_mat_index[vert_index] = batch.material_index;
		}
	}

	if((num_indices % 3) != 0)
		throw glare::Exception("Mesh simplification requires batch num indices to be a multiple of 3.");

	const size_t target_index_count = (size_t)(num_indices / target_reduction_ratio);
	float result_error = 0;

	size_t res_num_indices;
	if(sloppy)
	{
		res_num_indices = meshopt_simplifySloppy(/*destination=*/simplified_indices.data(), 
			temp_indices.data(), temp_indices.size(),
			(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
			mesh.numVerts(), // vert count
			mesh.vertexSize(), // vert stride
			target_index_count,
			target_error,
			&result_error
		);
	}
	else
	{
		if(!use_attributes)
		{
			res_num_indices = meshopt_simplify(/*destination=*/simplified_indices.data(), temp_indices.data(), temp_indices.size(),
				(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
				mesh.numVerts(), // vert count
				mesh.vertexSize(), // vert stride
				target_index_count,
				target_error,
				/*meshopt_SimplifySparse | */meshopt_SimplifyErrorAbsolute, // options
				&result_error
			);
		}
		else
		{
			res_num_indices = meshopt_simplifyWithAttributes(/*destination=*/simplified_indices.data(), 
				temp_indices.data(), temp_indices.size(),
				(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
				mesh.numVerts(), // vert count
				mesh.vertexSize(), // vert stride
				vertex_attributes.data(),
				vert_data_num_attributes * sizeof(float), // vert attributes stride
				attribute_weights,
				vert_data_num_attributes, // attribute count
				NULL, // vertex lock
				target_index_count,
				target_error,
				/*meshopt_SimplifySparse | */meshopt_SimplifyErrorAbsolute, // options
				&result_error
			);
		}
	}

	assert(res_num_indices <= simplified_indices.size());
	simplified_indices.resize(res_num_indices);

	//js::Vector<uint8, 16> vertex_data;
	//std::vector<uint32> indices;
	/*meshopt_optimizeVertexFetch(
		vertex_data.data(), // destination - resulting vertex buffer
		simplified_indices.data(), // indices is used both as an input and as an output index buffer
		simplified_indices.size(), // index_count 
		vertex_data.data(), // vertices (source vertices?)
		num_vertices, // vertex count
		vertex_size); // vertex_size
	*/

	glare::AllocatorVector<uint8, 16> new_vertex_data;
	discardUnusedVertices(&mesh, simplified_indices, new_vertex_data);

	// Sort triangles (indices) by material
	struct IndexAndMat
	{
		uint32 index;
		uint32 mat;
	};
	struct MatLessThan
	{
		bool operator () (const IndexAndMat& a, const IndexAndMat& b) { return a.mat < b.mat; }
	};
	std::vector<IndexAndMat> v(res_num_indices);
	for(size_t i=0; i<res_num_indices; i += 3)
	{
		const uint32 mat = new_vert_mat_index[simplified_indices[i]];
		v[i  ].index = simplified_indices[i];
		v[i+1].index = simplified_indices[i+1];
		v[i+2].index = simplified_indices[i+2];
		v[i    ].mat = mat;
		v[i + 1].mat = mat;
		v[i + 2].mat = mat;
	}

	std::stable_sort(v.begin(), v.end(), MatLessThan());

	uint32 cur_begin = 0;
	uint32 cur_mat = 100000000;
	for(size_t i=0; i<v.size(); ++i)
	{
		if(v[i].mat != cur_mat)
		{
			if(i > cur_begin)
			{
				BatchedMesh::IndicesBatch new_batch;
				new_batch.indices_start = cur_begin;
				new_batch.material_index = cur_mat;
				new_batch.num_indices = (uint32)(i - cur_begin);
				new_batches.push_back(new_batch);
			}
			cur_begin = (uint32)i;
			cur_mat = v[i].mat;
		}
	}
	if(v.size() > cur_begin)
	{
		BatchedMesh::IndicesBatch new_batch;
		new_batch.indices_start = cur_begin;
		new_batch.material_index = cur_mat;
		new_batch.num_indices = (uint32)(v.size() - cur_begin);
		new_batches.push_back(new_batch);
	}

	// Copy back to simplified_indices
	simplified_indices.resize(res_num_indices);
	for(size_t i=0; i<res_num_indices; ++i)
		simplified_indices[i] = v[i].index;

	const size_t new_num_verts = new_vertex_data.size() / mesh.vertexSize();

#ifndef NDEBUG
	// Check all indices are in-bounds
	for(size_t i=0; i<simplified_indices.size(); ++i)
	{
		assert(simplified_indices[i] < new_num_verts);
	}
#endif

	// Copy new index data
	simplified_mesh->setIndexDataFromIndices(simplified_indices, new_num_verts);

	// Copy new vertex data
	simplified_mesh->vertex_data.takeFrom(new_vertex_data);

	simplified_mesh->batches = new_batches;

	simplified_mesh->animation_data = mesh.animation_data;


#else //-------------------------------------- Else do simplification for each batch separately. -----------------------------------------

	js::Vector<uint32, 16> new_indices;
	new_indices.reserve(mesh.numIndices());

	std::vector<BatchedMesh::IndicesBatch> new_batches;

	const BatchedMesh::VertAttribute& pos_attr = mesh.getAttribute(BatchedMesh::VertAttribute_Position);
	if(pos_attr.component_type != BatchedMesh::ComponentType_Float)
		throw glare::Exception("Mesh simplification needs float position type.");

	js::Vector<uint32, 16> temp_batch_indices;
	js::Vector<uint32, 16> simplified_indices;
	for(size_t b=0; b<mesh.batches.size(); ++b)
	{
		const BatchedMesh::IndicesBatch& batch = mesh.batches[b];

		if((batch.num_indices % 3) != 0)
			throw glare::Exception("Mesh simplification requires batch num indices to be a multiple of 3.");

		// Build vector of uint32 indices for this batch
		temp_batch_indices.resizeNoCopy(batch.num_indices);
		for(size_t i=0; i<(size_t)batch.num_indices; ++i)
			temp_batch_indices[i] = mesh.getIndexAsUInt32(batch.indices_start + i);

		const size_t target_index_count = (size_t)(temp_batch_indices.size() / target_reduction_ratio);
		float result_error = 0;

		simplified_indices.resizeNoCopy(temp_batch_indices.size());
		size_t res_num_indices;
		if(sloppy)
		{
			res_num_indices = meshopt_simplifySloppy(/*destination=*/simplified_indices.data(), temp_batch_indices.data(), temp_batch_indices.size(),
				(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
				mesh.numVerts(), // vert count
				mesh.vertexSize(), // vert stride
				target_index_count,
				target_error,
				&result_error
			);
		}
		else
		{
			unsigned int options = meshopt_SimplifyErrorAbsolute;
			if(mesh.batches.size() > 1)
				options |= meshopt_SimplifySparse;

			if(!use_attributes)
			{
				res_num_indices = meshopt_simplify(/*destination=*/simplified_indices.data(), temp_batch_indices.data(), temp_batch_indices.size(),
					(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
					mesh.numVerts(), // vert count
					mesh.vertexSize(), // vert stride
					target_index_count,
					target_error,
					options,
					&result_error
				);
			}
			else
			{
				res_num_indices = meshopt_simplifyWithAttributes(/*destination=*/simplified_indices.data(), 
					temp_batch_indices.data(), temp_batch_indices.size(),
					(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
					mesh.numVerts(), // vert count
					mesh.vertexSize(), // vert stride
					vertex_attributes.data(),
					vert_data_num_attributes * sizeof(float), // vert attributes stride
					attribute_weights,
					vert_data_num_attributes, // attribute count
					NULL, // vertex lock
					target_index_count,
					target_error,
					options,
					&result_error
				);
			}
		}

		assert(res_num_indices <= simplified_indices.size());

		//js::Vector<uint8, 16> vertex_data;
		//std::vector<uint32> indices;
		/*meshopt_optimizeVertexFetch(
			vertex_data.data(), // destination - resulting vertex buffer
			simplified_indices.data(), // indices is used both as an input and as an output index buffer
			simplified_indices.size(), // index_count 
			vertex_data.data(), // vertices (source vertices?)
			num_vertices, // vertex count
			vertex_size); // vertex_size
		*/

		// Add simplified indices to new_indices
		const size_t write_i = new_indices.size();
		new_indices.resize(write_i + res_num_indices);
		for(size_t i=0; i<res_num_indices; ++i)
			new_indices[write_i + i] = simplified_indices[i];

		BatchedMesh::IndicesBatch new_batch;
		new_batch.indices_start = (uint32)write_i;
		new_batch.material_index = batch.material_index;
		new_batch.num_indices = (uint32)res_num_indices;
		new_batches.push_back(new_batch);
	}

	glare::AllocatorVector<uint8, 16> new_vertex_data;
	discardUnusedVertices(&mesh, new_indices, new_vertex_data);

	const size_t new_num_verts = new_vertex_data.size() / mesh.vertexSize();

#ifndef NDEBUG
	// Check all indices are in-bounds
	for(size_t i=0; i<new_indices.size(); ++i)
	{
		assert(new_indices[i] < new_num_verts);
	}
#endif

	// Copy new index data
	simplified_mesh->setIndexDataFromIndices(new_indices, new_num_verts);

	// Copy new vertex data
	simplified_mesh->vertex_data.takeFrom(new_vertex_data);

	simplified_mesh->batches = new_batches;

	simplified_mesh->animation_data = mesh.animation_data;

#endif //-------------------------------------- End else do simplification for each batch separately. -----------------------------------------

	if(false)
	{
		if(sloppy)
			conPrint("-------------- sloppy simplified mesh ------------");
		else
			conPrint("-------------- simplified mesh ------------");
		conPrint("Original num indices: " + toString(mesh.numIndices()));
		conPrint("new num indices:      " + toString(simplified_mesh->numIndices()));
		conPrint("reduction ratio:      " + toString((float)mesh.numIndices() / simplified_mesh->numIndices()));
		conPrint("Original num verts:   " + toString(mesh.numVerts()));
		conPrint("new num verts:        " + toString(simplified_mesh->numVerts()));
		conPrint("reduction ratio:      " + toString((float)mesh.numVerts() / simplified_mesh->numVerts()));

		conPrint("Simplification took " + timer.elapsedStringMSWIthNSigFigs(4));
	}

	return simplified_mesh;
}


struct EdgeKey
{
	EdgeKey(){}
	EdgeKey(const Vec3f& a, const Vec3f& b) : start(a), end(b) {}

	Vec3f start;
	Vec3f end;

	inline bool operator == (const EdgeKey& other) const { return start == other.start && end == other.end; }
	inline bool operator != (const EdgeKey& other) const { return start != other.start || end != other.end; }
};

class EdgeKeyHash
{
public:
	inline size_t operator()(const EdgeKey& v) const
	{
		return hashBytes((const uint8*)&v, sizeof(EdgeKey));
	}
};

struct EdgeInfo
{
	EdgeInfo() : processed(false) {}

	SmallVector<uint32, 2> adjacent_polys;
	bool processed;
};

struct Triangle
{
	Triangle() : keep(false), processed(false) {}
	int edge_i[3];
	bool keep;
	bool processed;
};



static const uint32_t mod3_table[] = { 0, 1, 2, 0, 1, 2 };

inline static uint32 mod3(uint32 x)
{
	return mod3_table[x];
}


BatchedMeshRef removeSmallComponents(const BatchedMeshRef mesh_, float target_error)
{
	Timer timer;

	const BatchedMesh& mesh = *mesh_;

	const BatchedMesh::VertAttribute& pos_attr = mesh.getAttribute(BatchedMesh::VertAttribute_Position);
	if(pos_attr.component_type != BatchedMesh::ComponentType_Float)
		throw glare::Exception("Mesh simplification needs float position type.");

	// Build adjacency info, build tris vector.
	
	const size_t vertex_size_B = mesh.vertexSize(); // In bytes
	const size_t triangles_in_size = mesh.numIndices() / 3;

	const Vec3f inf_vec(std::numeric_limits<float>::infinity());
	HashMapInsertOnly2<EdgeKey, int, EdgeKeyHash> edge_indices_map(EdgeKey(inf_vec, inf_vec),
		triangles_in_size * 2 // expected_num_items
	);

	std::vector<EdgeInfo> edges;
	edges.reserve(triangles_in_size * 2);

	std::vector<Triangle> tris(triangles_in_size);

	for(size_t t = 0; t < triangles_in_size; ++t) // For each triangle
	{
		const size_t v0_i = t * 3; // index of v0
		for(unsigned int z = 0; z < 3; ++z) // For each edge:
		{
			const size_t vz  = mesh.getIndexAsUInt32(v0_i + z);
			const size_t vz1 = mesh.getIndexAsUInt32(v0_i + mod3(z + 1));

			// Get vertex positions at either end of the edge.
			Vec3f vz_pos, vz1_pos;
			std::memcpy(&vz_pos,  &mesh.vertex_data[pos_attr.offset_B + vertex_size_B * vz ], sizeof(Vec3f));
			std::memcpy(&vz1_pos, &mesh.vertex_data[pos_attr.offset_B + vertex_size_B * vz1], sizeof(Vec3f));

			EdgeKey edge_key;
			if(vz_pos < vz1_pos)
			{
				edge_key.start = vz_pos;
				edge_key.end = vz1_pos;
			}
			else
			{
				edge_key.start = vz1_pos;
				edge_key.end = vz_pos;
			}

			auto result = edge_indices_map.find(edge_key); // Lookup edge
			if(result == edge_indices_map.end()) // If edge not added yet:
			{
				const size_t edge_i = edges.size();
				edges.resize(edge_i + 1);

				edges[edge_i].adjacent_polys.push_back((uint32)t); // Add this tri to the list of polys adjacent to edge.
					
				edge_indices_map.insert(std::make_pair(edge_key, (int)edge_i)); // Add edge index to map

				tris[t].edge_i[z] = (int)edge_i;
			}
			else
			{
				edges[result->second].adjacent_polys.push_back((uint32)t); // Add this tri to the list of polys adjacent to edge.

				tris[t].edge_i[z] = (int)result->second;
			}
		}
	}
	
	std::vector<size_t> patch_tris;
	patch_tris.reserve(triangles_in_size);

	std::vector<size_t> patch_tris_to_process;
	bool all_components_kept = true;

	for(size_t initial_i=0; initial_i<triangles_in_size; ++initial_i)
	{
		if(!tris[initial_i].processed)
		{
			// Start a patch
			patch_tris.clear();

			js::AABBox component_aabb = js::AABBox::emptyAABBox();

			tris[initial_i].processed = true;
			patch_tris_to_process.clear();

			patch_tris_to_process.push_back(initial_i);

			while(!patch_tris_to_process.empty())
			{
				const size_t tri_i = patch_tris_to_process.back();
				Triangle& tri = tris[tri_i];
				patch_tris_to_process.pop_back();

				patch_tris.push_back(tri_i);

				for(int i=0; i<3; ++i)
				{
					const uint32 vi = mesh.getIndexAsUInt32(tri_i * 3 + i);
					Vec3f v_pos;
					std::memcpy(&v_pos, &mesh.vertex_data[pos_attr.offset_B + vertex_size_B * vi], sizeof(Vec3f));

					component_aabb.enlargeToHoldPoint(v_pos.toVec4fPoint());
				}

				// Add adjacent polygons to poly_i to polys_to_process, if applicable
				for(int e=0; e<3; ++e)
				{
					// Check for adjacent polygons where the other polygon shares the edge with this polygon.
					const int edge_i = tri.edge_i[e];
					EdgeInfo& edge = edges[edge_i];
					if(!edge.processed) // edge.adjacent_polys can be large, so avoid processing more than once.
					{
						for(size_t q=0; q<edge.adjacent_polys.size(); ++q)
						{
							const size_t adj_tri_i = edge.adjacent_polys[q];
							if((adj_tri_i == tri_i) || tris[adj_tri_i].processed)
								continue;

							patch_tris_to_process.push_back(adj_tri_i); // Add the adjacent poly to set of polys to add to patch.

							tris[adj_tri_i].processed = true;
						}
						edge.processed = true;
					}
				}
			}

			if(component_aabb.longestLength() >= target_error)
			{
				for(size_t i=0; i<patch_tris.size(); ++i)
					tris[patch_tris[i]].keep = true;
			}
			else
				all_components_kept = false;
		}
	}

	if(all_components_kept)
	{
		if(false)
		{
			conPrint("-------------- removeSmallComponents ------------");
			conPrint("No components removed.  Elapsed: " + timer.elapsedStringMSWIthNSigFigs(4));
		}

		return mesh_;
	}
	else
	{
		BatchedMeshRef simplified_mesh = new BatchedMesh();
		simplified_mesh->vert_attributes = mesh.vert_attributes;
		simplified_mesh->aabb_os = mesh.aabb_os;

		js::Vector<uint32, 16> simplified_indices;
		simplified_indices.reserve(mesh.numIndices());

		for(size_t b=0; b<mesh.batches.size(); ++b)
		{
			const BatchedMesh::IndicesBatch& batch = mesh.batches[b];

			BatchedMesh::IndicesBatch new_batch;
			new_batch.indices_start = (uint32)simplified_indices.size();

			assert(batch.indices_start % 3 == 0);
			assert(batch.num_indices   % 3 == 0);
			for(uint32 i=batch.indices_start; i != batch.indices_start + batch.num_indices; i += 3)
			{
				const uint32 tri_i = i / 3;
				if(tris[tri_i].keep)
				{
					simplified_indices.push_back(mesh.getIndexAsUInt32(i));
					simplified_indices.push_back(mesh.getIndexAsUInt32(i+1));
					simplified_indices.push_back(mesh.getIndexAsUInt32(i+2));
				}
			}

			new_batch.num_indices = (uint32)simplified_indices.size() - new_batch.indices_start;
			new_batch.material_index = batch.material_index;
			if(new_batch.num_indices > 0)
				simplified_mesh->batches.push_back(new_batch);
		}

		glare::AllocatorVector<uint8, 16> new_vertex_data;
		discardUnusedVertices(&mesh ,simplified_indices, new_vertex_data);
		
		const size_t new_num_verts = new_vertex_data.size() / mesh.vertexSize();

		// Copy new index data
		simplified_mesh->setIndexDataFromIndices(simplified_indices, new_num_verts);

		// Copy new vertex data
		simplified_mesh->vertex_data.takeFrom(new_vertex_data);

		simplified_mesh->animation_data = mesh.animation_data;

		if(false)
		{
			conPrint("-------------- removeSmallComponents ------------");
			conPrint("Original num indices: " + toString(mesh.numIndices()));
			conPrint("new num indices:      " + toString(simplified_mesh->numIndices()));
			conPrint("reduction ratio:      " + toString((float)mesh.numIndices() / simplified_mesh->numIndices()));
			conPrint("Original num verts:   " + toString(mesh.numVerts()));
			conPrint("new num verts:        " + toString(simplified_mesh->numVerts()));
			conPrint("reduction ratio:      " + toString((float)mesh.numVerts() / simplified_mesh->numVerts()));

			conPrint("Simplification took " + timer.elapsedStringMSWIthNSigFigs(4));
		}

		return simplified_mesh;
	}
}


struct ShootRaysTask : public glare::Task
{
	GLARE_ALIGNED_16_NEW_DELETE

	virtual void run(size_t /*thread_index*/)
	{
		// Get vectors orthogonal to direction
		const Matrix4f mat = Matrix4f::constructFromVectorStatic(dir);

		// Get extents of mesh along orthogonal vectors, giving a rectangle
		const Vec4f basis_i = mat.getColumn(0);
		const Vec4f basis_j = mat.getColumn(1);

		js::AABBox dir_basis_aabb = mesh->aabb_os.transformedAABB(mat.getTranspose());

		const float min_i = dir_basis_aabb.min_[0];
		const float min_j = dir_basis_aabb.min_[1];
		const float min_d = dir_basis_aabb.min_[2];
		const float max_i = dir_basis_aabb.max_[0];
		const float max_j = dir_basis_aabb.max_[1];

		while(1)
		{
			const int num_rays_per_atomic_access = 16;
			const int next_i = (int)((*next_ray_i) += num_rays_per_atomic_access);
			if(next_i >= res * res)
				return;

			assert(next_i + num_rays_per_atomic_access <= res * res);
			for(int i=next_i; i<next_i + num_rays_per_atomic_access; ++i)
			{
				const int px = i % res;
				const int py = i / res;

				const float fx = (float)px / (res - 1);
				const float fy = (float)py / (res - 1);

				// Sample point in rectangle
				const Vec4f p = Vec4f(0,0,0,1) +
					dir * (min_d - 10.f) +
					basis_i * (min_i + fx * (max_i - min_i)) +
					basis_j * (min_j + fy * (max_j - min_j));

				HitInfo hitinfo;
				const Ray ray(p, dir, 0.f, 1.e30f);
				//rays.push_back(ray);
				const double dist = raymesh->traceRay(ray, hitinfo);
				if(dist > 0)
				{
					(*num_tri_hits)[hitinfo.sub_elem_index]++;
					//hit_positions.push_back(ray.pointf((float)dist));
				}
			}
		}
	}


	Vec4f dir;
	const BatchedMesh* mesh;
	const RayMesh* raymesh;
	int res;
	glare::AtomicInt* next_ray_i;
	std::vector<glare::AtomicInt>* num_tri_hits;
};


BatchedMeshRef removeInvisibleTriangles(const BatchedMeshRef mesh, std::vector<uint32>& index_map_out, glare::TaskManager& task_manager)
{
#if RAYMESH_TRACING_SUPPORT
	RayMesh raymesh("raymesh", false);
	raymesh.fromBatchedMesh(*mesh);

	Geometry::BuildOptions options;
	options.compute_is_planar = false;
	DummyShouldCancelCallback should_cancel_callback;
	StandardPrintOutput print_output;
	raymesh.build(options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

	std::vector<glare::AtomicInt> num_tri_hits(mesh->numIndices() / 3);

	Timer timer;

	std::vector<Ray> rays;
	std::vector<Vec4f> hit_positions;

	
	glare::TaskGroupRef task_group = new glare::TaskGroup();
	for(size_t i=0; i<myMax<size_t>(1, task_manager.getConcurrency()); ++i)
		task_group->tasks.push_back(new ShootRaysTask());

	const int num_dirs = 32;
	const int res = 1024;
	for(int i=0; i<num_dirs; ++i)
	{
		// Pick a direction using the Fibonacci lattice:  (https://extremelearning.com.au/how-to-evenly-distribute-points-on-a-sphere-more-effectively-than-the-canonical-fibonacci-lattice/#more-3069)
		const float cos_phi = 1 - 2 * (i + 0.5f) / num_dirs;
		const float sin_phi = std::sqrt(1 - Maths::square(cos_phi)); // sin(phi)^2 + cos(phi)^2 = 1   =>   sin(phi)^2 = 1 - cos(phi)^2   =>    sin(phi) = sqrt(1 - cos(phi)^2)
		const float golden_ratio = (float)((1.0 + std::sqrt(5.0)) / 2.0);
		const float theta = 2.f * Maths::pi<float>() * i / golden_ratio;

		const Vec4f dir(std::cos(theta) * sin_phi, std::sin(theta) * sin_phi, cos_phi, 0);

		glare::AtomicInt next_ray_i(0);
		for(size_t t=0; t<task_group->tasks.size(); ++t)
		{
			ShootRaysTask* task = task_group->tasks[t].downcastToPtr<ShootRaysTask>();
			task->dir = dir;
			task->mesh = mesh.ptr();
			task->raymesh = &raymesh;
			task->res = res;
			task->next_ray_i = &next_ray_i;
			task->num_tri_hits = &num_tri_hits;
		}

		task_manager.runTaskGroup(task_group);
	}

	//for(size_t i=0; i<num_tri_hits.size(); ++i)
	//	conPrint("tri " + toString(i) + ": " + toString(num_tri_hits[i]) + " hits");

	const int num_rays = num_dirs * res * res;
	const double ray_shooting_time_elapsed = timer.elapsed();
	


	// Remove all triangles from mesh that didn't get hit.

	BatchedMeshRef simplified_mesh = new BatchedMesh();
	simplified_mesh->vert_attributes = mesh->vert_attributes;
	simplified_mesh->aabb_os = mesh->aabb_os;

	js::Vector<uint32, 16> simplified_indices;
	simplified_indices.reserve(mesh->numIndices());

	index_map_out.resize(mesh->numIndices() + 1); // Plus one so we can remap end indices (one past last element).

	/*
	the new index = the number of kept vert indices preceding.

	0 1 2 3 4 5 6 7 8 9 10 11
	a b c d e f g h i j k  l

	0 1 2 3 4
	b d e h j
	
	map:
	map[0] = 0    // a     preceding kept:   {}
	map[1] = 0    // b                       {}
	map[2] = 1    // c                       {b}
	map[3] = 1    // d                       {b}
	map[4] = 2    // e                       {b, d}
	map[5] = 3    // f                       {b, d, e}
	map[6] = 3    // g                       {b, d, e}
	map[7] = 3    // h                       {b, d, e}
	map[8] = 4    // i                       {b, d, e, h}
	map[9] = 4    // j                       {b, d, e, h}
	map[10] = 5   // k                       {b, d, e, h, j}
	map[11] = 5   // l                       {b, d, e, h, j}
	map[12] = 5   // end                     {b, d, e, h, j}
	*/

	uint32 last_new_index = 0;
	for(size_t b=0; b<mesh->batches.size(); ++b)
	{
		const BatchedMesh::IndicesBatch& batch = mesh->batches[b];

		BatchedMesh::IndicesBatch new_batch;
		new_batch.indices_start = (uint32)simplified_indices.size();

		assert(batch.indices_start % 3 == 0);
		assert(batch.num_indices   % 3 == 0);
		for(uint32 i=batch.indices_start; i != batch.indices_start + batch.num_indices; i += 3)
		{
			const uint32 tri_i = i / 3;
			if(num_tri_hits[tri_i] > 0) // If keep tri:
			{
				simplified_indices.push_back(mesh->getIndexAsUInt32(i));
				index_map_out[i] = last_new_index++;

				simplified_indices.push_back(mesh->getIndexAsUInt32(i+1));
				index_map_out[i+1] = last_new_index++;

				simplified_indices.push_back(mesh->getIndexAsUInt32(i+2));
				index_map_out[i+2] = last_new_index++;
			}
			else
			{
				index_map_out[i]   = last_new_index;
				index_map_out[i+1] = last_new_index;
				index_map_out[i+2] = last_new_index;
			}
		}

		new_batch.num_indices = (uint32)simplified_indices.size() - new_batch.indices_start;
		new_batch.material_index = batch.material_index;
		if(new_batch.num_indices > 0)
			simplified_mesh->batches.push_back(new_batch);
	}

	index_map_out.back() = last_new_index;


	const size_t vertex_size_B = mesh->vertexSize();

	glare::AllocatorVector<uint8, 16> new_vertex_data;

	discardUnusedVertices(mesh.ptr(), simplified_indices, new_vertex_data);

	//TEMP: Add hit visualisation
	if(false)
	{
		BatchedMesh::IndicesBatch batch;
		batch.indices_start = (uint32)simplified_indices.size();

		for(size_t i=0; i<hit_positions.size(); ++i)
		{
			const Vec4f hitpos = hit_positions[i];
			uint32 v = (uint32)(new_vertex_data.size() / vertex_size_B);
			simplified_indices.push_back(v);
			simplified_indices.push_back(v+1);
			simplified_indices.push_back(v+2);

			// Add 3 vertices
			new_vertex_data.resize(new_vertex_data.size() + vertex_size_B * 3);

			Vec3f v0(hitpos);
			Vec3f v1(hitpos - Vec4f(0.3f, 0, 0.f, 0));
			Vec3f v2(hitpos + Vec4f(0, 0, 0.05f, 0));

			std::memcpy(&new_vertex_data[v       * vertex_size_B], &v0, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 1) * vertex_size_B], &v1, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 2) * vertex_size_B], &v2, sizeof(Vec3f));

			Vec3f normal(0,0,1);
			const size_t normal_offset_B = 12;
			std::memcpy(&new_vertex_data[v       * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 1) * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 2) * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
		}

		batch.num_indices = (uint32)simplified_indices.size() - batch.indices_start;
		batch.material_index = 0;
		simplified_mesh->batches.push_back(batch);
	}
	//TEMP: Add ray visualisation
	if(false)
	{
		BatchedMesh::IndicesBatch batch;
		batch.indices_start = (uint32)simplified_indices.size();

		for(size_t i=0; i<rays.size(); ++i)
		{
			const Ray& ray = rays[i];
			uint32 v = (uint32)(new_vertex_data.size() / vertex_size_B);
			simplified_indices.push_back(v);
			simplified_indices.push_back(v+1);
			simplified_indices.push_back(v+2);

			// Add 3 vertices
			new_vertex_data.resize(new_vertex_data.size() + vertex_size_B * 3);

			Vec3f v0(ray.startPos());
			Vec3f v1(ray.startPos() - Vec4f(0, 0, 0.2f, 0));
			Vec3f v2(ray.startPos() + ray.unitDir() * 5.f);

			std::memcpy(&new_vertex_data[v       * vertex_size_B], &v0, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 1) * vertex_size_B], &v1, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 2) * vertex_size_B], &v2, sizeof(Vec3f));

			Vec3f normal(0,0,1);
			const size_t normal_offset_B = 12;
			std::memcpy(&new_vertex_data[v       * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 1) * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
			std::memcpy(&new_vertex_data[(v + 2) * vertex_size_B + normal_offset_B], &normal, sizeof(Vec3f));
		}

		batch.num_indices = (uint32)simplified_indices.size() - batch.indices_start;
		batch.material_index = 0;
		simplified_mesh->batches.push_back(batch);
	}

	//for(size_t i=0; i<simplified_indices.size(); ++i)
	//	conPrint("simplified_indices[" + toString(i) + ": " + toString(simplified_indices[i]));

	// Copy new index data
	simplified_mesh->setIndexDataFromIndices(simplified_indices, new_vertex_data.size() / vertex_size_B);

	// Copy new vertex data
	simplified_mesh->vertex_data.takeFrom(new_vertex_data);

	simplified_mesh->animation_data = mesh->animation_data;

	if(false)
	{
		conPrint("-------------- removeInvisibleTriangles ------------");
		conPrint("Shot " + toString(num_rays) + " rays in " + doubleToStringNSigFigs(ray_shooting_time_elapsed, 4) + " s (" + doubleToStringNSigFigs(num_rays / ray_shooting_time_elapsed * 1.0e-6, 4) + " M rays/sec)");
		conPrint("Original num indices: " + toString(mesh->numIndices()));
		conPrint("new num indices:      " + toString(simplified_mesh->numIndices()));
		conPrint("reduction ratio:      " + toString((float)mesh->numIndices() / simplified_mesh->numIndices()));
		conPrint("Original num verts:   " + toString(mesh->numVerts()));
		conPrint("new num verts:        " + toString(simplified_mesh->numVerts()));
		conPrint("reduction ratio:      " + toString((float)mesh->numVerts() / simplified_mesh->numVerts()));

		conPrint("Simplification took " + timer.elapsedStringMSWIthNSigFigs(4));
	}


	return simplified_mesh;
#else // RAYMESH_TRACING_SUPPORT
	throw glare::Exception("unsupported.");
#endif
}


} // end namespace MeshSimplification


#if BUILD_TESTS


#include "BatchedMesh.h"
#include "FormatDecoderGLTF.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"


namespace MeshSimplification
{


void testOnBMesh(const std::string& src_path)
{
	try
	{
		BatchedMeshRef batched_mesh = BatchedMesh::readFromFile(src_path, /*mem allocator=*/NULL);

		BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, 10.f, 0.02f, /*sloppy=*/false);

		const std::string dest_path = eatExtension(FileUtils::getFilename(src_path)) + "_lod1.bmesh";

		simplified_mesh->writeToFile(dest_path);

		const size_t src_size = FileUtils::getFileSize(src_path);
		const size_t new_size = FileUtils::getFileSize(dest_path);
		conPrint("src size: " + toString(src_size) + " B");
		conPrint("simplified size: " + toString(new_size) + " B");
		conPrint("Reduction ratio: " + toString((float)src_size / new_size));
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void buildLODVersions(const std::string& src_path)
{
	try
	{
		conPrint("===========================================");
		conPrint("Creating LOD models for " + src_path + "...");
		BatchedMeshRef batched_mesh = BatchedMesh::readFromFile(src_path, /*mem allocator=*/NULL);

		// Create lod1
		{
			BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/false);

			const std::string dest_path = removeDotAndExtension(src_path) + "_lod1.bmesh";

			simplified_mesh->writeToFile(dest_path);

			const size_t src_size = FileUtils::getFileSize(src_path);
			const size_t new_size = FileUtils::getFileSize(dest_path);
			conPrint("src size: " + toString(src_size) + " B");
			conPrint("simplified size: " + toString(new_size) + " B");
			conPrint("Reduction ratio: " + toString((float)src_size / new_size));
		}

		// Create lod2
		{
			BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, /*target_reduction_ratio=*/100.f, /*target_error=*/0.08f, /*sloppy=*/true);

			const std::string dest_path = removeDotAndExtension(src_path) + "_lod2.bmesh";

			simplified_mesh->writeToFile(dest_path);

			const size_t src_size = FileUtils::getFileSize(src_path);
			const size_t new_size = FileUtils::getFileSize(dest_path);
			conPrint("src size: " + toString(src_size) + " B");
			conPrint("simplified size: " + toString(new_size) + " B");
			conPrint("Reduction ratio: " + toString((float)src_size / new_size));
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#if 0
// Command line:
// C:\fuzz_corpus\mesh_simplification

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	Clock::init();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		BatchedMesh batched_mesh;
		BatchedMesh::readFromData(data, size, batched_mesh);

		batched_mesh.checkValidAndSanitiseMesh();

		// From generateLODModel() in LodGeneration.cpp:

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/false);

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/true);

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/100.f, /*target_error=*/0.08f, /*sloppy=*/true);
	}
	catch(glare::Exception& )
	{
	}
	return 0;  // Non-zero return values are reserved for future use.
}
#endif


void test()
{


	glare::TaskManager task_manager;

	{
		BatchedMeshRef mesh = BatchedMesh::readFromFile(TestUtils::getTestReposDir() + "/testfiles/bmesh/Fox_glb_3500729461392160556.bmesh", NULL);
		BatchedMeshRef simplified_mesh1 = removeSmallComponents(mesh, 0.1f);
		BatchedMeshRef simplified_mesh3 = buildSimplifiedMesh(*mesh, /*target_reduction_ratio=*/10.f, /*target error=*/0.4f, false);

		std::vector<uint32> index_map;
		BatchedMeshRef simplified_mesh2 = removeInvisibleTriangles(mesh, index_map, task_manager);
	}
	
	//{
	//	BatchedMeshRef mesh = BatchedMesh::readFromFile("C:\\Users\\nick\\AppData\\Roaming\\Substrata/server_data/server_resources/Valhalla_gltf_10539081704724699996.bmesh", NULL);
	//	//BatchedMeshRef simplified_mesh = removeSmallComponents(*mesh, 0.1f);
	//	BatchedMeshRef simplified_mesh = buildSimplifiedMesh(*mesh, /*target_reduction_ratio=*/10.f, /*target error=*/0.1f, true);
	//}


//	buildLODVersions("D:\\models\\dancedevil_glb_16934124793649044515.bmesh");

	/*try
	{
		Indigo::Mesh indigo_mesh;
		Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getTestReposDir()) + "/testscenes/zomb_head.igmesh", indigo_mesh);

		BatchedMesh batched_mesh;
		batched_mesh.buildFromIndigoMesh(indigo_mesh);

		batched_mesh.writeToFile("zomb_head.bmesh");

		BatchedMeshRef simplified_mesh = buildSimplifiedMesh(batched_mesh, 100.f, 0.1f, true);

		simplified_mesh->writeToFile("zomb_head_simplified.bmesh");

		conPrint("zomb_head_simplified.bmesh size: " + toString(FileUtils::getFileSize("zomb_head_simplified.bmesh")) + " B");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}*/

	
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\i_robot_obj_obj_14262372365043307937.bmesh");
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\banksyflower_gltf_6744734537774616040_bmesh_6744734537774616040.bmesh");
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\2020_07_19_20_24_11_obj_6467811551951867739_bmesh_6467811551951867739.bmesh");

	//TEMP: Made LOD versions of all meshes in cyberspace resources dir
	/*{
		const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPaths("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources", "bmesh");

		for(size_t i=0; i<paths.size(); ++i)
		{
			buildLODVersions(paths[i]);
		}
	}*/

	conPrint("Done");
}


} // end namespace MeshSimplification


#endif // BUILD_TESTS
