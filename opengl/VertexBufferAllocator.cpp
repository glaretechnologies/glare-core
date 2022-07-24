/*=====================================================================
VertexBufferAllocator.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VertexBufferAllocator.h"


#include "IncludeOpenGL.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/Check.h"
#include "../utils/ConPrint.h"
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


VertBufAllocationHandle VertexBufferAllocator::allocate(const VertexSpec& vertex_spec, const void* vbo_data, size_t size)
{
#if DO_INDIVIDUAL_VAO_ALLOC
	// This is for the Mac, that can't easily do VAO sharing due to having to use glVertexAttribPointer().
	VertBufAllocationHandle handle;
	handle.vbo = new VBO(vbo_data, size);
	handle.per_spec_data_index = 0;
	handle.offset = 0;
	handle.size = size;
	handle.base_vertex = 0;
	return handle;

#else

	auto res = per_spec_data_index.find(vertex_spec);
	size_t use_per_spec_data_index;
	if(res == per_spec_data_index.end())
	{
		// This is a new vertex specification we don't have a VAO for.

		use_per_spec_data_index = per_spec_data.size();

		PerSpecData new_data;
		new_data.vao = new VAO(vertex_spec);
		new_data.next_offset = 0;
		per_spec_data.push_back(new_data);

		per_spec_data_index.insert(std::make_pair(vertex_spec, (int)use_per_spec_data_index));
	}
	else
		use_per_spec_data_index = res->second;

	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
		VertBufAllocationHandle handle;
		handle.vbo = new VBO(vbo_data, size);
		handle.per_spec_data_index = use_per_spec_data_index;
		handle.offset = 0;
		handle.size = size;
		handle.base_vertex = 0;
		return handle;
	}
	else
	{
		PerSpecData& data = per_spec_data[use_per_spec_data_index];

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
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new VBO.");
		}
		//-------------------------------------------------------------------------------



		VertBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(used_block);
		doRuntimeCheck(handle.block_handle->block->aligned_offset % vert_stride == 0);
		handle.vbo = used_vbo;
		handle.per_spec_data_index = use_per_spec_data_index;
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
	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
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
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, GL_ELEMENT_ARRAY_BUFFER, /*usage=*/GL_DYNAMIC_DRAW);
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vbo_and_alloc.allocator->name = "index VBO allocator";
			index_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new index VBO");
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


std::string VertexBufferAllocator::getDiagnostics() const
{
	std::string s;
	s += "use_VBO_size: " + getNiceByteSize(use_VBO_size_B) + "\n";
	s += "Vert VBOs: " + toString(vert_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * vert_vbos.size()) + ")\n";
	s += "Index VBOs: " + toString(vert_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * vert_vbos.size()) + ")\n";
	return s;
}
