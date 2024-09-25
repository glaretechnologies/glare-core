/*=====================================================================
VAO.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "VAO.h"


#include "IncludeOpenGL.h"
#include "VBO.h"
#include "VertexBufferAllocator.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"


VAO::VAO(const VertexSpec& vertex_spec_)
:	handle(0),
	current_bound_vert_vbo(NULL),
	current_bound_index_VBO(NULL)
{
#if defined(OSX) || defined(EMSCRIPTEN)
	assert(0);
#else
	vertex_spec = vertex_spec_;

	// Create new VAO
	glGenVertexArrays(1, &handle);

	// Make VAO active
	glBindVertexArray(handle);

	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
		const VertexAttrib& attribute = vertex_spec.attributes[i];

		if(attribute.type == GL_UNSIGNED_BYTE || attribute.type == GL_UNSIGNED_SHORT || attribute.type == GL_UNSIGNED_INT ||
			attribute.type == GL_BYTE || attribute.type == GL_SHORT || attribute.type == GL_INT)
		{
			glVertexAttribIFormat(
				(uint32)i, // index
				attribute.num_comps, // size - "Specifies the number of components per generic vertex attribute"
				attribute.type, // type
				attribute.offset // relative offset
			);
		}
		else
		{
			glVertexAttribFormat(
				(uint32)i, // index
				attribute.num_comps, // size - "Specifies the number of components per generic vertex attribute"
				attribute.type, // type
				attribute.normalised, // normalised
				attribute.offset // relative offset
			);
		}

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
#endif
}


// Constructor that uses old-style glVertexAttribPointer().  Just kept around for Mac and WebGL / Emscripten.
VAO::VAO(const Reference<VBO>& vertex_data, const Reference<VBO>& vert_indices_buf, const VertexSpec& vertex_spec_)
:	handle(0),
	current_bound_vert_vbo(NULL),
	current_bound_index_VBO(NULL)
{
	assert(vertex_data.nonNull());
#if DO_INDIVIDUAL_VAO_ALLOC
	assert(vert_indices_buf.nonNull());
#endif

	vertex_spec = vertex_spec_;

	// Create new VAO
	glGenVertexArrays(1, &handle);

	// Make buffer active
	glBindVertexArray(handle);

	if(vert_indices_buf.nonNull())
		vert_indices_buf->bind(); // Bind the vertex indices buffer to this VAO.

	this->current_bound_index_VBO = vert_indices_buf.ptr();

	if(vertex_data.nonNull())
		vertex_data->bind(); // Use vertex_data by default.  This binding is stored by the glVertexAttribPointer() call. See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml

	this->current_bound_vert_vbo = vertex_data.ptr();

	//conPrint("---------------------");
	for(size_t i=0; i<vertex_spec.attributes.size(); ++i)
	{
		if(vertex_spec.attributes[i].vbo.nonNull()) // If this attribute uses its own vertex buffer, bind it
		{
			if(vertex_data.nonNull())
				vertex_data->unbind();
			vertex_spec.attributes[i].vbo->bind();
		}

		//conPrint("attribute " + toString(i) + ", num_comps: " + toString(vertex_spec.attributes[i].num_comps) + ", type: " + toHexString(vertex_spec.attributes[i].type) + ", stride: " + toString(vertex_spec.attributes[i].stride) + ", offset: " + 
		//	toString((uint64)vertex_spec.attributes[i].offset) + ", enabled: " + boolToString(vertex_spec.attributes[i].enabled));

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
			if(vertex_data.nonNull())
				vertex_data->bind();
		}
	}
	
	if(vertex_data.nonNull())
		vertex_data->unbind();

	assert(getBoundVertexBuffer(0) == vertex_data->bufferName());
	if(vert_indices_buf.nonNull())
		assert(getBoundIndexBuffer() == vert_indices_buf->bufferName());
	
	glBindVertexArray(0);
} 


VAO::~VAO()
{
	glDeleteVertexArrays(1, &handle);
}


void VAO::bindVertexArray() const
{
	// Make VAO active
	glBindVertexArray(handle);
}


void VAO::unbind()
{
	// Unbind VAO
	glBindVertexArray(0);
}


GLuint VAO::getBoundVAO()
{
	GLuint id;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint*)&id);
	return id;
}


GLuint VAO::getBoundVertexBuffer(GLint attribute_index) const
{
	glBindVertexArray(handle);

	GLuint id;
	glGetVertexAttribIuiv(attribute_index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &id);
	return id;
}


GLuint VAO::getBoundIndexBuffer() const
{
	glBindVertexArray(handle);

	GLuint id;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint*)&id);
	return id;
}


void VertexSpec::checkValid() const
{
	// Check vertex spec: check data is aligned properly.
	// This is a requirement for WebGL (see https://github.com/KhronosGroup/WebGL/issues/914) and may be for Desktop OpenGL as well.
#ifndef NDEBUG
	for(size_t i=0; i<attributes.size(); ++i)
	{
		const VertexAttrib& attr = attributes[i];
		size_t data_type_size;
		if(attr.type == GL_FLOAT)
			data_type_size = 4;
		else if(attr.type == GL_INT_2_10_10_10_REV)
			data_type_size = 4;
		else if(attr.type == GL_HALF_FLOAT)
			data_type_size = 2;
		else if(attr.type == GL_BYTE)
			data_type_size = 1;
		else if(attr.type == GL_SHORT)
			data_type_size = 2;
		else if(attr.type == GL_UNSIGNED_SHORT)
			data_type_size = 2;
		else if(attr.type == GL_UNSIGNED_INT)
			data_type_size = 4;
		else
		{
			assert(0);
			data_type_size = 4;
		}

		assert((attr.offset % data_type_size) == 0);
		if((attr.offset % data_type_size) != 0)
			conPrint("======================== ERROR: attribute offset unaligned");

		assert((attr.stride % data_type_size) == 0);
		if((attr.stride % data_type_size) != 0)
			conPrint("======================== ERROR: attribute stride not a multiple of datatype size");
	}
#endif
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


static VertexAttrib makeTestAttrib()
{
	VertexAttrib attr;
	attr.enabled = true;
	attr.num_comps = 3;
	attr.type = GL_FLOAT;
	attr.normalised = false;
	attr.stride = 12;
	attr.offset = 0;
	return attr;
}


void VAO::test()
{
	//------------------------------ Test VertexSpec ------------------------------
	{
		VertexSpec spec_a;
		spec_a.attributes.push_back(makeTestAttrib());

		VertexSpec spec_b;
		spec_b.attributes.push_back(makeTestAttrib());
		
		testAssert(!(spec_a < spec_b));
		testAssert(!(spec_b < spec_a));

		spec_b.attributes[0].enabled = false;

		testAssert(spec_b < spec_a);
		testAssert(!(spec_a < spec_b));
	}
	{
		VertexSpec spec_a;
		spec_a.attributes.push_back(makeTestAttrib());

		VertexSpec spec_b;
		spec_b.attributes.push_back(makeTestAttrib());
		spec_b.attributes.push_back(makeTestAttrib());
		
		testAssert(spec_a < spec_b);
		testAssert(!(spec_b < spec_a));
	}
}


#endif // BUILD_TESTS
