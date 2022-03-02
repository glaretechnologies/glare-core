/*=====================================================================
VertexBufferAllocator.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VertexBufferAllocator.h"


#include "../utils/Exception.h"
#include "../maths/mathstypes.h"


const static bool USE_INDIVIDUAL_VBOS = true;


VertexBufferAllocator::VertexBufferAllocator()
{
	//next_offset = 0;
	//vbo = new VBO(NULL, 512 * 1024 * 1024);

	/*indices_vbo = new VBO(NULL, 256 * 1024 * 1024, GL_ELEMENT_ARRAY_BUFFER);
	next_indices_offset = 0;*/
	next_indices_offset = 0;
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
			new_data.vbo = NULL;
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
		if(main_vbo.isNull())
		{
			main_vbo = new VBO(NULL, 512 * 1024 * 1024);
			main_vbo_offset = 0;
		}

		auto res = per_spec_data_index.find(vertex_spec);
		size_t index;
		if(res == per_spec_data_index.end())
		{
			// This is a new vertex specification we don't have a VAO for.

			index = per_spec_data.size();

			PerSpecData new_data;
			new_data.vbo = main_vbo;// new VBO(NULL, 16 * 1024 * 1024);
			new_data.vao = new VAO(vertex_spec);
			new_data.next_offset = 0;
			per_spec_data.push_back(new_data);

			per_spec_data_index.insert(std::make_pair(vertex_spec, (int)index));
		}
		else
			index = res->second;

		PerSpecData& data = per_spec_data[index];

		const size_t vert_stride = data.vao->vertex_spec.attributes[0].stride;
		main_vbo_offset = Maths::roundUpToMultiple(main_vbo_offset, vert_stride); // We need our offset in the vert buffer to be a multiple of the vertex stride.

		if(main_vbo_offset/*data.next_offset*/ + size <= data.vbo->getSize())
		{
			VertBufAllocationHandle handle;

			handle.vbo = data.vbo;
			handle.per_spec_data_index = index;
			handle.offset = main_vbo_offset;// data.next_offset;
			handle.size = size;
			handle.base_vertex = (int)(main_vbo_offset / vert_stride);
			
			data.vbo->updateData(
				main_vbo_offset, /*data.next_offset*/
				vbo_data,
				size
			);

			main_vbo_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(main_vbo_offset + size, 16);
			//data.next_offset += size;

			return handle;
		}
		else
			throw glare::Exception("VertexBufferAllocator: allocation failed");
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
		if(indices_vbo.isNull())
		{
			indices_vbo = new VBO(NULL, 256 * 1024 * 1024, GL_ELEMENT_ARRAY_BUFFER);
			next_indices_offset = 0;
		}

		if(next_indices_offset + size < indices_vbo->getSize())
		{
			IndexBufAllocationHandle handle;
			handle.offset = next_indices_offset;
			handle.size = size;
			handle.index_vbo = indices_vbo;

			indices_vbo->updateData(next_indices_offset, data, size);

			next_indices_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(next_indices_offset + size, 16);

			return handle;
		}
		else
			throw glare::Exception("VertexBufferAllocator: index allocation failed");
	}
#endif
}
