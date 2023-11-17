/*=====================================================================
MeshPrimitiveBuilding.h
-----------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../utils/Reference.h"
class Vec4f;
class Colour4f;
struct GLObject;
class OpenGLMeshRenderData;
class VertexBufferAllocator;


/*=====================================================================
MeshPrimitiveBuilding
---------------------
=====================================================================*/
class MeshPrimitiveBuilding
{
public:
	static Reference<OpenGLMeshRenderData> makeLineMesh(VertexBufferAllocator& allocator);
	static Reference<OpenGLMeshRenderData> makeSphereMesh(VertexBufferAllocator& allocator);
	static Reference<OpenGLMeshRenderData> makeCubeMesh(VertexBufferAllocator& allocator);
	static Reference<OpenGLMeshRenderData> makeQuadMesh(VertexBufferAllocator& allocator, const Vec4f& i, const Vec4f& j, int vert_res); // Has vert_res^2 vertices.  Require vert_res >= 2.
	static Reference<OpenGLMeshRenderData> makeUnitQuadMesh(VertexBufferAllocator& allocator); // Makes a quad from (0, 0, 0) to (1, 1, 0)
	static Reference<OpenGLMeshRenderData> makeCapsuleMesh(VertexBufferAllocator& allocator, const Vec3f& bottom_spans, const Vec3f& top_spans);
	static Reference<OpenGLMeshRenderData> makeCylinderMesh(VertexBufferAllocator& allocator, bool end_caps); // Make a cylinder from (0,0,0), to (0,0,1) with radius 1;
	static Reference<OpenGLMeshRenderData> make3DArrowMesh(VertexBufferAllocator& allocator);
	static Reference<OpenGLMeshRenderData> make3DBasisArrowMesh(VertexBufferAllocator& allocator);
	static Reference<GLObject> makeDebugHexahedron(VertexBufferAllocator& allocator, const Vec4f* verts_ws, const Colour4f& col);
	static Reference<OpenGLMeshRenderData> makeRoundedCornerRect(VertexBufferAllocator& allocator, const Vec4f& i, const Vec4f& j, float w, float h, float corner_radius, int tris_per_corner);
	static Reference<OpenGLMeshRenderData> makeSpriteQuad(VertexBufferAllocator& allocator);
};
