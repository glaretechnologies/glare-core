/*=====================================================================
VAO.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Platform.h"
class VBO;


struct VertexAttrib
{
	bool enabled;
	int num_comps;
	GLenum type;
	bool normalised;
	uint32 stride;
	uint32 offset;
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
	VAO(const Reference<VBO>& vertex_data, const VertexSpec& vertex_spec);
	~VAO();

	void bind();
	void unbind();

private:
	INDIGO_DISABLE_COPY(VAO)

	GLuint handle;

};


typedef Reference<VAO> VAORef;
