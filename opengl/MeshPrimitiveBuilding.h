/*=====================================================================
MeshPrimitiveBuilding.h
-----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../utils/Reference.h"
class Vec4f;
class Matrix4f;
class Colour4f;
struct GLObject;
class OpenGLMeshRenderData;


/*=====================================================================
MeshPrimitiveBuilding
---------------------
=====================================================================*/
class MeshPrimitiveBuilding
{
public:
	static Reference<OpenGLMeshRenderData> makeCubeMesh();
	static Reference<OpenGLMeshRenderData> makeQuadMesh(const Vec4f& i, const Vec4f& j);
	static Reference<OpenGLMeshRenderData> makeUnitQuadMesh(); // Makes a quad from (0, 0, 0) to (1, 1, 0)
	static Reference<OpenGLMeshRenderData> makeCapsuleMesh(const Vec3f& bottom_spans, const Vec3f& top_spans);
	static Reference<OpenGLMeshRenderData> makeCylinderMesh(); // Make a cylinder from (0,0,0), to (0,0,1) with radius 1;
	static Reference<OpenGLMeshRenderData> make3DArrowMesh();
	static Reference<OpenGLMeshRenderData> make3DBasisArrowMesh();
	static Reference<GLObject> makeDebugHexahedron(const Vec4f* verts_ws, const Colour4f& col);
};
