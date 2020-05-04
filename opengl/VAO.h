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
};


struct VertexSpec
{
	std::vector<VertexAttrib> attributes;
};


/*=====================================================================
VAO
-------------------
Vertex array object
=====================================================================*/
class VAO : public RefCounted
{
public:
	VAO(const Reference<VBO>& vertex_data, const Reference<VBO>& instance_data, const VertexSpec& vertex_spec);
	~VAO();

	void bind();
	void unbind();

private:
	INDIGO_DISABLE_COPY(VAO)

	GLuint handle;

};


typedef Reference<VAO> VAORef;
