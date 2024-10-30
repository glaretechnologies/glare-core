/*=====================================================================
GLMeshBuilding.h
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <utils/ArrayRef.h>
#include <maths/vec2.h>
#include <maths/vec3.h>
class OpenGLMeshRenderData;
class VBO;
class BatchedMesh;
class VertexBufferAllocator;
namespace Indigo { class Mesh; }
namespace glare { class StackAllocator; }


/*=====================================================================
GLMeshBuilding
--------------
Various functions to build OpenGLMeshRenderData from various sources.
=====================================================================*/
class GLMeshBuilding
{
public:
	// Build OpenGLMeshRenderData from vectors of vertices, normals, uvs etc.
	static Reference<OpenGLMeshRenderData> buildMeshRenderData(VertexBufferAllocator& allocator, const js::Vector<Vec3f, 16>& vertices, const js::Vector<Vec3f, 16>& normals, const js::Vector<Vec2f, 16>& uvs, 
		const js::Vector<uint32, 16>& indices);

	// Build OpenGLMeshRenderData from vectors of just vertices, uvs and indices.
	static Reference<OpenGLMeshRenderData> buildMeshRenderData(VertexBufferAllocator& allocator, ArrayRef<float> vert_pos_and_uvs, ArrayRef<uint32> indices,
		glare::StackAllocator& stack_allocator);

	// Build OpenGLMeshRenderData from vectors of just vertices (positions), and indices.
	static Reference<OpenGLMeshRenderData> buildMeshRenderData(VertexBufferAllocator& allocator, const js::Vector<Vec3f, 16>& vertices, ArrayRef<uint32> indices);


	// Build OpenGLMeshRenderData from an Indigo::Mesh.
	static Reference<OpenGLMeshRenderData> buildIndigoMesh(VertexBufferAllocator* allocator, const Reference<Indigo::Mesh>& mesh_, bool skip_opengl_calls);

	// Build OpenGLMeshRenderData from a BatchedMesh.
	// May keep a reference to the mesh in the newly created OpenGLMeshRenderData.
	static Reference<OpenGLMeshRenderData> buildBatchedMesh(VertexBufferAllocator* allocator, const Reference<BatchedMesh>& mesh_, bool skip_opengl_calls, const Reference<VBO>& instancing_matrix_data);
};
