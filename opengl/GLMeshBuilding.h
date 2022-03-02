/*=====================================================================
GLMeshBuilding.h
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <maths/vec2.h>
#include <maths/vec3.h>
class OpenGLMeshRenderData;
class VBO;
class BatchedMesh;
class VertexBufferAllocator;
namespace Indigo { class Mesh; }


/*=====================================================================
GLMeshBuilding
--------------
=====================================================================*/
class GLMeshBuilding
{
public:
	static void buildMeshRenderData(VertexBufferAllocator& allocator, OpenGLMeshRenderData& meshdata, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, const js::Vector<uint32, 16>& indices);

	// Build OpenGLMeshRenderData from an Indigo::Mesh.
	// Throws glare::Exception on failure.
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(VertexBufferAllocator* allocator, const Reference<Indigo::Mesh>& mesh_, bool skip_opengl_calls);

	// May keep a reference to the mesh in the newly created OpenGLMeshRenderData.
	static Reference<OpenGLMeshRenderData> buildBatchedMesh(VertexBufferAllocator* allocator, const Reference<BatchedMesh>& mesh_, bool skip_opengl_calls, const Reference<VBO>& instancing_matrix_data);
};
