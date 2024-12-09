/*=====================================================================
VertexBufferAllocator.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VertexBufferAllocator.h"


#include "IncludeOpenGL.h"
#include "OpenGLMeshRenderData.h"
#include "../graphics/BatchedMesh.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../maths/mathstypes.h"


VertexBufferAllocator::VertexBufferAllocator(bool use_grouped_vbo_allocator_)
:	use_grouped_vbo_allocator(use_grouped_vbo_allocator_), 
	use_VBO_size_B(64 * 1024 * 1024) // Default VBO size to use, will be overridden in OpenGLEngine::initialise() though.
{
	//conPrint("VertexBufferAllocator::VertexBufferAllocator()");
}


VertexBufferAllocator::~VertexBufferAllocator()
{
	//conPrint("VertexBufferAllocator::~VertexBufferAllocator()");
}


void VertexBufferAllocator::allocateBufferSpaceAndVAO(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec, const void* vert_data, size_t vert_data_size_B, 
	const void* index_data, size_t index_data_size_B)
{
	mesh_data_in_out.vbo_handle = allocateVertexDataSpace(vertex_spec.vertStride(), vert_data, vert_data_size_B);

	mesh_data_in_out.indices_vbo_handle = allocateIndexDataSpace(index_data, index_data_size_B);

	getOrCreateAndAssignVAOForMesh(mesh_data_in_out, vertex_spec);
}


void VertexBufferAllocator::getOrCreateAndAssignVAOForMesh(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec)
{
	vertex_spec.checkValid();

#if DO_INDIVIDUAL_VAO_ALLOC
	mesh_data_in_out.individual_vao = new VAO(mesh_data_in_out.vbo_handle.vbo, mesh_data_in_out.indices_vbo_handle.index_vbo, vertex_spec);
#else

	VAOKey key;
	key.vertex_spec = vertex_spec;

	auto res = vao_map.find(key);
	size_t use_vao_data_index;
	if(res == vao_map.end())
	{
		// This is a new vertex specification we don't have a VAO for.

		use_vao_data_index = vao_data.size();

		VAOData new_data;
		new_data.vao = new VAO(vertex_spec); // Allocate new VAO
		vao_data.push_back(new_data);

		vao_map.insert(std::make_pair(key, (int)use_vao_data_index));
	}
	else
		use_vao_data_index = res->second;

	mesh_data_in_out.vao_data_index = (uint32)use_vao_data_index;

#endif // end if !DO_INDIVIDUAL_VAO_ALLOC
}


VertBufAllocationHandle VertexBufferAllocator::allocateVertexDataSpace(size_t vert_stride, const void* vbo_data, size_t size)
{
#if DO_INDIVIDUAL_VAO_ALLOC
	// This is for the Mac, that can't easily do VAO sharing due to having to use glVertexAttribPointer().
	VertBufAllocationHandle handle;
	handle.vbo = new VBO(vbo_data, size);
	handle.vbo_id = 0;
	handle.offset = 0;
	handle.size = size;
	handle.base_vertex = 0;
	return handle;

#else // else if !DO_INDIVIDUAL_VAO_ALLOC:

	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
		VertBufAllocationHandle handle;
		handle.vbo = new VBO(vbo_data, size);
		handle.vbo_id = 0;
		handle.offset = 0;
		handle.size = size;
		handle.base_vertex = 0;
		return handle;
	}
	else
	{
		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO
		VBORef used_vbo;
		size_t used_vbo_id = 0;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(size_t i=0; i<vert_vbos.size(); ++i)
		{
			glare::BestFitAllocator::BlockInfo* block = vert_vbos[i].allocator->alloc(size, vert_stride);
			if(block)
			{
				used_vbo = vert_vbos[i].vbo;
				used_vbo_id = i;
				used_block = block;
				break;
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new VBO! (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, /*buffer_type=*/GL_ARRAY_BUFFER, /*usage=*/GL_DYNAMIC_DRAW); 
			// Since we will be updating chunks of the VBO, we will make it GL_DYNAMIC_DRAW instead of GL_STATIC_DRAW, otherwise will get OpenGL error/warning messages about updating a GL_STATIC_DRAW buffer.
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vbo_and_alloc.allocator->name = "VBO allocator";
			vert_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, vert_stride);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_vbo_id = vert_vbos.size() - 1;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new VBO.");
		}
		//-------------------------------------------------------------------------------


		if(vbo_data)
			used_vbo->updateData(used_block->aligned_offset, vbo_data, size);


		const int base_vertex = (int)(used_block->aligned_offset / vert_stride);

		VertBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(used_block);
		runtimeCheck(handle.block_handle->block->aligned_offset % vert_stride == 0);
		handle.vbo = used_vbo;
		handle.vbo_id = used_vbo_id;
		handle.offset = handle.block_handle->block->aligned_offset;
		handle.size = size;
		handle.base_vertex = base_vertex;

		return handle;
	}
#endif // end if !DO_INDIVIDUAL_VAO_ALLOC
}


IndexBufAllocationHandle VertexBufferAllocator::allocateIndexDataSpace(const void* data, size_t size)
{
#if DO_INDIVIDUAL_VAO_ALLOC

	IndexBufAllocationHandle handle;
	handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
	handle.vbo_id = 0;
	handle.offset = 0;
	handle.size = size;
	return handle;

#else
	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
		IndexBufAllocationHandle handle;
		handle.offset = 0;
		handle.size = size;
		handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
		handle.vbo_id = 0;
		return handle;
	}
	else
	{
		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO
		VBORef used_vbo;
		size_t used_vbo_id = 0;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(size_t i=0; i<index_vbos.size(); ++i)
		{
			glare::BestFitAllocator::BlockInfo* block = index_vbos[i].allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = index_vbos[i].vbo;
				used_vbo_id = i;
				used_block = block;
				break;
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new index VBO!  (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, GL_ELEMENT_ARRAY_BUFFER, /*usage=*/GL_DYNAMIC_DRAW);
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vbo_and_alloc.allocator->name = "index VBO allocator";
			index_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_vbo_id = index_vbos.size() - 1;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new index VBO");
		}
		//-------------------------------------------------------------------------------


		IndexBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(used_block);
		runtimeCheck(handle.block_handle->block->aligned_offset % 4 == 0);
		handle.offset = handle.block_handle->block->aligned_offset;
		handle.size = size;
		handle.index_vbo = used_vbo;// indices_vbo;
		handle.vbo_id = used_vbo_id;

		if(data != NULL)
			used_vbo->updateData(handle.block_handle->block->aligned_offset, data, size);

		return handle;
	}
#endif
}


std::string VertexBufferAllocator::getDiagnostics() const
{
	std::string s;
	s += "VAOs: " + toString(vao_data.size()) + "\n";
	s += "use_VBO_size: " + getNiceByteSize(use_VBO_size_B) + "\n";
	s += "Vert VBOs: " + toString(vert_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * vert_vbos.size()) + ")\n";
	s += "Index VBOs: " + toString(vert_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * vert_vbos.size()) + ")\n";
	return s;
}
