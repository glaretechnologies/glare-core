/*=====================================================================
MeshPrimitiveBuilding.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "MeshPrimitiveBuilding.h"


#include "OpenGLEngine.h"
#include "GLMeshBuilding.h"
#include "../graphics/ImageMap.h"
#include "../graphics/DXTCompression.h"
#include "../maths/mathstypes.h"
#include "../maths/GeometrySampling.h"
#include "../utils/Timer.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/ArrayRef.h"
#include <vector>


// Make a cylinder from (0,0,0), to (0,0,1) with radius 1.
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCylinderMesh(VertexBufferAllocator& allocator)
{
	const Vec3f endpoint_a(0, 0, 0);
	const Vec3f endpoint_b(0, 0, 1);

	const int res = 16;

	js::Vector<Vec3f, 16> verts;
	verts.resize(res * 4 + 2);
	js::Vector<Vec3f, 16> normals;
	normals.resize(res * 4 + 2);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(res * 4 + 2);
	js::Vector<uint32, 16> indices;
	indices.resize(res * 12); // 4 tris * res

	const Vec3f dir(0, 0, 1);
	const Vec3f basis_i(1, 0, 0);
	const Vec3f basis_j(0, 1, 0);

	// Make cylinder sides
	for(int i=0; i<res; ++i)
	{
		const float angle = i * Maths::get2Pi<float>() / res;

		// Set verts
		{
			//v[i*2 + 0] is top of side edge, facing outwards.
			//v[i*2 + 1] is bottom of side edge, facing outwards.
			const Vec3f normal = basis_i * cos(angle) + basis_j * sin(angle);
			const Vec3f bottom_pos = endpoint_a + normal;
			const Vec3f top_pos    = endpoint_b + normal;

			normals[i*2 + 0] = normal;
			normals[i*2 + 1] = normal;
			verts[i*2 + 0] = bottom_pos;
			verts[i*2 + 1] = top_pos;
			uvs[i*2 + 0] = Vec2f(0.f);
			uvs[i*2 + 1] = Vec2f(0.f);

			//v[2*r + i] is top of side edge, facing upwards
			normals[2*res + i] = dir;
			verts[2*res + i] = top_pos;
			uvs[2*res + i] = Vec2f(0.f);

			//v[3*r + i] is bottom of side edge, facing down
			normals[3*res + i] = -dir;
			verts[3*res + i] = bottom_pos;
			uvs[3*res + i] = Vec2f(0.f);
		}

		// top centre vert
		normals[4*res + 0] = dir;
		verts[4*res + 0] = endpoint_b;
		uvs[4*res + 0] = Vec2f(0.f);

		// bottom centre vert
		normals[4*res + 1] = -dir;
		verts[4*res + 1] = endpoint_a;
		uvs[4*res + 1] = Vec2f(0.f);


		// Set tris

		// Side face triangles
		indices[i*12 + 0] = i*2 + 1; // top
		indices[i*12 + 1] = i*2 + 0; // bottom 
		indices[i*12 + 2] = (i*2 + 2) % (2*res); // bottom on next edge
		indices[i*12 + 3] = i*2 + 1; // top
		indices[i*12 + 4] = (i*2 + 2) % (2*res); // bottom on next edge
		indices[i*12 + 5] = (i*2 + 3) % (2*res); // top on next edge

												 // top triangle
		indices[i*12 + 6] = res*2 + i + 0;
		indices[i*12 + 7] = res*2 + (i + 1) % res;
		indices[i*12 + 8] = res*4; // top centre vert

								   // bottom triangle
		indices[i*12 + 9]  = res*3 + (i + 1) % res;
		indices[i*12 + 10] = res*3 + i + 0;
		indices[i*12 + 11] = res*4 + 1; // bottom centre vert
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


// Base will be at origin, tip will lie at (1, 0, 0)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::make3DArrowMesh(VertexBufferAllocator& allocator)
{
	const int res = 20;

	js::Vector<Vec3f, 16> verts;
	verts.resize(res * 4 * 2);
	js::Vector<Vec3f, 16> normals;
	normals.resize(res * 4 * 2);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(res * 4 * 2);
	js::Vector<uint32, 16> indices;
	indices.resize(res * 6 * 2); // two tris per quad

	const Vec3f dir(1,0,0);
	const Vec3f basis_i(0,1,0);
	const Vec3f basis_j(0,0,1);

	const float length = 1;
	const float shaft_r = length * 0.02f;
	const float shaft_len = length * 0.8f;
	const float head_r = length * 0.04f;

	// Draw cylinder for shaft of arrow
	for(int i=0; i<res; ++i)
	{
		const float angle      = i       * Maths::get2Pi<float>() / res;
		const float next_angle = (i + 1) * Maths::get2Pi<float>() / res;

		// Define quad
		{
			Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

			normals[i*8 + 0] = normal1;
			normals[i*8 + 1] = normal2;
			normals[i*8 + 2] = normal2;
			normals[i*8 + 3] = normal1;

			Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r);
			Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r);
			Vec3f v2((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r + dir * shaft_len);
			Vec3f v3((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r + dir * shaft_len);

			verts[i*8 + 0] = v0;
			verts[i*8 + 1] = v1;
			verts[i*8 + 2] = v2;
			verts[i*8 + 3] = v3;

			uvs[i*8 + 0] = Vec2f(0.f);
			uvs[i*8 + 1] = Vec2f(0.f);
			uvs[i*8 + 2] = Vec2f(0.f);
			uvs[i*8 + 3] = Vec2f(0.f);

			indices[i*12 + 0] = i*8 + 0; 
			indices[i*12 + 1] = i*8 + 1; 
			indices[i*12 + 2] = i*8 + 2; 
			indices[i*12 + 3] = i*8 + 0;
			indices[i*12 + 4] = i*8 + 2;
			indices[i*12 + 5] = i*8 + 3;
		}

		// Define arrow head
		{
			//Vec3f normal(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			// NOTE: this normal is somewhat wrong.
			Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

			normals[i*8 + 4] = normal1;
			normals[i*8 + 5] = normal2;
			normals[i*8 + 6] = normal2;
			normals[i*8 + 7] = normal1;

			Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * head_r + dir * shaft_len);
			Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * head_r + dir * shaft_len);
			Vec3f v2(dir * length);
			Vec3f v3(dir * length);

			verts[i*8 + 4] = v0;
			verts[i*8 + 5] = v1;
			verts[i*8 + 6] = v2;
			verts[i*8 + 7] = v3;

			uvs[i*8 + 4] = Vec2f(0.f);
			uvs[i*8 + 5] = Vec2f(0.f);
			uvs[i*8 + 6] = Vec2f(0.f);
			uvs[i*8 + 7] = Vec2f(0.f);

			indices[i*12 +  6] = i*8 + 4; 
			indices[i*12 +  7] = i*8 + 5; 
			indices[i*12 +  8] = i*8 + 6; 
			indices[i*12 +  9] = i*8 + 4;
			indices[i*12 + 10] = i*8 + 6;
			indices[i*12 + 11] = i*8 + 7;
		}
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


// Base will be at origin, tips will lie at (1, 0, 0), (0,1,0), (0,0,1)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::make3DBasisArrowMesh(VertexBufferAllocator& allocator)
{
	const int res = 20;

	js::Vector<Vec3f, 16> verts;
	verts.resize(res * 4 * 2 * 3);
	js::Vector<Vec3f, 16> normals;
	normals.resize(res * 4 * 2 * 3);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(res * 4 * 2 * 3);
	js::Vector<uint32, 16> indices;
	indices.resize(res * 6 * 2 * 3); // two tris per quad

	for(int z=0; z<3; ++z)
	{
		const int verts_offset   = res * 4 * 2 * z;
		const int indices_offset = res * 6 * 2 * z;
		Vec3f dir, basis_i, basis_j;
		if(z == 0)
		{
			dir = Vec3f(1,0,0);
			basis_i = Vec3f(0,1,0);
		}
		else if(z == 1)
		{
			dir = Vec3f(0,1,0);
			basis_i = Vec3f(0,0,1);
		}
		else
		{
			dir = Vec3f(0,0,1);
			basis_i = Vec3f(1,0,0);
		}
		basis_j = crossProduct(dir, basis_i);

		const float length = 1;
		const float shaft_r = length * 0.02f;
		const float shaft_len = length * 0.8f;
		const float head_r = length * 0.04f;

		// Draw cylinder for shaft of arrow
		for(int i=0; i<res; ++i)
		{
			const float angle      = i       * Maths::get2Pi<float>() / res;
			const float next_angle = (i + 1) * Maths::get2Pi<float>() / res;

			// Define quad
			{
				Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
				Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

				normals[verts_offset + i*8 + 0] = normal1;
				normals[verts_offset + i*8 + 1] = normal2;
				normals[verts_offset + i*8 + 2] = normal2;
				normals[verts_offset + i*8 + 3] = normal1;

				Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r);
				Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r);
				Vec3f v2((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r + dir * shaft_len);
				Vec3f v3((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r + dir * shaft_len);

				verts[verts_offset + i*8 + 0] = v0;
				verts[verts_offset + i*8 + 1] = v1;
				verts[verts_offset + i*8 + 2] = v2;
				verts[verts_offset + i*8 + 3] = v3;

				uvs[verts_offset + i*8 + 0] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 1] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 2] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 3] = Vec2f(0.f);

				indices[indices_offset + i*12 + 0] = verts_offset + i*8 + 0; 
				indices[indices_offset + i*12 + 1] = verts_offset + i*8 + 1; 
				indices[indices_offset + i*12 + 2] = verts_offset + i*8 + 2; 
				indices[indices_offset + i*12 + 3] = verts_offset + i*8 + 0;
				indices[indices_offset + i*12 + 4] = verts_offset + i*8 + 2;
				indices[indices_offset + i*12 + 5] = verts_offset + i*8 + 3;
			}

			// Define arrow head
			{
				//Vec3f normal(basis_i * cos(angle     ) + basis_j * sin(angle     ));
				// NOTE: this normal is somewhat wrong.
				Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
				Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

				normals[verts_offset + i*8 + 4] = normal1;
				normals[verts_offset + i*8 + 5] = normal2;
				normals[verts_offset + i*8 + 6] = normal2;
				normals[verts_offset + i*8 + 7] = normal1;

				Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * head_r + dir * shaft_len);
				Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * head_r + dir * shaft_len);
				Vec3f v2(dir * length);
				Vec3f v3(dir * length);

				verts[verts_offset + i*8 + 4] = v0;
				verts[verts_offset + i*8 + 5] = v1;
				verts[verts_offset + i*8 + 6] = v2;
				verts[verts_offset + i*8 + 7] = v3;

				uvs[verts_offset + i*8 + 4] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 5] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 6] = Vec2f(0.f);
				uvs[verts_offset + i*8 + 7] = Vec2f(0.f);

				indices[indices_offset + i*12 +  6] = verts_offset + i*8 + 4; 
				indices[indices_offset + i*12 +  7] = verts_offset + i*8 + 5; 
				indices[indices_offset + i*12 +  8] = verts_offset + i*8 + 6; 
				indices[indices_offset + i*12 +  9] = verts_offset + i*8 + 4;
				indices[indices_offset + i*12 + 10] = verts_offset + i*8 + 6;
				indices[indices_offset + i*12 + 11] = verts_offset + i*8 + 7;
			}
		}
	}

	Reference<OpenGLMeshRenderData> mesh_data = GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);

	mesh_data->batches.resize(3);
	for(int z=0; z<3; ++z)
	{
		mesh_data->batches[z].material_index = z;
		mesh_data->batches[z].num_indices = (uint32)res * 6 * 2;
		mesh_data->batches[z].prim_start_offset = sizeof(uint32) * res * 6 * 2 * z;
	}

	return mesh_data;
}


// Base will be at origin, tip will lie at (1, 0, 0)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCapsuleMesh(VertexBufferAllocator& allocator, const Vec3f& /*bottom_spans*/, const Vec3f& /*top_spans*/)
{
	const int phi_res = 20;
	//	const int theta_res = 10;

	js::Vector<Vec3f, 16> verts;
	verts.resize(phi_res * 2 * 2);
	js::Vector<Vec3f, 16> normals;
	normals.resize(phi_res * 2 * 2);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(phi_res * 2 * 2);
	js::Vector<uint32, 16> indices;
	indices.resize(phi_res * 3 * 2); // two tris per quad * 3 indices per tri

#if 0
	const Vec4f dir(0,0,1,0);
	const Vec4f bot_basis_i(bottom_spans[0],0,0,0);
	const Vec4f bot_basis_j(0,bottom_spans[1],0,0);
	const Vec4f top_basis_i(top_spans[0],0,0,0);
	const Vec4f top_basis_j(0,top_spans[1],0,0);

	// Create cylinder
	for(int i=0; i<phi_res; ++i)
	{
		const float angle      = i       * Maths::get2Pi<float>() / phi_res;
		const float next_angle = (i + 1) * Maths::get2Pi<float>() / phi_res;

		Vec3f normal1(bot_basis_i * cos(angle     ) + bot_basis_j * sin(angle     ));
		Vec3f normal2(bot_basis_i * cos(next_angle) + bot_basis_j * sin(next_angle));

		normals[i*4 + 0] = normal1;
		normals[i*4 + 1] = normal2;
		normals[i*4 + 2] = normal2;
		normals[i*4 + 3] = normal1;

		Vec3f v0((bot_basis_i * cos(angle     ) + bot_basis_j * sin(angle     )));
		Vec3f v1((bot_basis_i * cos(next_angle) + bot_basis_j * sin(next_angle)));

		Vec3f v2((top_basis_i * cos(next_angle) + top_basis_j * sin(next_angle)) + dir);
		Vec3f v3((top_basis_i * cos(angle     ) + top_basis_j * sin(angle     )) + dir);

		verts[i*4 + 0] = v0;
		verts[i*4 + 1] = v1;
		verts[i*4 + 2] = v2;
		verts[i*4 + 3] = v3;

		uvs[i*4 + 0] = Vec2f(0.f);
		uvs[i*4 + 1] = Vec2f(0.f);
		uvs[i*4 + 2] = Vec2f(0.f);
		uvs[i*4 + 3] = Vec2f(0.f);

		indices[i*6 + 0] = i*4 + 0; 
		indices[i*6 + 1] = i*4 + 1; 
		indices[i*6 + 2] = i*4 + 2; 
		indices[i*6 + 3] = i*4 + 0;
		indices[i*6 + 4] = i*4 + 2;
		indices[i*6 + 5] = i*4 + 3;
	}

	int normals_offset = phi_res * 4;
	int uvs_offset = phi_res * 4;
	int verts_offset = phi_res * 4;
	int indices_offset = phi_res * 6;

	//Matrix4f to_basis(Vec4f(0,0,0,1), 

	// Create top hemisphere cap
	/*for(int theta_i=0; theta_i<theta_res; ++theta_i)
	{
	for(int phi_i=0; phi_i<phi_res; ++phi_i)
	{
	const float phi_1 = Maths::get2Pi<float>() * phi_i / phi_res;
	const float phi_2 = Maths::get2Pi<float>() * (phi_i + 1) / phi_res;
	const float theta_1 = Maths::pi<float>() * theta_i / theta_res;
	const float theta_2 = Maths::pi<float>() * (theta_i + 1) / theta_res;

	const Vec4f p1 = GeometrySampling::dirForSphericalCoords(phi_1, theta_1);
	const Vec4f p2 = GeometrySampling::dirForSphericalCoords(phi_2, theta_1);
	const Vec4f p3 = GeometrySampling::dirForSphericalCoords(phi_2, theta_2);
	const Vec4f p4 = GeometrySampling::dirForSphericalCoords(phi_1, theta_2);


	//Vec3f normal1(bot_basis_i * cos(phi     ) + bot_basis_j * sin(phi     ));
	//Vec3f normal2(bot_basis_i * cos(next_phi) + bot_basis_j * sin(next_phi));

	normals[i*4 + 0] = p1;
	normals[i*4 + 1] = p2;
	normals[i*4 + 2] = p3;
	normals[i*4 + 3] = p4;

	Vec3f v0((bot_basis_i * cos(angle     ) + bot_basis_j * sin(angle     )));
	Vec3f v1((bot_basis_i * cos(next_angle) + bot_basis_j * sin(next_angle)));

	Vec3f v2((top_basis_i * cos(next_angle) + top_basis_j * sin(next_angle)) + dir);
	Vec3f v3((top_basis_i * cos(angle     ) + top_basis_j * sin(angle     )) + dir);

	verts[i*4 + 0] = v0;
	verts[i*4 + 1] = v1;
	verts[i*4 + 2] = v2;
	verts[i*4 + 3] = v3;

	uvs[i*4 + 0] = Vec2f(0.f);
	uvs[i*4 + 1] = Vec2f(0.f);
	uvs[i*4 + 2] = Vec2f(0.f);
	uvs[i*4 + 3] = Vec2f(0.f);

	indices[i*6 + 0] = i*4 + 0; 
	indices[i*6 + 1] = i*4 + 1; 
	indices[i*6 + 2] = i*4 + 2; 
	indices[i*6 + 3] = i*4 + 0;
	indices[i*6 + 4] = i*4 + 2;
	indices[i*6 + 5] = i*4 + 3;
	}
	}*/


#endif
	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeLineMesh(VertexBufferAllocator& allocator)
{
	js::Vector<Vec3f, 16> verts;
	verts.resize(2);
	js::Vector<Vec3f, 16> normals;
	normals.resize(2);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(2);
	js::Vector<uint32, 16> indices;
	indices.resize(2); // two tris per face

	indices[0] = 0;
	indices[1] = 1;

	Vec3f v0(0, 0, 0); // bottom left
	Vec3f v1(1, 0, 0); // bottom right

	verts[0] = v0;
	verts[1] = v1;

	Vec2f uv0(0, 0);
	Vec2f uv1(1, 0);

	uvs[0] = uv0;
	uvs[1] = uv1;

	for(int i=0; i<2; ++i)
		normals[i] = Vec3f(0, 0, 1);

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeSphereMesh(VertexBufferAllocator& allocator)
{
	const int phi_res = 100;
	const int theta_res = 50;

	js::Vector<Vec3f, 16> verts;
	verts.resize(phi_res * theta_res * 4);
	js::Vector<Vec3f, 16> normals;
	normals.resize(phi_res * theta_res * 4);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(phi_res * theta_res * 4);
	js::Vector<uint32, 16> indices;
	indices.resize(phi_res * theta_res * 6); // two tris per quad

	int i = 0;
	int quad_i = 0;
	for(int phi_i=0; phi_i<phi_res; ++phi_i)
	for(int theta_i=0; theta_i<theta_res; ++theta_i)
	{
		const float phi_1 = Maths::get2Pi<float>() * phi_i / phi_res;
		const float phi_2 = Maths::get2Pi<float>() * (phi_i + 1) / phi_res;
		const float theta_1 = Maths::pi<float>() * theta_i / theta_res;
		const float theta_2 = Maths::pi<float>() * (theta_i + 1) / theta_res;

		const Vec4f p1 = GeometrySampling::dirForSphericalCoords(phi_1, theta_1);
		const Vec4f p2 = GeometrySampling::dirForSphericalCoords(phi_2, theta_1);
		const Vec4f p3 = GeometrySampling::dirForSphericalCoords(phi_2, theta_2);
		const Vec4f p4 = GeometrySampling::dirForSphericalCoords(phi_1, theta_2);

		verts[i    ] = Vec3f(p1); 
		verts[i + 1] = Vec3f(p2); 
		verts[i + 2] = Vec3f(p3); 
		verts[i + 3] = Vec3f(p4);

		normals[i    ] = Vec3f(p1); 
		normals[i + 1] = Vec3f(p2);
		normals[i + 2] = Vec3f(p3); 
		normals[i + 3] = Vec3f(p4);

		uvs[i    ] = Vec2f(phi_1, theta_1); 
		uvs[i + 1] = Vec2f(phi_2, theta_1); 
		uvs[i + 2] = Vec2f(phi_2, theta_2); 
		uvs[i + 3] = Vec2f(phi_1, theta_2);

		indices[quad_i + 0] = i + 0; 
		indices[quad_i + 1] = i + 1; 
		indices[quad_i + 2] = i + 2; 
		indices[quad_i + 3] = i + 0;
		indices[quad_i + 4] = i + 2;
		indices[quad_i + 5] = i + 3;

		i += 4;
		quad_i += 6;
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


// Make a cube mesh.  Bottom left corner will be at origin, opposite corner will lie at (1, 1, 1)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCubeMesh(VertexBufferAllocator& allocator)
{
	js::Vector<Vec3f, 16> verts;
	verts.resize(24); // 6 faces * 4 verts/face
	js::Vector<Vec3f, 16> normals;
	normals.resize(24);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(24);
	js::Vector<uint32, 16> indices;
	indices.resize(6 * 6); // two tris per face, 6 faces

	for(int face = 0; face < 6; ++face)
	{
		indices[face*6 + 0] = face*4 + 0; 
		indices[face*6 + 1] = face*4 + 1; 
		indices[face*6 + 2] = face*4 + 2; 
		indices[face*6 + 3] = face*4 + 0;
		indices[face*6 + 4] = face*4 + 2;
		indices[face*6 + 5] = face*4 + 3;
	}

	int face = 0;

	// x = 0 face
	{
		Vec3f v0(0, 0, 0);
		Vec3f v1(0, 0, 1);
		Vec3f v2(0, 1, 1);
		Vec3f v3(0, 1, 0);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(1, 0);
		uvs[face*4 + 1] = Vec2f(1, 1);
		uvs[face*4 + 2] = Vec2f(0, 1);
		uvs[face*4 + 3] = Vec2f(0, 0);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(-1, 0, 0);

		face++;
	}

	// x = 1 face
	{
		Vec3f v0(1, 0, 0);
		Vec3f v1(1, 1, 0);
		Vec3f v2(1, 1, 1);
		Vec3f v3(1, 0, 1);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(0, 0);
		uvs[face*4 + 1] = Vec2f(1, 0);
		uvs[face*4 + 2] = Vec2f(1, 1);
		uvs[face*4 + 3] = Vec2f(0, 1);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(1, 0, 0);

		face++;
	}

	// y = 0 face
	{
		Vec3f v0(0, 0, 0);
		Vec3f v1(1, 0, 0);
		Vec3f v2(1, 0, 1);
		Vec3f v3(0, 0, 1);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(0, 0);
		uvs[face*4 + 1] = Vec2f(1, 0);
		uvs[face*4 + 2] = Vec2f(1, 1);
		uvs[face*4 + 3] = Vec2f(0, 1);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(0, -1, 0);

		face++;
	}

	// y = 1 face
	{
		Vec3f v0(0, 1, 0);
		Vec3f v1(0, 1, 1);
		Vec3f v2(1, 1, 1);
		Vec3f v3(1, 1, 0);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(1, 0);
		uvs[face*4 + 1] = Vec2f(1, 1);
		uvs[face*4 + 2] = Vec2f(0, 1);
		uvs[face*4 + 3] = Vec2f(0, 0);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(0, 1, 0);

		face++;
	}

	// z = 0 face
	{
		Vec3f v0(0, 0, 0);
		Vec3f v1(0, 1, 0);
		Vec3f v2(1, 1, 0);
		Vec3f v3(1, 0, 0);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(0, 0);
		uvs[face*4 + 1] = Vec2f(1, 0);
		uvs[face*4 + 2] = Vec2f(1, 1);
		uvs[face*4 + 3] = Vec2f(0, 1);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(0, 0, -1);

		face++;
	}

	// z = 1 face
	{
		Vec3f v0(0, 0, 1);
		Vec3f v1(1, 0, 1);
		Vec3f v2(1, 1, 1);
		Vec3f v3(0, 1, 1);

		verts[face*4 + 0] = v0;
		verts[face*4 + 1] = v1;
		verts[face*4 + 2] = v2;
		verts[face*4 + 3] = v3;

		uvs[face*4 + 0] = Vec2f(0, 0);
		uvs[face*4 + 1] = Vec2f(1, 0);
		uvs[face*4 + 2] = Vec2f(1, 1);
		uvs[face*4 + 3] = Vec2f(0, 1);

		for(int i=0; i<4; ++i)
			normals[face*4 + i] = Vec3f(0, 0, 1);

		face++;
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


GLObjectRef MeshPrimitiveBuilding::makeDebugHexahedron(VertexBufferAllocator& allocator, const Vec4f* verts_ws, const Colour4f& col)
{
	js::Vector<Vec3f, 16> verts;
	verts.resize(24); // 6 faces * 4 verts/face
	js::Vector<Vec3f, 16> normals;
	normals.resize(24);
	js::Vector<Vec2f, 16> uvs(24, Vec2f(0.f));
	js::Vector<uint32, 16> indices;
	indices.resize(6 * 6); // two tris per face, 6 faces

	for(int face = 0; face < 6; ++face)
	{
		indices[face*6 + 0] = face*4 + 0; 
		indices[face*6 + 1] = face*4 + 1; 
		indices[face*6 + 2] = face*4 + 2; 
		indices[face*6 + 3] = face*4 + 0;
		indices[face*6 + 4] = face*4 + 2;
		indices[face*6 + 5] = face*4 + 3;
	}

	int face = 0;

	// x = 0 face
	verts[face*4 + 0] = toVec3f(verts_ws[0]);
	verts[face*4 + 1] = toVec3f(verts_ws[4]);
	verts[face*4 + 2] = toVec3f(verts_ws[7]);
	verts[face*4 + 3] = toVec3f(verts_ws[3]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	// x = 1 face
	verts[face*4 + 0] = toVec3f(verts_ws[1]);
	verts[face*4 + 1] = toVec3f(verts_ws[2]);
	verts[face*4 + 2] = toVec3f(verts_ws[6]);
	verts[face*4 + 3] = toVec3f(verts_ws[5]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	// y = 0 face
	verts[face*4 + 0] = toVec3f(verts_ws[0]);
	verts[face*4 + 1] = toVec3f(verts_ws[1]);
	verts[face*4 + 2] = toVec3f(verts_ws[5]);
	verts[face*4 + 3] = toVec3f(verts_ws[4]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	// y = 1 face
	verts[face*4 + 0] = toVec3f(verts_ws[2]);
	verts[face*4 + 1] = toVec3f(verts_ws[3]);
	verts[face*4 + 2] = toVec3f(verts_ws[7]);
	verts[face*4 + 3] = toVec3f(verts_ws[6]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	// z = 0 face
	verts[face*4 + 0] = toVec3f(verts_ws[0]);
	verts[face*4 + 1] = toVec3f(verts_ws[3]);
	verts[face*4 + 2] = toVec3f(verts_ws[2]);
	verts[face*4 + 3] = toVec3f(verts_ws[1]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	// z = 1 face
	verts[face*4 + 0] = toVec3f(verts_ws[4]);
	verts[face*4 + 1] = toVec3f(verts_ws[5]);
	verts[face*4 + 2] = toVec3f(verts_ws[6]);
	verts[face*4 + 3] = toVec3f(verts_ws[7]);
	for(int i=0; i<4; ++i)
		normals[face*4 + i] = crossProduct(verts[face*4 + 1] - verts[face*4 + 0], verts[face*4 + 3] - verts[face*4 + 0]);
	face++;

	Reference<OpenGLMeshRenderData> mesh_data = GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);

	// Make the object
	GLObjectRef ob = new GLObject();
	ob->ob_to_world_matrix = Matrix4f::identity();
	ob->mesh_data = mesh_data;
	ob->materials.resize(1);
	ob->materials[0].albedo_rgb = Colour3f(col[0], col[1], col[2]);
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;

	return ob;
}


// Makes a quad with xspan = 1, yspan = 1, lying on the z = 0 plane.
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeUnitQuadMesh(VertexBufferAllocator& allocator)
{
	js::Vector<Vec3f, 16> verts;
	verts.resize(4);
	js::Vector<Vec3f, 16> normals;
	normals.resize(4);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(4);
	js::Vector<uint32, 16> indices;
	indices.resize(6); // two tris per face

	indices[0] = 0; 
	indices[1] = 1; 
	indices[2] = 2; 
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	Vec3f v0(0, 0, 0); // bottom left
	Vec3f v1(1, 0, 0); // bottom right
	Vec3f v2(1, 1, 0); // top right
	Vec3f v3(0, 1, 0); // top left

	verts[0] = v0;
	verts[1] = v1;
	verts[2] = v2;
	verts[3] = v3;

	Vec2f uv0(0, 0);
	Vec2f uv1(1, 0);
	Vec2f uv2(1, 1);
	Vec2f uv3(0, 1);

	uvs[0] = uv0;
	uvs[1] = uv1;
	uvs[2] = uv2;
	uvs[3] = uv3;

	for(int i=0; i<4; ++i)
		normals[i] = Vec3f(0, 0, 1);

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeQuadMesh(VertexBufferAllocator& allocator, const Vec4f& i, const Vec4f& j)
{
	js::Vector<Vec3f, 16> verts;
	verts.resize(4);
	js::Vector<Vec3f, 16> normals;
	normals.resize(4);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(4);
	js::Vector<uint32, 16> indices;
	indices.resize(6); // two tris per face

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	Vec3f v0(0.f); // bottom left
	Vec3f v1 = toVec3f(i); // bottom right
	Vec3f v2 = toVec3f(i) + toVec3f(j); // top right
	Vec3f v3 = toVec3f(j); // top left

	verts[0] = v0;
	verts[1] = v1;
	verts[2] = v2;
	verts[3] = v3;

	Vec2f uv0(0, 0);
	Vec2f uv1(1, 0);
	Vec2f uv2(1, 1);
	Vec2f uv3(0, 1);

	uvs[0] = uv0;
	uvs[1] = uv1;
	uvs[2] = uv2;
	uvs[3] = uv3;

	for(int z=0; z<4; ++z)
		normals[z] = toVec3f(crossProduct(i, j));// Vec3f(0, 0, -1);

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}
