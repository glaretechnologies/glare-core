/*=====================================================================
GLMeshBuilding.cpp
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "GLMeshBuilding.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "OpenGLMeshRenderData.h"
#include "../dll/include/IndigoMesh.h"
#include "../maths/mathstypes.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Sort.h"
#include "../utils/IncludeHalf.h"
#include "../utils/StackAllocator.h"
#include <vector>
#include <tracy/Tracy.hpp>


static const bool MEM_PROFILE = false;


Reference<OpenGLMeshRenderData> GLMeshBuilding::buildMeshRenderData(VertexBufferAllocator& allocator, 
	const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices)
{
	Reference<OpenGLMeshRenderData> meshdata_ref = new OpenGLMeshRenderData();
	OpenGLMeshRenderData& meshdata = *meshdata_ref;

	meshdata.has_uvs = !uvs.empty();
	meshdata.has_shading_normals = !normals.empty();
	meshdata.batches.resize(1);
	meshdata.batches[0].material_index = 0;
	meshdata.batches[0].num_indices = (uint32)indices.size();
	meshdata.batches[0].prim_start_offset_B = 0;

	meshdata.aabb_os = js::AABBox::emptyAABBox();
	meshdata.num_materials_referenced = 1;

	const size_t vertices_size = vertices.size();

	const int NUM_COMPONENTS = 8;
	js::Vector<float, 16> combined_vert_data(NUM_COMPONENTS * vertices_size);
	for(size_t i=0; i<vertices_size; ++i)
	{
		combined_vert_data[i*NUM_COMPONENTS + 0] = vertices[i].x;
		combined_vert_data[i*NUM_COMPONENTS + 1] = vertices[i].y;
		combined_vert_data[i*NUM_COMPONENTS + 2] = vertices[i].z;
		combined_vert_data[i*NUM_COMPONENTS + 3] = normals[i].x;
		combined_vert_data[i*NUM_COMPONENTS + 4] = normals[i].y;
		combined_vert_data[i*NUM_COMPONENTS + 5] = normals[i].z;
		combined_vert_data[i*NUM_COMPONENTS + 6] = uvs[i].x;
		combined_vert_data[i*NUM_COMPONENTS + 7] = uvs[i].y;

		meshdata.aabb_os.enlargeToHoldPoint(Vec4f(vertices[i].x, vertices[i].y, vertices[i].z, 1.f));
	}


	if(vertices_size < 65536)
	{
		meshdata.setIndexType(GL_UNSIGNED_SHORT);

		const size_t indices_size = indices.size();
		js::Vector<uint16, 16> index_buf(indices_size); // Build array of uint16 indices.
		for(size_t i=0; i<indices_size; ++i)
		{
			assert(indices[i] < 65536);
			index_buf[i] = (uint16)indices[i];
		}
		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(index_buf.data(), index_buf.dataSizeBytes());
	}
	else
	{
		meshdata.setIndexType(GL_UNSIGNED_INT);

		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(indices.data(), indices.dataSizeBytes());
	}

	VertexSpec& spec = meshdata.vertex_spec;

	const uint32 vert_stride = (uint32)(sizeof(float) * NUM_COMPONENTS); // also vertex size.

	// NOTE: The order of these attributes should be the same as in OpenGLProgram constructor with the glBindAttribLocations.
	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = vert_stride;
	pos_attrib.offset = 0;
	spec.attributes.push_back(pos_attrib);

	VertexAttrib normal_attrib;
	normal_attrib.enabled = true;
	normal_attrib.num_comps = 3;
	normal_attrib.type = GL_FLOAT;
	normal_attrib.normalised = false;
	normal_attrib.stride = vert_stride;
	normal_attrib.offset = sizeof(float) * 3; // goes after position
	spec.attributes.push_back(normal_attrib);

	VertexAttrib uv_attrib;
	uv_attrib.enabled = true;
	uv_attrib.num_comps = 2;
	uv_attrib.type = GL_FLOAT;
	uv_attrib.normalised = false;
	uv_attrib.stride = vert_stride;
	uv_attrib.offset = (uint32)(sizeof(float) * 6); // after position and normal.
	spec.attributes.push_back(uv_attrib);

	meshdata.vbo_handle = allocator.allocateVertexDataSpace(vert_stride, combined_vert_data.data(), combined_vert_data.dataSizeBytes());


	allocator.getOrCreateAndAssignVAOForMesh(meshdata, spec);
	
	return meshdata_ref;
}


// No normals version
Reference<OpenGLMeshRenderData> GLMeshBuilding::buildMeshRenderData(VertexBufferAllocator& allocator, 
	ArrayRef<float> vert_pos_and_uvs, ArrayRef<uint32> indices, glare::StackAllocator& stack_allocator)
{
	const int NUM_COMPONENTS = 5;
	assert(vert_pos_and_uvs.size() % NUM_COMPONENTS == 0);

	Reference<OpenGLMeshRenderData> meshdata_ref = new OpenGLMeshRenderData();
	OpenGLMeshRenderData& meshdata = *meshdata_ref;

	meshdata.has_uvs = true;
	meshdata.has_shading_normals = false;
	meshdata.batches.resize(1);
	meshdata.batches[0].material_index = 0;
	meshdata.batches[0].num_indices = (uint32)indices.size();
	meshdata.batches[0].prim_start_offset_B = 0;

	meshdata.aabb_os = js::AABBox::emptyAABBox();
	meshdata.num_materials_referenced = 1;

	const size_t num_vertices = vert_pos_and_uvs.size() / NUM_COMPONENTS;

	for(size_t i=0; i<num_vertices; ++i)
	{
		const Vec4f vertpos(vert_pos_and_uvs[i*NUM_COMPONENTS + 0], vert_pos_and_uvs[i*NUM_COMPONENTS + 1], vert_pos_and_uvs[i*NUM_COMPONENTS + 2], 1.f);
		meshdata.aabb_os.enlargeToHoldPoint(vertpos);
	}

	if(num_vertices < 65536)
	{
		meshdata.setIndexType(GL_UNSIGNED_SHORT);

		const size_t indices_size = indices.size();
		// Build array of uint16 indices.
		glare::StackAllocation index_buf_allocation(sizeof(uint16) * indices_size, /*alignment=*/16, stack_allocator);
		MutableArrayRef<uint16> index_buf((uint16*)index_buf_allocation.ptr, indices_size);
		for(size_t i=0; i<indices_size; ++i)
		{
			assert(indices[i] < 65536);
			index_buf[i] = (uint16)indices[i];
		}
		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(index_buf.data(), index_buf.dataSizeBytes());
	}
	else
	{
		meshdata.setIndexType(GL_UNSIGNED_INT);

		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(indices.data(), indices.dataSizeBytes());
	}

	VertexSpec& spec = meshdata.vertex_spec;

	const uint32 vert_stride = (uint32)(sizeof(float) * NUM_COMPONENTS); // also vertex size.

	// NOTE: The order of these attributes should be the same as in OpenGLProgram constructor with the glBindAttribLocations.
	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = vert_stride;
	pos_attrib.offset = 0;
	spec.attributes.push_back(pos_attrib);

	VertexAttrib normal_attrib;
	normal_attrib.enabled = false;
	normal_attrib.num_comps = 3;
	normal_attrib.type = GL_FLOAT;
	normal_attrib.normalised = false;
	normal_attrib.stride = vert_stride;
	normal_attrib.offset = 0;
	spec.attributes.push_back(normal_attrib);

	VertexAttrib uv_attrib;
	uv_attrib.enabled = true;
	uv_attrib.num_comps = 2;
	uv_attrib.type = GL_FLOAT;
	uv_attrib.normalised = false;
	uv_attrib.stride = vert_stride;
	uv_attrib.offset = (uint32)(sizeof(float) * 3); // after position.
	spec.attributes.push_back(uv_attrib);

	meshdata.vbo_handle = allocator.allocateVertexDataSpace(vert_stride, vert_pos_and_uvs.data(), vert_pos_and_uvs.dataSizeBytes());

	allocator.getOrCreateAndAssignVAOForMesh(meshdata, spec);

	return meshdata_ref;
}


Reference<OpenGLMeshRenderData> GLMeshBuilding::buildMeshRenderData(VertexBufferAllocator& allocator, const js::Vector<Vec3f, 16>& vertices, ArrayRef<uint32> indices)
{
	Reference<OpenGLMeshRenderData> meshdata_ref = new OpenGLMeshRenderData();
	OpenGLMeshRenderData& meshdata = *meshdata_ref;

	meshdata.has_uvs = false;
	meshdata.has_shading_normals = false;
	meshdata.batches.resize(1);
	meshdata.batches[0].material_index = 0;
	meshdata.batches[0].num_indices = (uint32)indices.size();
	meshdata.batches[0].prim_start_offset_B = 0;

	meshdata.aabb_os = js::AABBox::emptyAABBox();
	meshdata.num_materials_referenced = 1;

	const size_t num_vertices = vertices.size();

	for(size_t i=0; i<num_vertices; ++i)
		meshdata.aabb_os.enlargeToHoldPoint(vertices[i].toVec4fPoint());

	if(num_vertices < 65536)
	{
		meshdata.setIndexType(GL_UNSIGNED_SHORT);

		// Build array of uint16 indices.
		js::Vector<uint16> index_buf(indices.size());
		for(size_t i=0; i<indices.size(); ++i)
		{
			assert(indices[i] < 65536);
			index_buf[i] = (uint16)indices[i];
		}
		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(index_buf.data(), index_buf.dataSizeBytes());
	}
	else
	{
		meshdata.setIndexType(GL_UNSIGNED_INT);

		meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(indices.data(), indices.dataSizeBytes());
	}

	const uint32 vert_stride = (uint32)sizeof(Vec3f); // also vertex size.

	// NOTE: The order of these attributes should be the same as in OpenGLProgram constructor with the glBindAttribLocations.
	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = vert_stride;
	pos_attrib.offset = 0;
	meshdata.vertex_spec.attributes.push_back(pos_attrib);

	meshdata.vbo_handle = allocator.allocateVertexDataSpace(vert_stride, vertices.data(), vertices.dataSizeBytes());

	allocator.getOrCreateAndAssignVAOForMesh(meshdata, meshdata.vertex_spec);

	return meshdata_ref;
}


/*
When the triangle and quad vertex_indices and uv_indices are the same,
and the triangles and quads are (mostly) sorted by material, we can 
more-or-less directly load the mesh data into OpenGL, instead of building unique (pos, normal, uv0) vertices and sorting
indices by material.
*/
static bool canLoadMeshDirectly(const Indigo::Mesh* const mesh)
{
	// Some meshes just have a single UV of value (0,0).  We can treat this as just not having UVs.
	const bool use_mesh_uvs = (mesh->num_uv_mappings > 0) && !(mesh->uv_pairs.size() == 1 && mesh->uv_pairs[0] == Indigo::Vec2f(0, 0));

	// If we just have a single UV coord then we can handle this with a special case, and we don't require vert and uv indices to match.
	const bool single_uv = mesh->uv_pairs.size() == 1;

	bool need_uv_match = use_mesh_uvs && !single_uv;

	uint32 last_mat_index = std::numeric_limits<uint32>::max();
	uint32 num_changes = 0; // Number of changes of the current material

	const Indigo::Triangle* tris = mesh->triangles.data();
	const size_t num_tris = mesh->triangles.size();
	for(size_t t=0; t<num_tris; ++t)
	{
		const Indigo::Triangle& tri = tris[t];
		const uint32 mat_index = tri.tri_mat_index;
		if(mat_index != last_mat_index)
			num_changes++;
		last_mat_index = mat_index;

		if(need_uv_match &&
			(tri.vertex_indices[0] != tri.uv_indices[0] ||
			tri.vertex_indices[1] != tri.uv_indices[1] ||
			tri.vertex_indices[2] != tri.uv_indices[2]))
			return false;
	}

	const Indigo::Quad* quads = mesh->quads.data();
	const size_t num_quads = mesh->quads.size();
	for(size_t q=0; q<num_quads; ++q)
	{
		const Indigo::Quad& quad = quads[q];
		const uint32 mat_index = quad.mat_index;
		if(mat_index != last_mat_index)
			num_changes++;
		last_mat_index = mat_index;

		if(need_uv_match &&
			(quad.vertex_indices[0] != quad.uv_indices[0] ||
			quad.vertex_indices[1] != quad.uv_indices[1] ||
			quad.vertex_indices[2] != quad.uv_indices[2] ||
			quad.vertex_indices[3] != quad.uv_indices[3]))
			return false;
	}

	return num_changes <= mesh->num_materials_referenced * 2;
}


// If UVs are somewhat small in magnitude, use GL_HALF_FLOAT instead of GL_FLOAT.
// If the magnitude is too high we can get articacts if we just use half precision.
static bool canUseHalfUVs(const Indigo::Mesh* const mesh)
{
	const Indigo::Vec2f* const uv_pairs	= mesh->uv_pairs.data();
	const size_t uvs_size				= mesh->uv_pairs.size();

	const float max_use_half_range = 10.f;
	for(size_t i=0; i<uvs_size; ++i)
		if(std::fabs(uv_pairs[i].x) > max_use_half_range || std::fabs(uv_pairs[i].y) > max_use_half_range)
			return false;
	return true;
}


struct UVsAtVert
{
	UVsAtVert() : merged_v_index(-1) {}

	Vec2f uv;
	int merged_v_index;
};


// Pack normal into GL_INT_2_10_10_10_REV format.
inline static uint32 packNormal(const Indigo::Vec3f& normal)
{
	int x = (int)(normal.x * 511.f);
	int y = (int)(normal.y * 511.f);
	int z = (int)(normal.z * 511.f);
	// ANDing with 1023 isolates the bottom 10 bits.
	return (x & 1023) | ((y & 1023) << 10) | ((z & 1023) << 20);
}


struct TakeFirstElement
{
	inline uint32 operator() (const std::pair<uint32, uint32>& pair) const { return pair.first; }
};


// This is used to combine vertices with the same position, normal, and uv.
// Note that there is a tradeoff here - we could combine with the full position vector, normal, and uvs, but then the keys would be slow to compare.
// Or we could compare with the existing indices.  This will combine vertices effectively only if there are merged (not duplicated) in the Indigo mesh.
// In practice positions are usually combined (subdiv relies on it etc..), but UVs often aren't.  So we will use the index for positions, and the actual UV (0) value
// for the UVs.


//#define USE_INDIGO_MESH_INDICES 1


Reference<OpenGLMeshRenderData> GLMeshBuilding::buildIndigoMesh(VertexBufferAllocator* allocator, const Reference<Indigo::Mesh>& mesh_, bool skip_opengl_calls)
{
	if(mesh_->triangles.empty() && mesh_->quads.empty())
		throw glare::Exception("Mesh empty.");
	//-------------------------------------------------------------------------------------------------------
#if USE_INDIGO_MESH_INDICES
	if(!mesh_->indices.empty()) // If this mesh uses batches of primitives already that already share materials:
	{
		Timer timer;
		const Indigo::Mesh* const mesh = mesh_.getPointer();
		const bool mesh_has_shading_normals = !mesh->vert_normals.empty();
		const bool mesh_has_uvs = mesh->num_uv_mappings > 0;
		const uint32 num_uv_sets = mesh->num_uv_mappings;

		Reference<OpenGLMeshRenderData> opengl_render_data = new OpenGLMeshRenderData();

		// If UVs are somewhat small in magnitude, use GL_HALF_FLOAT instead of GL_FLOAT.
		// If the magnitude is too high we can get articacts if we just use half precision.
		const float max_use_half_range = 10.f;
		bool use_half_uvs = true;
		for(size_t i=0; i<mesh->uv_pairs.size(); ++i)
			if(std::fabs(mesh->uv_pairs[i].x) > max_use_half_range || std::fabs(mesh->uv_pairs[i].y) > max_use_half_range)
				use_half_uvs = false;

		js::Vector<uint8, 16> vert_data;
		const size_t packed_normal_size = 4; // 4 bytes since we are using GL_INT_2_10_10_10_REV format.
		const size_t packed_uv_size = use_half_uvs ? sizeof(half)*2 : sizeof(float)*2;
		const size_t num_bytes_per_vert = sizeof(float)*3 + (mesh_has_shading_normals ? packed_normal_size : 0) + (mesh_has_uvs ? packed_uv_size : 0);
		const size_t normal_offset      = sizeof(float)*3;
		const size_t uv_offset          = sizeof(float)*3 + (mesh_has_shading_normals ? packed_normal_size : 0);
		vert_data.resizeNoCopy(mesh->vert_positions.size() * num_bytes_per_vert);


		const size_t vert_positions_size = mesh->vert_positions.size();
		const size_t uvs_size = mesh->uv_pairs.size();

		// Copy vertex positions, normals and UVs to OpenGL mesh data.
		for(size_t i=0; i<vert_positions_size; ++i)
		{
			const size_t offset = num_bytes_per_vert * i;

			std::memcpy(&vert_data[offset], &mesh->vert_positions[i].x, sizeof(Indigo::Vec3f));

			if(mesh_has_shading_normals)
			{
				// Pack normal into GL_INT_2_10_10_10_REV format.
				int x = (int)((mesh->vert_normals[i].x) * 511.f);
				int y = (int)((mesh->vert_normals[i].y) * 511.f);
				int z = (int)((mesh->vert_normals[i].z) * 511.f);
				// ANDing with 1023 isolates the bottom 10 bits.
				uint32 n = (x & 1023) | ((y & 1023) << 10) | ((z & 1023) << 20);
				std::memcpy(&vert_data[offset + normal_offset], &n, 4);
			}

			if(mesh_has_uvs)
			{
				if(use_half_uvs)
				{
					half uv[2];
					uv[0] = half(mesh->uv_pairs[i].x);
					uv[1] = half(mesh->uv_pairs[i].y);
					std::memcpy(&vert_data[offset + uv_offset], uv, 4);
				}
				else
					std::memcpy(&vert_data[offset + uv_offset], &mesh->uv_pairs[i].x, sizeof(Indigo::Vec2f));
			}
		}


		// Build batches
		for(size_t i=0; i<mesh->chunks.size(); ++i)
		{
			opengl_render_data->batches.push_back(OpenGLBatch());
			OpenGLBatch& batch = opengl_render_data->batches.back();
			batch.material_index = mesh->chunks[i].mat_index;
			batch.prim_start_offset_B = mesh->chunks[i].indices_start * sizeof(uint32);
			batch.num_indices = mesh->chunks[i].num_indices;
			
			// If subsequent batches use the same material, combine them
			for(size_t z=i + 1; z<mesh->chunks.size(); ++z)
			{
				if(mesh->chunks[z].mat_index == batch.material_index)
				{
					// Combine:
					batch.num_indices += mesh->chunks[z].num_indices;
					i++;
				}
				else
					break;
			}
		}


		if(skip_opengl_calls)
			return opengl_render_data;

		opengl_render_data->vert_vbo = new VBO(&vert_data[0], vert_data.dataSizeBytes());

		VertexSpec spec;

		VertexAttrib pos_attrib;
		pos_attrib.enabled = true;
		pos_attrib.num_comps = 3;
		pos_attrib.type = GL_FLOAT;
		pos_attrib.normalised = false;
		pos_attrib.stride = (uint32)num_bytes_per_vert;
		pos_attrib.offset = 0;
		spec.attributes.push_back(pos_attrib);

		VertexAttrib normal_attrib;
		normal_attrib.enabled = mesh_has_shading_normals;
		normal_attrib.num_comps = 4;
		normal_attrib.type = GL_INT_2_10_10_10_REV;
		normal_attrib.normalised = true;
		normal_attrib.stride = (uint32)num_bytes_per_vert;
		normal_attrib.offset = (uint32)normal_offset;
		spec.attributes.push_back(normal_attrib);

		VertexAttrib uv_attrib;
		uv_attrib.enabled = mesh_has_uvs;
		uv_attrib.num_comps = 2;
		uv_attrib.type = use_half_uvs ? GL_HALF_FLOAT : GL_FLOAT;
		uv_attrib.normalised = false;
		uv_attrib.stride = (uint32)num_bytes_per_vert;
		uv_attrib.offset = (uint32)uv_offset;
		spec.attributes.push_back(uv_attrib);

		opengl_render_data->vert_vao = new VAO(opengl_render_data->vert_vbo, spec);

		opengl_render_data->has_uvs				= mesh_has_uvs;
		opengl_render_data->has_shading_normals = mesh_has_shading_normals;

		const size_t vert_index_buffer_size = mesh->indices.size();

		if(mesh->vert_positions.size() < 256)
		{
			js::Vector<uint8, 16> index_buf; index_buf.resize(vert_index_buffer_size);
			for(size_t i=0; i<vert_index_buffer_size; ++i)
			{
				assert(mesh->indices[i] < 256);
				index_buf[i] = (uint8)mesh->indices[i];
			}
			opengl_render_data->vert_indices_buf = new VBO(&index_buf[0], index_buf.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
			opengl_render_data->index_type = GL_UNSIGNED_BYTE;
			// Go through the batches and adjust the start offset to take into account we're using uint8s.
			for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
				opengl_render_data->batches[i].prim_start_offset_B /= 4;
		}
		else if(mesh->vert_positions.size() < 65536)
		{
			js::Vector<uint16, 16> index_buf; index_buf.resize(vert_index_buffer_size);
			for(size_t i=0; i<vert_index_buffer_size; ++i)
			{
				assert(mesh->indices[i] < 65536);
				index_buf[i] = (uint16)mesh->indices[i];
			}
			opengl_render_data->vert_indices_buf = new VBO(&index_buf[0], index_buf.dataSizeBytes(), GL_ELEMENT_ARRAY_BUFFER);
			opengl_render_data->index_type = GL_UNSIGNED_SHORT;
			// Go through the batches and adjust the start offset to take into account we're using uint16s.
			for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
				opengl_render_data->batches[i].prim_start_offset_B /= 2;
		}
		else
		{
			opengl_render_data->vert_indices_buf = new VBO(mesh->indices.data(), mesh->indices.size() * sizeof(uint32), GL_ELEMENT_ARRAY_BUFFER);
			opengl_render_data->index_type = GL_UNSIGNED_INT;
		}

#ifndef NDEBUG
		for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
		{
			const OpenGLBatch& batch = opengl_render_data->batches[i];
			assert(batch.material_index < mesh->num_materials_referenced);
			assert(batch.num_indices > 0);
			assert(batch.prim_start_offset_B < opengl_render_data->vert_indices_buf->getSize());
			const uint32 bytes_per_index = opengl_render_data->index_type == GL_UNSIGNED_INT ? 4 : (opengl_render_data->index_type == GL_UNSIGNED_SHORT ? 2 : 1);
			assert(batch.prim_start_offset_B + batch.num_indices*bytes_per_index <= opengl_render_data->vert_indices_buf->getSize());
		}
#endif

		if(MEM_PROFILE)
		{
			const int pad_w = 8;
			conPrint("Src num verts:     " + toString(mesh->vert_positions.size()) + ", num tris: " + toString(mesh->triangles.size()) + ", num quads: " + toString(mesh->quads.size()));
			if(use_half_uvs) conPrint("Using half UVs.");
			conPrint("verts:             " + rightPad(toString(mesh->vert_positions.size()), ' ', pad_w) + "(" + getNiceByteSize(vert_data.dataSizeBytes()) + ")");
			//conPrint("merged_positions:  " + rightPad(toString(num_merged_verts),         ' ', pad_w) + "(" + getNiceByteSize(merged_positions.dataSizeBytes()) + ")");
			//conPrint("merged_normals:    " + rightPad(toString(merged_normals.size()),    ' ', pad_w) + "(" + getNiceByteSize(merged_normals.dataSizeBytes()) + ")");
			//conPrint("merged_uvs:        " + rightPad(toString(merged_uvs.size()),        ' ', pad_w) + "(" + getNiceByteSize(merged_uvs.dataSizeBytes()) + ")");
			conPrint("vert_index_buffer: " + rightPad(toString(mesh->indices.size()), ' ', pad_w) + "(" + getNiceByteSize(opengl_render_data->vert_indices_buf->getSize()) + ")");
		}

		opengl_render_data->aabb_os.min_ = Vec4f(mesh_->aabb_os.bound[0].x, mesh_->aabb_os.bound[0].y, mesh_->aabb_os.bound[0].z, 1.f);
		opengl_render_data->aabb_os.max_ = Vec4f(mesh_->aabb_os.bound[1].x, mesh_->aabb_os.bound[1].y, mesh_->aabb_os.bound[1].z, 1.f);
		return opengl_render_data;
	}
#endif
	//-------------------------------------------------------------------------------------------------------

	Timer timer;

	const Indigo::Mesh* const mesh				= mesh_.getPointer();
	const Indigo::Triangle* const tris			= mesh->triangles.data();
	const size_t num_tris						= mesh->triangles.size();
	const Indigo::Quad* const quads				= mesh->quads.data();
	const size_t num_quads						= mesh->quads.size();
	const Indigo::Vec3f* const vert_positions	= mesh->vert_positions.data();
	const size_t vert_positions_size			= mesh->vert_positions.size();
	const Indigo::Vec3f* const vert_normals		= mesh->vert_normals.data();
	const Indigo::Vec3f* const vert_colours		= mesh->vert_colours.data();
	const Indigo::Vec2f* const uv_pairs			= mesh->uv_pairs.data();
	const size_t uvs_size						= mesh->uv_pairs.size();

	const bool mesh_has_shading_normals			= !mesh->vert_normals.empty();
	      bool mesh_has_uvs						= mesh->num_uv_mappings > 0;
	const uint32 num_uv_sets					= mesh->num_uv_mappings;
	const bool mesh_has_vert_cols				= !mesh->vert_colours.empty();


	// Some meshes just have a single UV of value (0,0).  We can treat this as just not having UVs.
	if(uvs_size == 1 && uv_pairs[0] == Indigo::Vec2f(0, 0))
		mesh_has_uvs = false;

	// If we just have a single UV coord then we can handle this with a special case, and we don't require vert and uv indices to match.
	const bool single_uv = uvs_size == 1;

	assert(mesh->num_materials_referenced > 0);

	Reference<OpenGLMeshRenderData> opengl_render_data = new OpenGLMeshRenderData();

	// If UVs are somewhat small in magnitude, use GL_HALF_FLOAT instead of GL_FLOAT.
	// If the magnitude is too high we can get articacts if we just use half precision.
	const bool use_half_uvs = canUseHalfUVs(mesh);

	const size_t pos_size = sizeof(float)*3;
	const size_t packed_normal_size = 4; // 4 bytes since we are using GL_INT_2_10_10_10_REV format.
	const size_t packed_uv_size = use_half_uvs ? sizeof(half)*2 : sizeof(float)*2;

	/*
	Vertex data layout is
	position [always present]
	normal   [optional]
	uv_0     [optional]
	colour   [optional]
	*/
	const size_t normal_offset      = pos_size;
	const size_t uv_offset          = normal_offset   + (mesh_has_shading_normals ? packed_normal_size : 0);
	const size_t vert_col_offset    = uv_offset       + (mesh_has_uvs ? packed_uv_size : 0);
	const size_t num_bytes_per_vert = vert_col_offset + (mesh_has_vert_cols ? sizeof(float)*3 : 0);
	glare::AllocatorVector<uint8, 16>& vert_data = opengl_render_data->vert_data;
	vert_data.reserve(mesh->vert_positions.size() * num_bytes_per_vert);
	

	glare::AllocatorVector<uint32, 16>& vert_index_buffer = opengl_render_data->vert_index_buffer;
	vert_index_buffer.resizeNoCopy(mesh->triangles.size()*3 + mesh->quads.size()*6); // Quads are rendered as two tris.
	

	const bool can_load_mesh_directly = canLoadMeshDirectly(mesh);

	size_t num_merged_verts;

	if(can_load_mesh_directly)
	{
		// Don't need to create unique vertices and sort by material.

		// Make vertex index buffer from triangles, also build batch info.
		size_t write_i = 0; // Current write index into vert_index_buffer
		uint32 last_mat_index = std::numeric_limits<uint32>::max();
		size_t last_pass_start_index = 0;
		for(size_t t=0; t<num_tris; ++t)
		{
			const Indigo::Triangle& tri = tris[t];
			vert_index_buffer[write_i + 0] = tri.vertex_indices[0];
			vert_index_buffer[write_i + 1] = tri.vertex_indices[1];
			vert_index_buffer[write_i + 2] = tri.vertex_indices[2];
			
			if(tri.tri_mat_index != last_mat_index)
			{
				if(t > 0) // Don't add zero-length passes.
				{
					OpenGLBatch batch;
					batch.material_index = last_mat_index;
					batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32));
					batch.num_indices = (uint32)(write_i - last_pass_start_index);
					opengl_render_data->batches.push_back(batch);
				}
				last_mat_index = tri.tri_mat_index;
				last_pass_start_index = write_i;
			}

			write_i += 3;
		}
		for(size_t q=0; q<num_quads; ++q)
		{
			const Indigo::Quad& quad = quads[q];
			vert_index_buffer[write_i + 0] = quad.vertex_indices[0];
			vert_index_buffer[write_i + 1] = quad.vertex_indices[1];
			vert_index_buffer[write_i + 2] = quad.vertex_indices[2];
			vert_index_buffer[write_i + 3] = quad.vertex_indices[0];
			vert_index_buffer[write_i + 4] = quad.vertex_indices[2];
			vert_index_buffer[write_i + 5] = quad.vertex_indices[3];

			if(quad.mat_index != last_mat_index)
			{
				if(write_i > last_pass_start_index) // Don't add zero-length passes.
				{
					OpenGLBatch batch;
					batch.material_index = last_mat_index;
					batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32));
					batch.num_indices = (uint32)(write_i - last_pass_start_index);
					opengl_render_data->batches.push_back(batch);
				}
				last_mat_index = quad.mat_index;
				last_pass_start_index = write_i;
			}

			write_i += 6;
		}
		// Build last pass data that won't have been built yet.
		{
			OpenGLBatch batch;
			batch.material_index = last_mat_index;
			batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32)); // Offset in bytes
			batch.num_indices = (uint32)(write_i - last_pass_start_index);
			opengl_render_data->batches.push_back(batch);
		}

		// Build vertex data
		vert_data.resize(mesh->vert_positions.size() * num_bytes_per_vert);
		write_i = 0;
		for(size_t v=0; v<vert_positions_size; ++v)
		{
			// Copy vert position
			std::memcpy(&vert_data[write_i], &vert_positions[v].x, sizeof(Indigo::Vec3f));

			// Copy vertex normal
			if(mesh_has_shading_normals)
			{
				const uint32 n = packNormal(vert_normals[v]); // Pack normal into GL_INT_2_10_10_10_REV format.
				std::memcpy(&vert_data[write_i + normal_offset], &n, 4);
			}

			// Copy UVs
			if(mesh_has_uvs)
			{
				const Indigo::Vec2f uv = single_uv ? uv_pairs[0] : uv_pairs[v * num_uv_sets]; // v * num_uv_sets = Index of UV for UV set 0.
				if(use_half_uvs)
				{
					half half_uv[2];
					half_uv[0] = half(uv.x);
					half_uv[1] = half(uv.y);
					std::memcpy(&vert_data[write_i + uv_offset], half_uv, 4);
				}
				else
					std::memcpy(&vert_data[write_i + uv_offset], &uv.x, sizeof(Indigo::Vec2f));
			}

			// Copy vert colour
			if(mesh_has_vert_cols)
				std::memcpy(&vert_data[write_i + vert_col_offset], &vert_colours[v].x, sizeof(Indigo::Vec3f));

			write_i += num_bytes_per_vert;
		}

		num_merged_verts = mesh->vert_positions.size();
	}
	else // ----------------- else if can't load mesh directly: ------------------------
	{
		size_t vert_index_buffer_i = 0; // Current write index into vert_index_buffer
		size_t next_merged_vert_i = 0;
		size_t last_pass_start_index = 0;
		uint32 current_mat_index = std::numeric_limits<uint32>::max();

		std::vector<UVsAtVert> uvs_at_vert(mesh->vert_positions.size());

		// Create per-material OpenGL triangle indices
		if(mesh->triangles.size() > 0)
		{
			// Create list of triangle references sorted by material index
			js::Vector<std::pair<uint32, uint32>, 16> unsorted_tri_indices(num_tris);
			js::Vector<std::pair<uint32, uint32>, 16> tri_indices         (num_tris); // Sorted by material

			for(uint32 t = 0; t < num_tris; ++t)
				unsorted_tri_indices[t] = std::make_pair(tris[t].tri_mat_index, t);

			Sort::serialCountingSort(/*in=*/unsorted_tri_indices.data(), /*out=*/tri_indices.data(), num_tris, TakeFirstElement());

			for(uint32 t = 0; t < num_tris; ++t)
			{
				// If we've switched to a new material then start a new triangle range
				if(tri_indices[t].first != current_mat_index)
				{
					if(t > 0) // Don't add zero-length passes.
					{
						OpenGLBatch batch;
						batch.material_index = current_mat_index;
						batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32));
						batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
						opengl_render_data->batches.push_back(batch);
					}
					last_pass_start_index = vert_index_buffer_i;
					current_mat_index = tri_indices[t].first;
				}
			
				const Indigo::Triangle& tri = tris[tri_indices[t].second];
				for(uint32 i = 0; i < 3; ++i) // For each vert in tri:
				{
					const uint32 pos_i		= tri.vertex_indices[i];
					const uint32 base_uv_i	= tri.uv_indices[i];
					const uint32 uv_i = base_uv_i * num_uv_sets; // Index of UV for UV set 0.
					if(pos_i >= vert_positions_size)
						throw glare::Exception("vert index out of bounds");
					if(mesh_has_uvs && uv_i >= uvs_size)
						throw glare::Exception("UV index out of bounds");

					// Look up merged vertex
					const Vec2f uv = mesh_has_uvs ? Vec2f(uv_pairs[uv_i].x, uv_pairs[uv_i].y) : Vec2f(0.f);

					UVsAtVert& at_vert = uvs_at_vert[pos_i];
					const bool found = at_vert.merged_v_index != -1 && at_vert.uv == uv;

					uint32 merged_v_index;
					if(!found) // Not created yet:
					{
						merged_v_index = (uint32)next_merged_vert_i++;

						if(at_vert.merged_v_index == -1) // If there is no UV inserted yet for this vertex position:
						{
							at_vert.uv = uv; // Insert it
							at_vert.merged_v_index = merged_v_index;
						}

						const size_t cur_size = vert_data.size();
						vert_data.resize(cur_size + num_bytes_per_vert);
						std::memcpy(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f)); // Copy vert position

						if(mesh_has_shading_normals)
						{
							const uint32 n = packNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
							std::memcpy(&vert_data[cur_size + normal_offset], &n, 4);
						}

						if(mesh_has_uvs)
						{
							if(use_half_uvs)
							{
								half half_uv[2];
								half_uv[0] = half(uv.x);
								half_uv[1] = half(uv.y);
								std::memcpy(&vert_data[cur_size + uv_offset], half_uv, 4);
							}
							else
								std::memcpy(&vert_data[cur_size + uv_offset], &uv.x, sizeof(Indigo::Vec2f));
						}

						if(mesh_has_vert_cols)
							std::memcpy(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
					}
					else
					{
						merged_v_index = at_vert.merged_v_index; // Else a vertex with this position index and UV has already been created, use it
					}

					vert_index_buffer[vert_index_buffer_i++] = merged_v_index;
				}
			}
		}

		if(mesh->quads.size() > 0)
		{
			// Create list of quad references sorted by material index
			js::Vector<std::pair<uint32, uint32>, 16> unsorted_quad_indices(num_quads);
			js::Vector<std::pair<uint32, uint32>, 16> quad_indices         (num_quads); // Sorted by material
				
			for(uint32 q = 0; q < num_quads; ++q)
				unsorted_quad_indices[q] = std::make_pair(quads[q].mat_index, q);
		
			Sort::serialCountingSort(/*in=*/unsorted_quad_indices.data(), /*out=*/quad_indices.data(), num_quads, TakeFirstElement());

			for(uint32 q = 0; q < num_quads; ++q)
			{
				// If we've switched to a new material then start a new quad range
				if(quad_indices[q].first != current_mat_index)
				{
					if(vert_index_buffer_i > last_pass_start_index) // Don't add zero-length passes.
					{
						OpenGLBatch batch;
						batch.material_index = current_mat_index;
						batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32));
						batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
						opengl_render_data->batches.push_back(batch);
					}
					last_pass_start_index = vert_index_buffer_i;
					current_mat_index = quad_indices[q].first;
				}
			
				const Indigo::Quad& quad = quads[quad_indices[q].second];
				uint32 vert_merged_index[4];
				for(uint32 i = 0; i < 4; ++i) // For each vert in quad:
				{
					const uint32 pos_i  = quad.vertex_indices[i];
					const uint32 uv_i   = quad.uv_indices[i];
					if(pos_i >= vert_positions_size)
						throw glare::Exception("vert index out of bounds");
					if(mesh_has_uvs && uv_i >= uvs_size)
						throw glare::Exception("UV index out of bounds");

					// Look up merged vertex
					const Vec2f uv = mesh_has_uvs ? Vec2f(uv_pairs[uv_i].x, uv_pairs[uv_i].y) : Vec2f(0.f);

					UVsAtVert& at_vert = uvs_at_vert[pos_i];
					const bool found = at_vert.merged_v_index != -1 && at_vert.uv == uv;

					uint32 merged_v_index;
					if(!found)
					{
						merged_v_index = (uint32)next_merged_vert_i++;

						if(at_vert.merged_v_index == -1) // If there is no UV inserted yet for this vertex position:
						{
							at_vert.uv = uv; // Insert it
							at_vert.merged_v_index = merged_v_index;
						}

						const size_t cur_size = vert_data.size();
						vert_data.resize(cur_size + num_bytes_per_vert);
						std::memcpy(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f));
						if(mesh_has_shading_normals)
						{
							const uint32 n = packNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
							std::memcpy(&vert_data[cur_size + normal_offset], &n, 4);
						}

						if(mesh_has_uvs)
						{
							if(use_half_uvs)
							{
								half half_uv[2];
								half_uv[0] = half(uv.x);
								half_uv[1] = half(uv.y);
								std::memcpy(&vert_data[cur_size + uv_offset], half_uv, 4);
							}
							else
								std::memcpy(&vert_data[cur_size + uv_offset], &uv_pairs[uv_i].x, sizeof(Indigo::Vec2f));
						}

						if(mesh_has_vert_cols)
							std::memcpy(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
					}
					else
					{
						merged_v_index = at_vert.merged_v_index; // Else a vertex with this position index and UV has already been created, use it.
					}

					vert_merged_index[i] = merged_v_index;
				} // End for each vert in quad:

				// Tri 1 of quad
				vert_index_buffer[vert_index_buffer_i + 0] = vert_merged_index[0];
				vert_index_buffer[vert_index_buffer_i + 1] = vert_merged_index[1];
				vert_index_buffer[vert_index_buffer_i + 2] = vert_merged_index[2];
				// Tri 2 of quad
				vert_index_buffer[vert_index_buffer_i + 3] = vert_merged_index[0];
				vert_index_buffer[vert_index_buffer_i + 4] = vert_merged_index[2];
				vert_index_buffer[vert_index_buffer_i + 5] = vert_merged_index[3];

				vert_index_buffer_i += 6;
			}
		}

		// Build last pass data that won't have been built yet.
		OpenGLBatch batch;
		batch.material_index = current_mat_index;
		batch.prim_start_offset_B = (uint32)(last_pass_start_index * sizeof(uint32));
		batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
		opengl_render_data->batches.push_back(batch);

		assert(vert_index_buffer_i == (uint32)vert_index_buffer.size());
		num_merged_verts = next_merged_vert_i;
	}

	/*conPrint("--------------\n"
		"tris: " + toString(mesh->triangles.size()) + "\n" +
		"quads: " + toString(mesh->quads.size()) + "\n" +
		//"num_changes: " + toString(num_changes) + "\n" +
		"num materials: " + toString(mesh->num_materials_referenced) + "\n" +
		"num vert positions: " + toString(mesh->vert_positions.size()) + "\n" +
		"num UVs:            " + toString(mesh->uv_pairs.size()) + "\n" +
		"mesh_has_uvs: " + boolToString(mesh_has_uvs) + "\n" +
		"can_load_mesh_directly: " + boolToString(can_load_mesh_directly) + "\n" +
		"num_merged_verts: " + toString(num_merged_verts));*/

	

	VertexSpec& spec = opengl_render_data->vertex_spec;

	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = (uint32)num_bytes_per_vert;
	pos_attrib.offset = 0;
	spec.attributes.push_back(pos_attrib);

	VertexAttrib normal_attrib;
	normal_attrib.enabled = mesh_has_shading_normals;
	normal_attrib.num_comps = 4;
	normal_attrib.type = GL_INT_2_10_10_10_REV;
	normal_attrib.normalised = true;
	normal_attrib.stride = (uint32)num_bytes_per_vert;
	normal_attrib.offset = (uint32)normal_offset;
	spec.attributes.push_back(normal_attrib);

	VertexAttrib uv_attrib;
	uv_attrib.enabled = mesh_has_uvs;
	uv_attrib.num_comps = 2;
	uv_attrib.type = use_half_uvs ? GL_HALF_FLOAT : GL_FLOAT;
	uv_attrib.normalised = false;
	uv_attrib.stride = (uint32)num_bytes_per_vert;
	uv_attrib.offset = (uint32)uv_offset;
	spec.attributes.push_back(uv_attrib);

	VertexAttrib colour_attrib;
	colour_attrib.enabled = mesh_has_vert_cols;
	colour_attrib.num_comps = 3;
	colour_attrib.type = GL_FLOAT;
	colour_attrib.normalised = false;
	colour_attrib.stride = (uint32)num_bytes_per_vert;
	colour_attrib.offset = (uint32)vert_col_offset;
	spec.attributes.push_back(colour_attrib);


	opengl_render_data->has_uvs				= mesh_has_uvs;
	opengl_render_data->has_shading_normals = mesh_has_shading_normals;
	opengl_render_data->has_vert_colours	= mesh_has_vert_cols;

	assert(!vert_index_buffer.empty());
	const size_t vert_index_buffer_size = vert_index_buffer.size();

	if(num_merged_verts < 256)
	{
		glare::AllocatorVector<uint8, 16>& index_buf = opengl_render_data->vert_index_buffer_uint8;
		index_buf.resize(vert_index_buffer_size);
		for(size_t i=0; i<vert_index_buffer_size; ++i)
		{
			assert(vert_index_buffer[i] < 256);
			index_buf[i] = (uint8)vert_index_buffer[i];
		}
		if(!skip_opengl_calls)
			opengl_render_data->indices_vbo_handle = allocator->allocateIndexDataSpace(index_buf.data(), index_buf.dataSizeBytes());

		opengl_render_data->setIndexType(GL_UNSIGNED_BYTE);

		// Go through the batches and adjust the start offset to take into account we're using uint8s.
		for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
			opengl_render_data->batches[i].prim_start_offset_B /= 4;
	}
	else if(num_merged_verts < 65536)
	{
		glare::AllocatorVector<uint16, 16>& index_buf = opengl_render_data->vert_index_buffer_uint16;
		index_buf.resize(vert_index_buffer_size);
		for(size_t i=0; i<vert_index_buffer_size; ++i)
		{
			assert(vert_index_buffer[i] < 65536);
			index_buf[i] = (uint16)vert_index_buffer[i];
		}
		if(!skip_opengl_calls)
			opengl_render_data->indices_vbo_handle = allocator->allocateIndexDataSpace(index_buf.data(), index_buf.dataSizeBytes());

		opengl_render_data->setIndexType(GL_UNSIGNED_SHORT);

		// Go through the batches and adjust the start offset to take into account we're using uint16s.
		for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
			opengl_render_data->batches[i].prim_start_offset_B /= 2;
	}
	else
	{
		if(!skip_opengl_calls)
			opengl_render_data->indices_vbo_handle = allocator->allocateIndexDataSpace(vert_index_buffer.data(), vert_index_buffer.dataSizeBytes());
		opengl_render_data->setIndexType(GL_UNSIGNED_INT);
	}

#ifndef NDEBUG
	for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
	{
		const OpenGLBatch& batch = opengl_render_data->batches[i];
		assert(batch.material_index < mesh->num_materials_referenced);
		assert(batch.num_indices > 0);
		//assert(batch.prim_start_offset < opengl_render_data->vert_indices_buf->getSize());
		//const uint32 bytes_per_index = opengl_render_data->index_type == GL_UNSIGNED_INT ? 4 : (opengl_render_data->index_type == GL_UNSIGNED_SHORT ? 2 : 1);
		//assert(batch.prim_start_offset + batch.num_indices*bytes_per_index <= opengl_render_data->vert_indices_buf->getSize());
	}

	// Check indices
	for(size_t i=0; i<vert_index_buffer.size(); ++i)
		assert(vert_index_buffer[i] < num_merged_verts);
#endif

	if(MEM_PROFILE)
	{
		const int pad_w = 8;
		conPrint("Src num verts:     " + toString(mesh->vert_positions.size()) + ", num tris: " + toString(mesh->triangles.size()) + ", num quads: " + toString(mesh->quads.size()));
		if(use_half_uvs) conPrint("Using half UVs.");
		conPrint("verts:             " + rightPad(toString(num_merged_verts),         ' ', pad_w) + "(" + getNiceByteSize(vert_data.dataSizeBytes()) + ")");
		//conPrint("vert_index_buffer: " + rightPad(toString(vert_index_buffer.size()), ' ', pad_w) + "(" + getNiceByteSize(opengl_render_data->vert_indices_buf->getSize()) + ")");
	}

	spec.checkValid();

	// If we did the OpenGL calls, then the data has been uploaded to VBOs etc.. so we can free it.
	if(!skip_opengl_calls)
	{
		opengl_render_data->vbo_handle = allocator->allocateVertexDataSpace(num_bytes_per_vert, vert_data.data(), vert_data.dataSizeBytes());

		allocator->getOrCreateAndAssignVAOForMesh(*opengl_render_data, spec);

		opengl_render_data->vert_data.clearAndFreeMem();
		opengl_render_data->vert_index_buffer.clearAndFreeMem();
		opengl_render_data->vert_index_buffer_uint16.clearAndFreeMem();
		opengl_render_data->vert_index_buffer_uint8.clearAndFreeMem();
	}

	opengl_render_data->aabb_os.min_ = Vec4f(mesh_->aabb_os.bound[0].x, mesh_->aabb_os.bound[0].y, mesh_->aabb_os.bound[0].z, 1.f);
	opengl_render_data->aabb_os.max_ = Vec4f(mesh_->aabb_os.bound[1].x, mesh_->aabb_os.bound[1].y, mesh_->aabb_os.bound[1].z, 1.f);
	return opengl_render_data;
}


static GLenum componentTypeGLEnum(BatchedMesh::ComponentType t)
{
	switch(t)
	{
	case BatchedMesh::ComponentType_Float:			return GL_FLOAT;
	case BatchedMesh::ComponentType_Half:			return GL_HALF_FLOAT;
	case BatchedMesh::ComponentType_UInt8:			return GL_UNSIGNED_BYTE;
	case BatchedMesh::ComponentType_UInt16:			return GL_UNSIGNED_SHORT;
	case BatchedMesh::ComponentType_UInt32:			return GL_UNSIGNED_INT;
	case BatchedMesh::ComponentType_PackedNormal:	return GL_INT_2_10_10_10_REV;
	};
	assert(0);
	return 1;
}


Reference<OpenGLMeshRenderData> GLMeshBuilding::buildBatchedMesh(VertexBufferAllocator* allocator, const Reference<BatchedMesh>& mesh_, bool skip_opengl_calls, const VBORef& instancing_matrix_data/*bool instancing*/)
{
	ZoneScoped; // Tracy profiler

	if(mesh_->index_data.empty())
		throw glare::Exception("Mesh empty.");

	Timer timer;

	const BatchedMesh* const mesh = mesh_.getPointer();

	Reference<OpenGLMeshRenderData> opengl_render_data = new OpenGLMeshRenderData();

	if(	mesh->index_type == BatchedMesh::ComponentType_UInt8 || 
		mesh->index_type == BatchedMesh::ComponentType_UInt16 ||
		mesh->index_type == BatchedMesh::ComponentType_UInt32)
	{
		opengl_render_data->setIndexType(componentTypeGLEnum(mesh->index_type));
	}
	else
		throw glare::Exception("OpenGLEngine::buildBatchedMesh(): Invalid index type.");


	// Make OpenGL batches
	opengl_render_data->batches.resize(mesh->batches.size());
	uint32 largest_material_index = 0;
	for(size_t i=0; i<opengl_render_data->batches.size(); ++i)
	{
		opengl_render_data->batches[i].material_index    = mesh->batches[i].material_index;
		opengl_render_data->batches[i].prim_start_offset_B = (uint32)(mesh->batches[i].indices_start * BatchedMesh::componentTypeSize(mesh->index_type));
		opengl_render_data->batches[i].num_indices       = mesh->batches[i].num_indices;

		largest_material_index = myMax(largest_material_index, mesh->batches[i].material_index);
	}


	// Make OpenGL vertex attributes.
	// NOTE: we need to make the opengl attributes in this particular order, with all present up to and including any attribute that is enabled.
	// The index order must be the same as for the glBindAttribLocation calls in OpenGLProgram::OpenGLProgram().

	const uint32 num_bytes_per_vert = (uint32)mesh->vertexSize();

	const BatchedMesh::VertAttribute* pos_attr			= mesh->findAttribute(BatchedMesh::VertAttribute_Position); // vertex attribute index = 0
	const BatchedMesh::VertAttribute* normal_attr		= mesh->findAttribute(BatchedMesh::VertAttribute_Normal); // 1
	const BatchedMesh::VertAttribute* uv0_attr			= mesh->findAttribute(BatchedMesh::VertAttribute_UV_0); // 2
	const BatchedMesh::VertAttribute* colour_attr		= mesh->findAttribute(BatchedMesh::VertAttribute_Colour); // 3
	const BatchedMesh::VertAttribute* uv1_attr			= mesh->findAttribute(BatchedMesh::VertAttribute_UV_1); // 4
	// 5, 6, 7, 8 - instancing
	const BatchedMesh::VertAttribute* joints_attr		= mesh->findAttribute(BatchedMesh::VertAttribute_Joints); // 9
	const BatchedMesh::VertAttribute* weights_attr		= mesh->findAttribute(BatchedMesh::VertAttribute_Weights); // 10
	const BatchedMesh::VertAttribute* tangent_attr		= mesh->findAttribute(BatchedMesh::VertAttribute_Tangent); // 11
	const BatchedMesh::VertAttribute* mat_index_attr	= mesh->findAttribute(BatchedMesh::VertAttribute_MatIndex); // 12

	if(!pos_attr)
		throw glare::Exception("Pos attribute not present.");

	// Work out the max vertex attribute index used, then only specify attributes up to that index.
	int max_attribute_index = 0;
	if(normal_attr)							max_attribute_index = 1;
	if(uv0_attr)							max_attribute_index = 2;
	if(colour_attr)							max_attribute_index = 3;
	if(uv1_attr)							max_attribute_index = 4;
	if(instancing_matrix_data.nonNull())	max_attribute_index = 8;
	if(joints_attr)							max_attribute_index = 9;
	if(weights_attr)						max_attribute_index = 10;
	if(tangent_attr)						max_attribute_index = 11;
	if(mat_index_attr)						max_attribute_index = 12;

	// NOTE: The order of these attributes should be the same as in OpenGLProgram constructor with the glBindAttribLocations.

	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = componentTypeGLEnum(pos_attr->component_type);
	pos_attrib.normalised = false;
	pos_attrib.stride = num_bytes_per_vert;
	pos_attrib.offset = (uint32)pos_attr->offset_B;
	opengl_render_data->vertex_spec.attributes.push_back(pos_attrib);

	if(max_attribute_index >= 1)
	{
		VertexAttrib normal_attrib;
		normal_attrib.enabled = normal_attr != NULL;
		normal_attrib.num_comps = 3;
		normal_attrib.type = GL_FLOAT;
		normal_attrib.normalised = false;
		if(normal_attr)
		{
			if(normal_attr->component_type == BatchedMesh::ComponentType_Float)
			{
				normal_attrib.num_comps = 3;
				normal_attrib.type = GL_FLOAT;
				normal_attrib.normalised = false;
			}
			else if(normal_attr->component_type == BatchedMesh::ComponentType_PackedNormal)
			{
				normal_attrib.num_comps = 4;
				normal_attrib.type = GL_INT_2_10_10_10_REV;
				normal_attrib.normalised = true;
			}
			else
				throw glare::Exception("Unhandled normal attr component type.");
		}
	
		normal_attrib.stride = num_bytes_per_vert;
		normal_attrib.offset = (uint32)(normal_attr ? normal_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(normal_attrib);
	}

	if(max_attribute_index >= 2)
	{
		VertexAttrib uv_attrib;
		uv_attrib.enabled = uv0_attr != NULL;
		uv_attrib.num_comps = 2;
		uv_attrib.type = uv0_attr ? componentTypeGLEnum(uv0_attr->component_type) : GL_FLOAT;
		uv_attrib.normalised = false;
		uv_attrib.stride = num_bytes_per_vert;
		uv_attrib.offset = (uint32)(uv0_attr ? uv0_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(uv_attrib);
	}

	if(max_attribute_index >= 3)
	{
		VertexAttrib colour_attrib;
		colour_attrib.enabled = colour_attr != NULL;
		colour_attrib.num_comps = 3;
		colour_attrib.type = colour_attr ? componentTypeGLEnum(colour_attr->component_type) : GL_FLOAT;
		colour_attrib.normalised = false;
		colour_attrib.stride = num_bytes_per_vert;
		colour_attrib.offset = (uint32)(colour_attr ? colour_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(colour_attrib);
	}

	if(max_attribute_index >= 4)
	{
		VertexAttrib lightmap_uv_attrib;
		lightmap_uv_attrib.enabled = uv1_attr != NULL;
		lightmap_uv_attrib.num_comps = 2;
		lightmap_uv_attrib.type = uv1_attr ? componentTypeGLEnum(uv1_attr->component_type) : GL_FLOAT;
		lightmap_uv_attrib.normalised = false;
		lightmap_uv_attrib.stride = num_bytes_per_vert;
		lightmap_uv_attrib.offset = (uint32)(uv1_attr ? uv1_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(lightmap_uv_attrib);
	}

	if(max_attribute_index >= 5)
	{
		// Add instancing matrix vert attributes, one for each vec4f comprising matrices
		for(int i = 0; i < 4; ++i)
		{
			VertexAttrib vec4_attrib;
			vec4_attrib.enabled = instancing_matrix_data.nonNull();
			vec4_attrib.num_comps = 4;
			vec4_attrib.type = GL_FLOAT;
			vec4_attrib.normalised = false;
			vec4_attrib.stride = 16 * sizeof(float); // This stride and offset is in the instancing_matrix_data VBO.
			vec4_attrib.offset = (uint32)(sizeof(float) * 4 * i);
			vec4_attrib.instancing = true;

			//vec4_attrib.vbo = instancing_matrix_data;

			opengl_render_data->vertex_spec.attributes.push_back(vec4_attrib);
		}
	}

	if(max_attribute_index >= 9)
	{
		// joints_attr
		VertexAttrib joints_attrib;
		joints_attrib.enabled = joints_attr != NULL;
		joints_attrib.num_comps = 4;
		joints_attrib.type = joints_attr ? componentTypeGLEnum(joints_attr->component_type) : GL_UNSIGNED_INT;
		joints_attrib.integer_attribute = true;
		joints_attrib.normalised = false;
		joints_attrib.stride = num_bytes_per_vert;
		joints_attrib.offset = (uint32)(joints_attr ? joints_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(joints_attrib);
	}

	if(max_attribute_index >= 10)
	{
		// weights_attr
		VertexAttrib weights_attrib;
		weights_attrib.enabled = weights_attr != NULL;
		weights_attrib.num_comps = 4;
		weights_attrib.type = weights_attr ? componentTypeGLEnum(weights_attr->component_type) : GL_FLOAT;
		weights_attrib.normalised = weights_attr ? (weights_attr->component_type != BatchedMesh::ComponentType_Float) : false;
		weights_attrib.stride = num_bytes_per_vert;
		weights_attrib.offset = (uint32)(weights_attr ? weights_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(weights_attrib);
	}

	if(max_attribute_index >= 11)
	{
		// tangent_attr
		VertexAttrib tangent_attrib;
		tangent_attrib.enabled = tangent_attr != NULL;
		tangent_attrib.num_comps = 4;
		tangent_attrib.type = tangent_attr ? componentTypeGLEnum(tangent_attr->component_type) : GL_FLOAT;
		tangent_attrib.normalised = tangent_attr ? (tangent_attr->component_type != BatchedMesh::ComponentType_Float) : false;
		tangent_attrib.stride = num_bytes_per_vert;
		tangent_attrib.offset = (uint32)(tangent_attr ? tangent_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(tangent_attrib);
	}

	if(max_attribute_index >= 12)
	{
		// mat_index_attr
		VertexAttrib mat_index_attrib;
		mat_index_attrib.enabled = mat_index_attr != NULL;
		mat_index_attrib.num_comps = 1;
		mat_index_attrib.type = mat_index_attr ? componentTypeGLEnum(mat_index_attr->component_type) : GL_UNSIGNED_INT;
		mat_index_attrib.integer_attribute = true;
		mat_index_attrib.normalised = false;
		mat_index_attrib.stride = num_bytes_per_vert;
		mat_index_attrib.offset = (uint32)(mat_index_attr ? mat_index_attr->offset_B : 0);
		opengl_render_data->vertex_spec.attributes.push_back(mat_index_attrib);
	}

	opengl_render_data->vertex_spec.checkValid();

	if(skip_opengl_calls)
	{
		// Copy data to opengl_render_data so it can be loaded later
		//opengl_render_data->vert_data = mesh->vertex_data;
		//opengl_render_data->vert_index_buffer_uint8 = mesh->index_data;

		opengl_render_data->batched_mesh = mesh_; // Hang onto a reference to the mesh, we will upload directly from it later.
	}
	else
	{
		allocator->allocateBufferSpaceAndVAO(*opengl_render_data, opengl_render_data->vertex_spec, mesh->vertex_data.data(), mesh->vertex_data.dataSizeBytes(),
			mesh->index_data.data(), mesh->index_data.dataSizeBytes());
	}


	opengl_render_data->has_uvs				= uv0_attr     != NULL;
	opengl_render_data->has_shading_normals = normal_attr  != NULL;
	opengl_render_data->has_vert_colours	= colour_attr  != NULL;
	opengl_render_data->has_vert_tangents   = tangent_attr != NULL;

	opengl_render_data->aabb_os = mesh->aabb_os;

	opengl_render_data->num_materials_referenced = largest_material_index + 1;

	return opengl_render_data;
}
