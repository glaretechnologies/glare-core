/*=====================================================================
MeshPrimitiveBuilding.cpp
-------------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "MeshPrimitiveBuilding.h"


#include "OpenGLEngine.h"
#include "GLMeshBuilding.h"
#include "OpenGLMeshRenderData.h"
#include "IncludeOpenGL.h"
#include "../graphics/SRGBUtils.h"
#include "../maths/mathstypes.h"
#include "../maths/GeometrySampling.h"
#include "../utils/StringUtils.h"


// Make a cylinder from (0,0,0), to (0,0,1) with radius 1.
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCylinderMesh(VertexBufferAllocator& allocator, bool end_caps)
{
	const int res = 16; // Number of quad faces around cylinder

	const int num_verts = end_caps ? (res * 4 + 2) : (res * 2);
	js::Vector<Vec3f, 16> verts(num_verts);
	js::Vector<Vec3f, 16> normals(num_verts);
	js::Vector<Vec2f, 16> uvs(num_verts);

	const int indices_per_face = end_caps ? 12 : 6; // with caps, 4 tris * res, or without caps, 2 tris * res

	js::Vector<uint32, 16> indices(res * indices_per_face);

	const Vec3f endpoint_a(0, 0, 0);
	const Vec3f endpoint_b(0, 0, 1);
	const Vec3f dir(0, 0, 1);
	const Vec3f basis_i(1, 0, 0);
	const Vec3f basis_j(0, 1, 0);

	for(int i=0; i<res; ++i)
	{
		const float angle = i * Maths::get2Pi<float>() / res;

		// vert[i*2 + 0] is top of side edge, normal facing outwards.
		// vert[i*2 + 1] is bottom of side edge, normal facing outwards.
		const Vec3f normal = basis_i * cos(angle) + basis_j * sin(angle);
		const Vec3f bottom_pos = endpoint_a + normal;
		const Vec3f top_pos    = endpoint_b + normal;

		normals[i*2 + 0] = normal;
		normals[i*2 + 1] = normal;
		verts[i*2 + 0] = bottom_pos;
		verts[i*2 + 1] = top_pos;
		uvs[i*2 + 0] = Vec2f((float)i / res, 0);
		uvs[i*2 + 1] = Vec2f((float)i / res, 1);

		if(end_caps)
		{
			// vert[2*r + i] is top of side edge, normal facing upwards
			normals[2*res + i] = dir;
			verts[2*res + i] = top_pos;
			uvs[2*res + i] = Vec2f(0.f);

			// vert[3*r + i] is bottom of side edge, normal facing down
			normals[3*res + i] = -dir;
			verts[3*res + i] = bottom_pos;
			uvs[3*res + i] = Vec2f(0.f);
		}

		// Side face triangles
		indices[i*indices_per_face + 0] = i*2 + 1; // top
		indices[i*indices_per_face + 1] = i*2 + 0; // bottom 
		indices[i*indices_per_face + 2] = (i*2 + 2) % (2*res); // bottom on next edge
		indices[i*indices_per_face + 3] = i*2 + 1; // top
		indices[i*indices_per_face + 4] = (i*2 + 2) % (2*res); // bottom on next edge
		indices[i*indices_per_face + 5] = (i*2 + 3) % (2*res); // top on next edge

		if(end_caps)
		{
			// top triangle
			indices[i*indices_per_face + 6] = res*2 + i + 0;
			indices[i*indices_per_face + 7] = res*2 + (i + 1) % res;
			indices[i*indices_per_face + 8] = res*4; // top centre vert

			// bottom triangle
			indices[i*indices_per_face + 9]  = res*3 + (i + 1) % res;
			indices[i*indices_per_face + 10] = res*3 + i + 0;
			indices[i*indices_per_face + 11] = res*4 + 1; // bottom centre vert
		}
	}

	if(end_caps)
	{
		// top centre vert
		normals[4*res + 0] = dir;
		verts[4*res + 0] = endpoint_b;
		uvs[4*res + 0] = Vec2f(0.f);

		// bottom centre vert
		normals[4*res + 1] = -dir;
		verts[4*res + 1] = endpoint_a;
		uvs[4*res + 1] = Vec2f(0.f);
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


// Base will be at origin, tip will lie at (1, 0, 0)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::make3DArrowMesh(VertexBufferAllocator& allocator)
{
	const int res = 20;
	const int verts_per_angle = 4 + 4 + 3;
	const int indices_per_angle = 6 + 6 + 3;

	js::Vector<Vec3f, 16> verts;
	verts.resize(res * verts_per_angle);
	js::Vector<Vec3f, 16> normals;
	normals.resize(res * verts_per_angle);
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(res * verts_per_angle);
	js::Vector<uint32, 16> indices;
	indices.resize(res * indices_per_angle);

	const Vec3f dir(1,0,0);
	const Vec3f basis_i(0,1,0);
	const Vec3f basis_j(0,0,1);

	const float length = 1;
	const float shaft_r = length * 0.02f;
	const float shaft_len = length * 0.8f;
	const float head_r = length * 0.04f;

	for(int i=0; i<res; ++i)
	{
		const float angle      = i       * Maths::get2Pi<float>() / res;
		const float next_angle = (i + 1) * Maths::get2Pi<float>() / res;

		// Define cylinder for shaft of arrow
		{
			Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

			normals[i*verts_per_angle + 0] = normal1;
			normals[i*verts_per_angle + 1] = normal2;
			normals[i*verts_per_angle + 2] = normal2;
			normals[i*verts_per_angle + 3] = normal1;

			Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r);
			Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r);
			Vec3f v2((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * shaft_r + dir * shaft_len);
			Vec3f v3((basis_i * cos(angle     ) + basis_j * sin(angle     )) * shaft_r + dir * shaft_len);

			verts[i*verts_per_angle + 0] = v0;
			verts[i*verts_per_angle + 1] = v1;
			verts[i*verts_per_angle + 2] = v2;
			verts[i*verts_per_angle + 3] = v3;

			uvs[i*verts_per_angle + 0] = Vec2f(0.f);
			uvs[i*verts_per_angle + 1] = Vec2f(0.f);
			uvs[i*verts_per_angle + 2] = Vec2f(0.f);
			uvs[i*verts_per_angle + 3] = Vec2f(0.f);

			indices[i*indices_per_angle + 0] = i*verts_per_angle + 0; 
			indices[i*indices_per_angle + 1] = i*verts_per_angle + 1; 
			indices[i*indices_per_angle + 2] = i*verts_per_angle + 2; 
			indices[i*indices_per_angle + 3] = i*verts_per_angle + 0;
			indices[i*indices_per_angle + 4] = i*verts_per_angle + 2;
			indices[i*indices_per_angle + 5] = i*verts_per_angle + 3;
		}

		// Define arrow head
		{
			//Vec3f normal(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			// NOTE: this normal is somewhat wrong.
			Vec3f normal1(basis_i * cos(angle     ) + basis_j * sin(angle     ));
			Vec3f normal2(basis_i * cos(next_angle) + basis_j * sin(next_angle));

			normals[i*verts_per_angle + 4] = normal1;
			normals[i*verts_per_angle + 5] = normal2;
			normals[i*verts_per_angle + 6] = normal2;
			normals[i*verts_per_angle + 7] = normal1;

			Vec3f v0((basis_i * cos(angle     ) + basis_j * sin(angle     )) * head_r + dir * shaft_len);
			Vec3f v1((basis_i * cos(next_angle) + basis_j * sin(next_angle)) * head_r + dir * shaft_len);
			Vec3f v2(dir * length);
			Vec3f v3(dir * length);

			verts[i*verts_per_angle + 4] = v0;
			verts[i*verts_per_angle + 5] = v1;
			verts[i*verts_per_angle + 6] = v2;
			verts[i*verts_per_angle + 7] = v3;

			uvs[i*verts_per_angle + 4] = Vec2f(0.f);
			uvs[i*verts_per_angle + 5] = Vec2f(0.f);
			uvs[i*verts_per_angle + 6] = Vec2f(0.f);
			uvs[i*verts_per_angle + 7] = Vec2f(0.f);

			indices[i*indices_per_angle +  6] = i*verts_per_angle + 4; 
			indices[i*indices_per_angle +  7] = i*verts_per_angle + 5; 
			indices[i*indices_per_angle +  8] = i*verts_per_angle + 6; 
			indices[i*indices_per_angle +  9] = i*verts_per_angle + 4;
			indices[i*indices_per_angle + 10] = i*verts_per_angle + 6;
			indices[i*indices_per_angle + 11] = i*verts_per_angle + 7;
		}

		// Draw disc on bottom of arrow head
		{
			normals[i*verts_per_angle +  8] = -dir;
			normals[i*verts_per_angle +  9] = -dir;
			normals[i*verts_per_angle + 10] = -dir;

			verts[i*verts_per_angle +  8] = dir * shaft_len;
			verts[i*verts_per_angle +  9] = (basis_i * cos(next_angle) + basis_j * sin(next_angle)) * head_r + dir * shaft_len;
			verts[i*verts_per_angle + 10] = (basis_i * cos(     angle) + basis_j * sin(     angle)) * head_r + dir * shaft_len;

			uvs[i*verts_per_angle +  8] = Vec2f(0.f);
			uvs[i*verts_per_angle +  9] = Vec2f(0.f);
			uvs[i*verts_per_angle + 10] = Vec2f(0.f);

			indices[i*indices_per_angle + 12] = i*verts_per_angle +  8; 
			indices[i*indices_per_angle + 13] = i*verts_per_angle +  9; 
			indices[i*indices_per_angle + 14] = i*verts_per_angle + 10; 
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
		mesh_data->batches[z].prim_start_offset_B = sizeof(uint32) * res * 6 * 2 * z;
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
	const int phi_res = 32;
	const int theta_res = 16;

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
	ob->materials[0].albedo_linear_rgb = toLinearSRGB(Colour3f(col[0], col[1], col[2]));
	ob->materials[0].alpha = col[3];
	ob->materials[0].transparent = col[3] < 1.f;

	return ob;
}


/*
makeQuadMesh
------------
For res = 4:

^ j
|
|     -----------------------------------------------
|     |v12        /  |v13        /   |v14        /   |v15
|     |         /    |         /     |         /     |
|     |   t13 /      |   t15 /       |   t17  /      |
|     |     /   t12  |     /         |     /         |
|     |   /          |   /       t14 |   /       t16 |
|     | /            | /             | /             |
|     -----------------------------------------------
|     |v8         /  |v9         /   |v10        /   |v11
|     |         /    |         /     |         /     |
|     |   t7  /      |   t9  /       |   t11  /      |
|     |     /   t6   |     /         |     /         |
|     |   /          |   /       t8  |   /       t10 |
|     | /            | /             | /             |
|     -----------------------------------------------
|     |v4         /  |v5         /   |v5         /   |v7
|     |         /    |         /     |         /     |
|     |   t1  /      |   t3  /       |   t5  /       |
|     |     /   t0   |     /         |     /         |
|     |   /          |   /       t2  |   /       t4  |
|     | /            | /             | /             |
|     -----------------------------------------------
|     v0            v1              v2             v3
|     
---------------------------------------------------------> i
*/
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeQuadMesh(VertexBufferAllocator& allocator, const Vec4f& i, const Vec4f& j, int res)
{
	assert(res >= 2);

	const int num_verts = Maths::square(res);
	const Vec3f normal = toVec3f(normalise(crossProduct(i, j)));

	js::Vector<Vec3f, 16> verts(num_verts);
	js::Vector<Vec3f, 16> normals(num_verts, normal); // NOTE: could use geometric normals instead.
	js::Vector<Vec2f, 16> uvs(num_verts);
	js::Vector<uint32, 16> indices(Maths::square(res - 1) * 6); // two tris * 3 verts/tri per quad

	for(int y=0; y<res; ++y)
	{
		const float v = (float)y / (res - 1);
		for(int x=0; x<res; ++x)
		{
			const float u = (float)x / (res - 1);

			verts[y*res + x] = toVec3f(i * u + j * v);

			uvs[y*res + x] = Vec2f(u, v);
		}
	}

	for(int y=0; y<res-1; ++y)
	{
		for(int x=0; x<res-1; ++x)
		{
			indices[(y*(res-1) + x) * 6 + 0] = y    *res + x + 0; // bot left
			indices[(y*(res-1) + x) * 6 + 1] = y    *res + x + 1; // bot right
			indices[(y*(res-1) + x) * 6 + 2] = (y+1)*res + x + 1; // top right
			indices[(y*(res-1) + x) * 6 + 3] = y    *res + x + 0; // bot left
			indices[(y*(res-1) + x) * 6 + 4] = (y+1)*res + x + 1; // top right
			indices[(y*(res-1) + x) * 6 + 5] = (y+1)*res + x + 0; // top left
		}
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


// Makes a quad from (0, 0, 0) to (1, 1, 0)
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeUnitQuadMesh(VertexBufferAllocator& allocator)
{
	return makeQuadMesh(allocator, Vec4f(1,0,0,0), Vec4f(0,1,0,0), /*res=*/2);
}


// See docs/rounded_rect_geometry.jpg
Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeRoundedCornerRect(VertexBufferAllocator& allocator, const Vec4f& i_, const Vec4f& j_, float w, float h, float corner_radius, int tris_per_corner)
{
	const int N = tris_per_corner; // Num tris in each rounded corner

	js::Vector<Vec3f, 16> verts;
	verts.resize(12 + N * 4); // 12 verts for centre 3 quads (6 tris), plus N for each rounded corner.
	js::Vector<Vec3f, 16> normals;
	normals.resize(verts.size());
	js::Vector<Vec2f, 16> uvs;
	uvs.resize(verts.size());
	js::Vector<uint32, 16> indices;
	const int num_tris = 6 + N * 4;
	indices.resize(num_tris * 3);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 0;
	indices[4] = 2;
	indices[5] = 3;

	indices[6] = 4;
	indices[7] = 5;
	indices[8] = 6;
	indices[9] = 4;
	indices[10] = 6;
	indices[11] = 7;

	indices[12] = 8;
	indices[13] = 9;
	indices[14] = 10;
	indices[15] = 8;
	indices[16] = 10;
	indices[17] = 11;

	const Vec3f i = toVec3f(i_);
	const Vec3f j = toVec3f(j_);

	const float r = corner_radius;

	verts[0] = i * r; // bottom left
	verts[1] = i * (w - r); // bottom right
	verts[2] = i * (w - r) + j * h; // top right
	verts[3] = i * r + j * h; // top left

	verts[4] = j * r; // bottom left
	verts[5] = i * r + j * r; // bottom right
	verts[6] = i * r + j * (h - r); // top right
	verts[7] = j * (h - r); // top left

	verts[8] = i * (w - r) + j * r; // bottom left
	verts[9] = i * w + j * r; // bottom right
	verts[10] = i * w + j * (h - r); // top right
	verts[11] = i * (w - r) + j * (h - r); // top left

	// Bottom right triangle fan
	{
		const int indices_start = 18;
		const int verts_start = 12;
		for(int z=0; z<N; ++z)
		{
			float theta = Maths::pi_2<float>() * (z + 1.f) / N;
			verts[verts_start + z] = verts[8] + i * std::sin(theta) * r - j * std::cos(theta) * r;

			indices[indices_start + z*3 + 0] = 8;
			if(z == 0)
				indices[indices_start + z*3 + 1] = 1;
			else
				indices[indices_start + z*3 + 1] = verts_start + z - 1;

			indices[indices_start + z*3 + 2] = verts_start + z;
		}
	}

	// top right triangle fan
	{
		const int indices_start = 18 + N * 3;
		const int verts_start = 12 + N;
		for(int z=0; z<N; ++z)
		{
			float theta = Maths::pi_2<float>() * (z + 1.f) / N;
			verts[verts_start + z] = verts[11] + i * std::cos(theta) * r + j * std::sin(theta) * r;

			indices[indices_start + z*3 + 0] = 11;
			if(z == 0)
				indices[indices_start + z*3 + 1] = 10;
			else
				indices[indices_start + z*3 + 1] = verts_start + z - 1;

			indices[indices_start + z*3 + 2] = verts_start + z;
		}
	}

	// bot left triangle fan
	{
		const int indices_start = 18 + (N * 3) * 2;
		const int verts_start = 12 + N*2;
		for(int z=0; z<N; ++z)
		{
			float theta = Maths::pi_2<float>() * (z + 1.f) / N;
			verts[verts_start + z] = verts[5] - i * std::cos(theta) * r - j * std::sin(theta) * r;

			indices[indices_start + z*3 + 0] = 5;
			if(z == 0)
				indices[indices_start + z*3 + 1] = 4;
			else
				indices[indices_start + z*3 + 1] = verts_start + z - 1;

			indices[indices_start + z*3 + 2] = verts_start + z;
		}
	}

	// top left triangle fan
	{
		const int indices_start = 18 + (N * 3) * 3;
		const int verts_start = 12 + N*3;
		for(int z=0; z<N; ++z)
		{
			float theta = Maths::pi_2<float>() * (z + 1.f) / N;
			verts[verts_start + z] = verts[6] - i * std::sin(theta) * r + j * std::cos(theta) * r;

			indices[indices_start + z*3 + 0] = 6;
			if(z == 0)
				indices[indices_start + z*3 + 1] = 3;
			else
				indices[indices_start + z*3 + 1] = verts_start + z - 1;

			indices[indices_start + z*3 + 2] = verts_start + z;
		}
	}

	const Vec3f n = crossProduct(i, j);
	for(size_t z=0; z<normals.size(); ++z)
		normals[z] = n;

	for(size_t z=0; z<uvs.size(); ++z)
		uvs[z] = Vec2f(dot(verts[z], i) / w, dot(verts[z], j) / h);

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, normals, uvs, indices);
}


Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeSpriteQuad(VertexBufferAllocator& allocator)
{
	OpenGLMeshRenderDataRef mesh_data = new OpenGLMeshRenderData();
	OpenGLMeshRenderData& meshdata = *mesh_data;

	meshdata.setIndexType(GL_UNSIGNED_SHORT);

	const int N = 1;

	meshdata.has_uvs = true;
	meshdata.has_shading_normals = true;
	meshdata.batches.resize(1);
	meshdata.batches[0].material_index = 0;
	meshdata.batches[0].num_indices = N * 6; // 2 tris / imposter * 3 indices / tri
	meshdata.batches[0].prim_start_offset_B = 0;

	meshdata.num_materials_referenced = 1;


	const size_t vert_size_B = sizeof(float) * 6; // position, width, uv

	// NOTE: The order of these attributes should be the same as in OpenGLProgram constructor with the glBindAttribLocations.
	size_t in_vert_offset_B = 0;
	VertexAttrib pos_attrib;
	pos_attrib.enabled = true;
	pos_attrib.num_comps = 3;
	pos_attrib.type = GL_FLOAT;
	pos_attrib.normalised = false;
	pos_attrib.stride = (uint32)vert_size_B;
	pos_attrib.offset = (uint32)in_vert_offset_B;
	meshdata.vertex_spec.attributes.push_back(pos_attrib);
	in_vert_offset_B += sizeof(float) * 3;

	const size_t width_offset_B = in_vert_offset_B;
	VertexAttrib imposter_width_attrib;
	imposter_width_attrib.enabled = true;
	imposter_width_attrib.num_comps = 1;
	imposter_width_attrib.type = GL_FLOAT;
	imposter_width_attrib.normalised = false;
	imposter_width_attrib.stride = (uint32)vert_size_B;
	imposter_width_attrib.offset = (uint32)in_vert_offset_B;
	meshdata.vertex_spec.attributes.push_back(imposter_width_attrib);
	in_vert_offset_B += sizeof(float);

	const size_t uv_offset_B = in_vert_offset_B;
	VertexAttrib uv_attrib;
	uv_attrib.enabled = true;
	uv_attrib.num_comps = 2;
	uv_attrib.type = GL_FLOAT;
	uv_attrib.normalised = false;
	uv_attrib.stride = (uint32)vert_size_B;
	uv_attrib.offset = (uint32)in_vert_offset_B;
	meshdata.vertex_spec.attributes.push_back(uv_attrib);
	in_vert_offset_B += sizeof(float) * 2;

	//const size_t imposter_rot_offset_B = in_vert_offset_B;
	//VertexAttrib imposter_rot_attrib;
	//imposter_rot_attrib.enabled = true;
	//imposter_rot_attrib.num_comps = 1;
	//imposter_rot_attrib.type = GL_FLOAT;
	//imposter_rot_attrib.normalised = false;
	//imposter_rot_attrib.stride = (uint32)vert_size_B;
	//imposter_rot_attrib.offset = (uint32)in_vert_offset_B;
	//meshdata.vertex_spec.attributes.push_back(imposter_rot_attrib);
	//in_vert_offset_B += sizeof(float);

	assert(in_vert_offset_B == vert_size_B);


	//glare::BumpAllocation temp_vert_data(N * 4 * vert_size_B, /*alignment=*/8, bump_allocator);
	std::vector<uint8> temp_vert_data(N * 4 * vert_size_B);
	uint8* const vert_data = (uint8*)temp_vert_data.data();

	js::AABBox aabb_os = js::AABBox::emptyAABBox();

	const Vec2f uv0(0,0);
	const Vec2f uv1(1,0);
	const Vec2f uv2(1,1);
	const Vec2f uv3(0,1);
	
	const float imposter_width = 1;

	// Build vertex data
	//const VegetationLocationInfo* const veg_locations = locations.data();
	for(int i=0; i<N; ++i)
	{
		//const float master_scale = 4.5f;
		//const float imposter_height = veg_locations[i].scale * master_scale;
		//const float imposter_width = imposter_height * imposter_width_over_height;
		//const float imposter_rot = 0; // TEMP
		

		// Store lower 2 vertices
		const Vec4f lower_pos(0,0,0,1);// = veg_locations[i].pos;
		
		const Vec4f v0pos = lower_pos;// - Vec4f(0.1f, 0,0,0); // TEMP
		const Vec4f v1pos = lower_pos;// + Vec4f(0.1f, 0,0,0);
		
		// Vertex 0
		std::memcpy(vert_data + vert_size_B * (i * 4 + 0), &v0pos, sizeof(float)*3); // Store x,y,z pos coords.
		std::memcpy(vert_data + vert_size_B * (i * 4 + 0) + width_offset_B , &imposter_width, sizeof(float));
		std::memcpy(vert_data + vert_size_B * (i * 4 + 0) + uv_offset_B , &uv0, sizeof(float)*2);
		//std::memcpy(vert_data + vert_size_B * (i * 4 + 0) + imposter_rot_offset_B , &imposter_rot, sizeof(float));

		// Vertex 1
		std::memcpy(vert_data + vert_size_B * (i * 4 + 1), &v1pos, sizeof(float)*3); // Store x,y,z pos coords.
		std::memcpy(vert_data + vert_size_B * (i * 4 + 1) + width_offset_B , &imposter_width, sizeof(float));
		std::memcpy(vert_data + vert_size_B * (i * 4 + 1) + uv_offset_B , &uv1, sizeof(float)*2);
		//std::memcpy(vert_data + vert_size_B * (i * 4 + 1) + imposter_rot_offset_B , &imposter_rot, sizeof(float));

		// Store upper 2 vertices
		const Vec4f upper_pos(0,0,0,1);// = veg_locations[i].pos + Vec4f(0,0,imposter_height, 0);
		
		const Vec4f v2pos = upper_pos;// + Vec4f(0.1f, 0,0,0); //TEMP
		const Vec4f v3pos = upper_pos;// - Vec4f(0.1f, 0,0,0);

		// Vertex 2
		std::memcpy(vert_data + vert_size_B * (i * 4 + 2), &v2pos, sizeof(float)*3); // Store x,y,z pos coords.
		std::memcpy(vert_data + vert_size_B * (i * 4 + 2) + width_offset_B , &imposter_width, sizeof(float));
		std::memcpy(vert_data + vert_size_B * (i * 4 + 2) + uv_offset_B , &uv2, sizeof(float)*2);
		//std::memcpy(vert_data + vert_size_B * (i * 4 + 2) + imposter_rot_offset_B , &imposter_rot, sizeof(float));

		// Vertex 3
		std::memcpy(vert_data + vert_size_B * (i * 4 + 3), &v3pos, sizeof(float)*3); // Store x,y,z pos coords.
		std::memcpy(vert_data + vert_size_B * (i * 4 + 3) + width_offset_B , &imposter_width, sizeof(float));
		std::memcpy(vert_data + vert_size_B * (i * 4 + 3) + uv_offset_B , &uv3, sizeof(float)*2);
		//std::memcpy(vert_data + vert_size_B * (i * 4 + 3) + imposter_rot_offset_B , &imposter_rot, sizeof(float));

		aabb_os.enlargeToHoldPoint(lower_pos);
	}

	// Enlarge aabb_os to take into account imposter size
	aabb_os.min_ -= Vec4f(imposter_width, imposter_width, imposter_width, 0);
	aabb_os.max_ += Vec4f(imposter_width, imposter_width, imposter_width, 0);

	mesh_data->vbo_handle = allocator.allocateVertexDataSpace(mesh_data->vertex_spec.vertStride(), temp_vert_data.data(), temp_vert_data.size());

	// Build index data
	//glare::BumpAllocation temp_index_data(sizeof(uint16) * N * 6, /*alignment=*/8, bump_allocator);
	std::vector<uint16> temp_index_data(N * 6);
	uint16* const indices = (uint16*)temp_index_data.data();

	for(int i=0; i<N; ++i)
	{
		// v3   v2
		// |----|
		// |   /|
		// |  / |
		// | /  |
		// |----|--> x
		// v0   v1

		// bot right tri
		const int offset = i * 6;
		indices[offset + 0] = (uint16)(i * 4 + 0); // bot left
		indices[offset + 1] = (uint16)(i * 4 + 1); // bot right
		indices[offset + 2] = (uint16)(i * 4 + 2); // top right

		// top left tri
		indices[offset + 3] = (uint16)(i * 4 + 0); // bot left
		indices[offset + 4] = (uint16)(i * 4 + 2); // top right
		indices[offset + 5] = (uint16)(i * 4 + 3); // top left
	}

	meshdata.indices_vbo_handle = allocator.allocateIndexDataSpace(indices, temp_index_data.size() * sizeof(uint16));

	allocator.getOrCreateAndAssignVAOForMesh(meshdata, mesh_data->vertex_spec);

	meshdata.aabb_os = aabb_os;

	return mesh_data;
}


static void addCuboidMesh(const Vec3f& min, const Vec3f& max, js::Vector<Vec3f, 16>& verts, js::Vector<uint32, 16>& indices)
{
	const uint32 start_verts_size   = (uint32)verts.size();
	const uint32 start_indices_size = (uint32)indices.size();

	verts.resize(start_verts_size + 24); // 6 faces * 4 verts/face
	indices.resize(start_indices_size + 6 * 6); // two tris per face, 6 faces

	for(int face = 0; face < 6; ++face)
	{
		indices[start_indices_size + face*6 + 0] = start_verts_size + face*4 + 0; 
		indices[start_indices_size + face*6 + 1] = start_verts_size + face*4 + 1; 
		indices[start_indices_size + face*6 + 2] = start_verts_size + face*4 + 2; 
		indices[start_indices_size + face*6 + 3] = start_verts_size + face*4 + 0;
		indices[start_indices_size + face*6 + 4] = start_verts_size + face*4 + 2;
		indices[start_indices_size + face*6 + 5] = start_verts_size + face*4 + 3;
	}

	int face = 0;

	// x = min.x face
	{
		Vec3f v0(min.x, min.y, min.z);
		Vec3f v1(min.x, min.y, max.z);
		Vec3f v2(min.x, max.y, max.z);
		Vec3f v3(min.x, max.y, min.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}

	// x = max.x face
	{
		Vec3f v0(max.x, min.y, min.z);
		Vec3f v1(max.x, max.y, min.z);
		Vec3f v2(max.x, max.y, max.z);
		Vec3f v3(max.x, min.y, max.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}

	// y = min.y face
	{
		Vec3f v0(min.x, min.y, min.z);
		Vec3f v1(max.x, min.y, min.z);
		Vec3f v2(max.x, min.y, max.z);
		Vec3f v3(min.x, min.y, max.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}

	// y = max.y face
	{
		Vec3f v0(min.x, max.y, min.z);
		Vec3f v1(min.x, max.y, max.z);
		Vec3f v2(max.x, max.y, max.z);
		Vec3f v3(max.x, max.y, min.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}

	// z = min.z face
	{
		Vec3f v0(min.x, min.y, min.z);
		Vec3f v1(min.x, max.y, min.z);
		Vec3f v2(max.x, max.y, min.z);
		Vec3f v3(max.x, min.y, min.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}

	// z = max.z face
	{
		Vec3f v0(min.x, min.y, max.z);
		Vec3f v1(max.x, min.y, max.z);
		Vec3f v2(max.x, max.y, max.z);
		Vec3f v3(min.x, max.y, max.z);

		verts[start_verts_size + face*4 + 0] = v0;
		verts[start_verts_size + face*4 + 1] = v1;
		verts[start_verts_size + face*4 + 2] = v2;
		verts[start_verts_size + face*4 + 3] = v3;

		face++;
	}
}


Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCuboidEdgeAABBMesh(VertexBufferAllocator& allocator, const Vec4f& span, float edge_width)
{
	// Make with cuboids along edges.

	js::Vector<Vec3f> verts;
	js::Vector<uint32> indices;

	// Along x axis, y=0, z=0
	addCuboidMesh(Vec3f(0.f), Vec3f(span[0], edge_width, edge_width), verts, indices);

	// Along x axis, y=span.y edge, z=0
	addCuboidMesh(Vec3f(0.f, span[1] - edge_width, 0.f), Vec3f(span[0], span[1], edge_width), verts, indices);

	// Along x axis, y=0, z = span.z edge
	addCuboidMesh(Vec3f(0.f, 0.f, span[2] - edge_width), Vec3f(span[0], edge_width, span[2]), verts, indices);

	// Along x axis, y=span.y edge, z = span.z edge
	addCuboidMesh(Vec3f(0.f, span[1] - edge_width, span[2] - edge_width), Vec3f(span[0], span[1], span[2]), verts, indices);


	// Along y axis, x=0, z=0
	addCuboidMesh(Vec3f(0.f), Vec3f(edge_width, span[1], edge_width), verts, indices);

	// Along y axis, x=span.x edge, z=0
	addCuboidMesh(Vec3f(span[0] - edge_width, 0.f, 0.f), Vec3f(span[0], span[1], edge_width), verts, indices);

	// Along y axis, x=0, z = span.z edge
	addCuboidMesh(Vec3f(0.f, 0.f, span[2] - edge_width), Vec3f(edge_width, span[1], span[2]), verts, indices);

	// Along y axis, x=span.x edge, z = span.z edge
	addCuboidMesh(Vec3f(span[0] - edge_width, 0.f, span[2] - edge_width), Vec3f(span[0], span[1], span[2]), verts, indices);


	// Along z axis, x=0, y=0
	addCuboidMesh(Vec3f(0.f), Vec3f(edge_width, edge_width, span[2]), verts, indices);

	// Along z axis, x=span.x edge, y=0
	addCuboidMesh(Vec3f(span[0] - edge_width, 0.f, 0.f), Vec3f(span[0], edge_width, span[2]), verts, indices);

	// Along z axis, x=0, y = span.y edge
	addCuboidMesh(Vec3f(0.f, span[1] - edge_width, 0.f), Vec3f(edge_width, span[1], span[2]), verts, indices);

	// Along z axis, x=span.x edge, y = span.y edge
	addCuboidMesh(Vec3f(span[0] - edge_width, span[1] - edge_width, 0.f), Vec3f(span[0], span[1], span[2]), verts, indices);


	return GLMeshBuilding::buildMeshRenderData(allocator, verts, indices);
}



 
/*
           res=2           ____  v3
                           /    \
                     -----        \
                   /              |
            ------                 \
        /                          |
      _           _----------------\ v2
    /         __/                   |
   / ---------                       \
  /                                  |
----------------------------------------
v0                                   v1


*/

Reference<OpenGLMeshRenderData> MeshPrimitiveBuilding::makeCircleSector(VertexBufferAllocator& allocator, float total_angle/*, int num_segments*/)
{
	const int res = (int)ceil(total_angle * 64); // Number of tris

	const int num_verts = (res + 2);
	js::Vector<Vec3f, 16> verts(num_verts);
	js::Vector<uint32, 16> indices(res * 3);

	const Vec3f basis_i(1, 0, 0);
	const Vec3f basis_j(0, 1, 0);

	verts[0] = Vec3f(0,0,0);

	for(int i=0; i<=res; ++i)
	{
		const float angle = i * total_angle / res;

		const Vec3f v_i = cos(angle) * basis_i + sin(angle) * basis_j;
		verts[1 + i] = v_i;

		if(i < res)
		{
			// Add tri
			indices[i*3 + 0] = 0;
			indices[i*3 + 1] = i + 1;
			indices[i*3 + 2] = i + 2;
		}
	}

	return GLMeshBuilding::buildMeshRenderData(allocator, verts, indices);
}
