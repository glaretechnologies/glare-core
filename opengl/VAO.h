/*=====================================================================
VAO.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <vector>
#include <stdlib.h>
class VBO;


struct VertexAttrib
{
	VertexAttrib() : instancing(false) {}
	bool enabled;
	int num_comps;
	GLenum type;
	bool normalised;
	uint32 stride;
	uint32 offset;
	bool instancing;

	Reference<VBO> vbo; // VBO to be bound for this attribute.  Can be left to NULL in which case the usual mesh data vert_vbo will be used.  Only used on Mac.

	bool operator < (const VertexAttrib& other) const
	{
		if(!enabled && other.enabled)
			return true;
		if(!other.enabled && enabled)
			return false;

		if(num_comps < other.num_comps)
			return true;
		else if(other.num_comps < num_comps)
			return false;

		if(type < other.type)
			return true;
		else if(other.type < type)
			return false;

		if(!normalised && other.normalised)
			return true;
		if(!other.normalised && normalised)
			return false;

		if(offset < other.offset)
			return true;
		else if(other.offset < offset)
			return false;

		if(!instancing && other.instancing)
			return true;
		if(!other.instancing && instancing)
			return false;

		return false;
	}
};


struct VertexSpec
{
	std::vector<VertexAttrib> attributes;

	bool operator < (const VertexSpec& other) const
	{
		if(attributes.size() < other.attributes.size())
			return true;
		else if(attributes.size() > other.attributes.size())
			return false;
		else
		{
			for(size_t i=0; i<attributes.size(); ++i)
			{
				if(attributes[i] < other.attributes[i])
					return true;
				else if(other.attributes[i] < attributes[i])
					return false;
			}
		}

		return false;
	}
};


/*=====================================================================
VAO
---
Vertex array object
=====================================================================*/
class VAO : public RefCounted
{
public:
	VAO(const VertexSpec& vertex_spec);
	VAO(const Reference<VBO>& vertex_data, Reference<VBO>& vert_indices_buf, const VertexSpec& vertex_spec);
	~VAO();

	void bindVertexArray() const;
	static void unbind();

	static GLuint getBoundVAO();

	GLuint getBoundVertexBuffer(GLint attribute_index) const;
	GLuint getBoundIndexBuffer() const;

	static void test();

private:
	GLARE_DISABLE_COPY(VAO)

public:
	GLuint handle;
	VertexSpec vertex_spec;

	const VBO* current_bound_vert_vbo;
	const VBO* current_bound_index_VBO;
};


typedef Reference<VAO> VAORef;
