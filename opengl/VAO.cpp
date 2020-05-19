/*=====================================================================
VAO.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "VAO.h"


#include "VBO.h"


VAO::VAO(const Reference<VBO>& vertex_data, const VertexSpec& vertex_spec)
:	handle(0)
{
	// Create new VAO
	glGenVertexArrays(1, &handle);

	// Make buffer active
	glBindVertexArray(handle);

	// Upload vertex data to the video device
	//glBufferData(buffer_type, size, data, GL_STATIC_DRAW);

	// Unbind buffer
	//glBindBuffer(buffer_type, 0);

	vertex_data->bind(); // Use vertex_data by default.
	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
		if(vertex_spec.attributes[i].vbo.nonNull()) // If this attribute uses its own vertex buffer, bind it
		{
			vertex_data->unbind();
			vertex_spec.attributes[i].vbo->bind();
		}

		glVertexAttribPointer(
			(uint32)i, // index
			vertex_spec.attributes[i].num_comps, // size - "Specifies the number of components per generic vertex attribute"
			vertex_spec.attributes[i].type, // type
			vertex_spec.attributes[i].normalised, // normalised
			vertex_spec.attributes[i].stride, // stride
			(void*)(uint64)vertex_spec.attributes[i].offset // pointer (offset)
		);

		if(vertex_spec.attributes[i].enabled)
			glEnableVertexAttribArray((uint32)i);
		else
			glDisableVertexAttribArray((uint32)i);

		if(vertex_spec.attributes[i].instancing)
			glVertexAttribDivisor((GLuint)i, 1);

		if(vertex_spec.attributes[i].vbo.nonNull())
		{
			vertex_spec.attributes[i].vbo->unbind();
			vertex_data->bind();
		}
	}
	vertex_data->unbind();
	glBindVertexArray(0);
}


VAO::~VAO()
{
	glDeleteVertexArrays(1, &handle);
}


void VAO::bind()
{
	// Make buffer active
	glBindVertexArray(handle);
}


void VAO::unbind()
{
	// Unbind buffer
	glBindVertexArray(0);
}
