/*=====================================================================
OpenGLMeshRenderData.cpp
------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLMeshRenderData.h"


#include "IncludeOpenGL.h"
#include "../graphics/BatchedMesh.h"


static inline uint32 indexTypeSizeBytes(GLenum index_type)
{
	if(index_type == GL_UNSIGNED_INT)
		return 4;
	else if(index_type == GL_UNSIGNED_SHORT)
		return 2;
	else
	{
		assert(index_type == GL_UNSIGNED_BYTE);
		return 1;
	}
}


GLMemUsage OpenGLMeshRenderData::getTotalMemUsage() const
{
	GLMemUsage usage;
	usage.geom_cpu_usage =
		vert_data.capacitySizeBytes() +
		vert_index_buffer.capacitySizeBytes() +
		vert_index_buffer_uint16.capacitySizeBytes() +
		vert_index_buffer_uint8.capacitySizeBytes() +
		(batched_mesh.nonNull() ? batched_mesh->getTotalMemUsage() : 0) + 
		animation_data.getTotalMemUsage();

	usage.geom_gpu_usage =
		(this->vbo_handle.valid() ? this->vbo_handle.size : 0) +
		(this->indices_vbo_handle.valid() ? this->indices_vbo_handle.size : 0);

	return usage;
}


size_t OpenGLMeshRenderData::GPUVertMemUsage() const
{
	return this->vbo_handle.valid() ? this->vbo_handle.size : 0;
}


size_t OpenGLMeshRenderData::GPUIndicesMemUsage() const
{
	return this->indices_vbo_handle.valid() ? this->indices_vbo_handle.size : 0;
}


size_t OpenGLMeshRenderData::getNumVerts() const
{
	if(!vertex_spec.attributes.empty() && (vertex_spec.attributes[0].stride > 0))
	{
		if(!vert_data.empty())
			return vert_data.size() / vertex_spec.attributes[0].stride;
		else if(this->vbo_handle.valid())
			return vbo_handle.size / vertex_spec.attributes[0].stride;
	}
	return 0;
}


size_t OpenGLMeshRenderData::getVertStrideB() const
{
	if(!vertex_spec.attributes.empty())
		return vertex_spec.attributes[0].stride;
	else
		return 0;
}


size_t OpenGLMeshRenderData::getNumTris() const
{
	if(!vert_index_buffer.empty())
		return vert_index_buffer.size() / 3;
	else if(!vert_index_buffer_uint16.empty())
		return vert_index_buffer_uint16.size() / 3;
	else if(!vert_index_buffer_uint8.empty())
		return vert_index_buffer_uint8.size() / 3;
	else if(indices_vbo_handle.valid())
	{
		const size_t index_type_size_B = indexTypeSizeBytes(index_type);
		return indices_vbo_handle.size / index_type_size_B / 3;
	}
	else
		return 0;
}


void OpenGLMeshRenderData::getVertAndIndexArrayRefs(ArrayRef<uint8>& vert_data_out, ArrayRef<uint8>& index_data_out)
{
	if(batched_mesh)
	{
		vert_data_out  = batched_mesh->vertex_data;
		index_data_out = batched_mesh->index_data;
	}
	else
	{
		vert_data_out = vert_data;

		if(!vert_index_buffer.empty())
			index_data_out = ArrayRef<uint8>((const uint8*)vert_index_buffer       .data(), vert_index_buffer       .dataSizeBytes());
		else if(!vert_index_buffer_uint16.empty())
			index_data_out = ArrayRef<uint8>((const uint8*)vert_index_buffer_uint16.data(), vert_index_buffer_uint16.dataSizeBytes());
		else if(!vert_index_buffer_uint8.empty())
			index_data_out = ArrayRef<uint8>((const uint8*)vert_index_buffer_uint8 .data(), vert_index_buffer_uint8 .dataSizeBytes());
	}
}


void OpenGLMeshRenderData::clearAndFreeGeometryMem()
{
	if(batched_mesh)
	{
		batched_mesh->vertex_data.clearAndFreeMem();
		batched_mesh->index_data.clearAndFreeMem();
	}
	else
	{
		vert_data.clearAndFreeMem();
		vert_index_buffer.clearAndFreeMem();
		vert_index_buffer_uint16.clearAndFreeMem();
		vert_index_buffer_uint8.clearAndFreeMem();
	}
}
