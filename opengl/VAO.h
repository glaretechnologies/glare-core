/*=====================================================================
VAO.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
#include <vector>
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

	Reference<VBO> vbo; // VBO to be bound for this attribute.  Can be left to NULL in which case the usual mesh data vert_vbo will be used.
};


struct VertexSpec
{
	std::vector<VertexAttrib> attributes;
};


/*=====================================================================
VAO
---
Vertex array object
=====================================================================*/
class VAO : public RefCounted
{
public:
	VAO(const Reference<VBO>& vertex_data, Reference<VBO>& vert_indices_buf, const VertexSpec& vertex_spec);
	~VAO();

	void bind();
	static void unbind();

private:
	GLARE_DISABLE_COPY(VAO)

	GLuint handle;

};


typedef Reference<VAO> VAORef;
