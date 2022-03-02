/*=====================================================================
VAO.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "VAO.h"


#include "VBO.h"


VAO::VAO(const VertexSpec& vertex_spec_)
:	handle(0)
{
	vertex_spec = vertex_spec_;

	// Create new VAO
	glGenVertexArrays(1, &handle);

	// Make buffer active
	glBindVertexArray(handle);

	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
		const VertexAttrib& attribute = vertex_spec.attributes[i];

		glVertexAttribFormat(
			(uint32)i, // index
			attribute.num_comps, // size - "Specifies the number of components per generic vertex attribute"
			attribute.type, // type
			attribute.normalised, // normalised
			attribute.offset // relativeoffset
		);

		if(attribute.enabled)
			glEnableVertexAttribArray((uint32)i);
		else
			glDisableVertexAttribArray((uint32)i);

		// Binding point 0: main vertex data buffer
		// Binding point 1: instancing matrix data buffer.
		const uint32 binding_point_index = attribute.instancing ? 1 : 0;
		
		glVertexAttribBinding(
			(uint32)i, // attribute index
			binding_point_index // binding index
		);

		if(vertex_spec.attributes[i].instancing)
			glVertexBindingDivisor(binding_point_index, 1);
	}
	
	glBindVertexArray(0);
}


VAO::VAO(const Reference<VBO>& vertex_data, Reference<VBO>& vert_indices_buf, const VertexSpec& vertex_spec_)
:	handle(0)
{
	vertex_spec = vertex_spec_;

	// Create new VAO
	glGenVertexArrays(1, &handle);

	// Make buffer active
	glBindVertexArray(handle);

	if(vert_indices_buf.nonNull())
		vert_indices_buf->bind(); // Bind the vertex indices buffer to this VAO.

	if(vertex_data.nonNull())
		vertex_data->bind(); // Use vertex_data by default.  This binding is stored by the glVertexAttribPointer() call. See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml

	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
	//	if(vertex_spec.attributes[i].vbo.nonNull()) // If this attribute uses its own vertex buffer, bind it
	//	{
	//		if(vertex_data.nonNull())
	//			vertex_data->unbind();
	//		vertex_spec.attributes[i].vbo->bind();
	//	}

		//glVertexAttribPointer(
		//	(uint32)i, // index
		//	vertex_spec.attributes[i].num_comps, // size - "Specifies the number of components per generic vertex attribute"
		//	vertex_spec.attributes[i].type, // type
		//	vertex_spec.attributes[i].normalised, // normalised
		//	vertex_spec.attributes[i].stride, // stride
		//	(void*)(uint64)vertex_spec.attributes[i].offset // pointer (offset)
		//);

		//TEMP:
		glVertexAttribFormat(
			(uint32)i, // index
			vertex_spec.attributes[i].num_comps, // size - "Specifies the number of components per generic vertex attribute"
			vertex_spec.attributes[i].type, // type
			vertex_spec.attributes[i].normalised, // normalised
			//vertex_spec.attributes[i].stride, // stride
			vertex_spec.attributes[i].offset // relativeoffset
		);

		if(vertex_spec.attributes[i].enabled)
			glEnableVertexAttribArray((uint32)i);
		else
			glDisableVertexAttribArray((uint32)i);

		if(vertex_spec.attributes[i].instancing)
			glVertexAttribDivisor((GLuint)i, 1);

	//	if(vertex_spec.attributes[i].vbo.nonNull())
	//	{
	//		vertex_spec.attributes[i].vbo->unbind();
	//		if(vertex_data.nonNull())
	//			vertex_data->bind();
	//	}
	}
	
	if(vertex_data.nonNull())
		vertex_data->unbind();
	
	glBindVertexArray(0);
}


VAO::~VAO()
{
	glDeleteVertexArrays(1, &handle);
}


void VAO::bindVertexArray() const
{
	// Make buffer active
	glBindVertexArray(handle);
}


void VAO::unbind()
{
	// Unbind buffer
	glBindVertexArray(0);
}


void VAO::bindVertexBuffer(const VBO& vertex_data)
{
	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
		glBindVertexBuffer(
			(GLuint)i, // binding index
			vertex_data.bufferName(), // buffer
			0, // offset - offset of the first element within the buffer
			vertex_spec.attributes[i].stride // stride
		);
	}
}

