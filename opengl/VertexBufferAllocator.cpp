/*=====================================================================
VertexBufferAllocator.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VertexBufferAllocator.h"


#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/Check.h"
#include "../utils/ConPrint.h"
#include "../maths/mathstypes.h"


const static bool USE_INDIVIDUAL_VBOS = false;


VertexBufferAllocator::VertexBufferAllocator()
:	use_VBO_size_B(128 * 1024 * 1024)
{
}


VertexBufferAllocator::~VertexBufferAllocator()
{
}


VertBufAllocationHandle VertexBufferAllocator::allocate(const VertexSpec& vertex_spec, const void* vbo_data, size_t size)
{
#if DO_INDIVIDUAL_VAO_ALLOC

	VertBufAllocationHandle handle;
	handle.vbo = new VBO(vbo_data, size);
	handle.per_spec_data_index = 0;
	handle.offset = 0;
	handle.size = size;
	handle.base_vertex = 0;
	return handle;

#else

	if(USE_INDIVIDUAL_VBOS)
	{
		auto res = per_spec_data_index.find(vertex_spec);
		size_t index;
		if(res == per_spec_data_index.end())
		{
			// This is a new vertex specification we don't have a VAO for.

			index = per_spec_data.size();

			PerSpecData new_data;
			new_data.vao = new VAO(vertex_spec);
			new_data.next_offset = 0;
			per_spec_data.push_back(new_data);

			per_spec_data_index.insert(std::make_pair(vertex_spec, (int)index));
		}
		else
			index = res->second;

		VertBufAllocationHandle handle;

		// Individual buffers:
		handle.vbo = new VBO(vbo_data, size);
		handle.per_spec_data_index = index;
		handle.offset = 0;
		handle.size = size;
		handle.base_vertex = 0;
		return handle;
	}
	else // else if !USE_INDIVIDUAL_VBOS:
	{
		auto res = per_spec_data_index.find(vertex_spec);
		size_t index;
		if(res == per_spec_data_index.end())
		{
			// This is a new vertex specification we don't have a VAO for.

			index = per_spec_data.size();

			PerSpecData new_data;
			new_data.vao = new VAO(vertex_spec);
			new_data.next_offset = 0;
			per_spec_data.push_back(new_data);

			per_spec_data_index.insert(std::make_pair(vertex_spec, (int)index));
		}
		else
			index = res->second;

		PerSpecData& data = per_spec_data[index];

		const size_t vert_stride = data.vao->vertex_spec.attributes[0].stride;
		


		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO
		VBORef used_vbo;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(size_t i=0; i<vert_vbos.size(); ++i)
		{
			glare::BestFitAllocator::BlockInfo* block = vert_vbos[i].allocator->alloc(size, vert_stride);
			if(block)
			{
				used_vbo = vert_vbos[i].vbo;
				used_block = block;
				break;
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new VBO! (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B);
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vert_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, vert_stride);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate VBO with size " + toString(size) + " B");
		}
		//-------------------------------------------------------------------------------



		VertBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(used_block);
		doRuntimeCheck(handle.block_handle->block->aligned_offset % vert_stride == 0);
		handle.vbo = used_vbo;
		handle.per_spec_data_index = index;
		handle.offset = handle.block_handle->block->aligned_offset;
		handle.size = size;
		handle.base_vertex = (int)(handle.block_handle->block->aligned_offset / vert_stride);

		used_vbo->updateData(handle.block_handle->block->aligned_offset, vbo_data, size);

		return handle;
	}
#endif
}


IndexBufAllocationHandle VertexBufferAllocator::allocateIndexData(const void* data, size_t size)
{
#if DO_INDIVIDUAL_VAO_ALLOC

	IndexBufAllocationHandle handle;
	handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
	handle.offset = 0;
	handle.size = size;
	return handle;

#else

	if(USE_INDIVIDUAL_VBOS)
	{
		IndexBufAllocationHandle handle;
		handle.offset = 0;
		handle.size = size;
		handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
		return handle;
	}
	else
	{
		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO
		VBORef used_vbo;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(size_t i=0; i<index_vbos.size(); ++i)
		{
			glare::BestFitAllocator::BlockInfo* block = index_vbos[i].allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = index_vbos[i].vbo;
				used_block = block;
				break;
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new index VBO!  (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, GL_ELEMENT_ARRAY_BUFFER);
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			index_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate index VBO with size " + toString(size) + " B");
		}
		//-------------------------------------------------------------------------------


		IndexBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(used_block);
		doRuntimeCheck(handle.block_handle->block->aligned_offset % 4 == 0);
		handle.offset = handle.block_handle->block->aligned_offset;
		handle.size = size;
		handle.index_vbo = used_vbo;// indices_vbo;

		used_vbo->updateData(handle.block_handle->block->aligned_offset, data, size);

		return handle;
	}
#endif
}
