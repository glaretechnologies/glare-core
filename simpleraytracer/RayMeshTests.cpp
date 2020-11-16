/*=====================================================================
RayMeshTests.cpp
----------------
Copyright Glare Technologies Limited 2017 -
Generated at 2017-05-09 18:51:12 +0100
=====================================================================*/
#include "RayMeshTests.h"


#if BUILD_TESTS


#include "raymesh.h"
#include "../dll/include/IndigoMesh.h"
#include "../indigo/TestUtils.h"
#include "../indigo/Diffuse.h"
#include "../indigo/ConstantSpectrumMatParameter.h"
#include "../indigo/Vec3MatParameter.h"
#include "../indigo/UniformSpectrum.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/ConPrint.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/EmbreeDeviceHandle.h"


void RayMeshTests::test()
{
	conPrint("RayMeshTests::test()");



	//======================== Test RayMeshTriangle =====================
	{
		RayMeshTriangle t;
		t.setMatIndexAndUseShadingNormals(777, RayMesh_NoShadingNormals);
		testAssert(t.getUseShadingNormals() == RayMesh_NoShadingNormals);
		testAssert(t.getTriMatIndex() == 777);
	}

	{
		RayMeshTriangle t;
		t.setMatIndexAndUseShadingNormals(123, RayMesh_UseShadingNormals);
		testAssert(t.getUseShadingNormals() == RayMesh_UseShadingNormals);
		testAssert(t.getTriMatIndex() == 123);
	}

	{
		const RayMeshTriangle t(0, 1, 2, 666, RayMesh_UseShadingNormals);
		testAssert(t.vertex_indices[0] == 0);
		testAssert(t.vertex_indices[1] == 1);
		testAssert(t.vertex_indices[2] == 2);
		testAssert(t.getTriMatIndex() == 666);
		testAssert(t.getUseShadingNormals() == RayMesh_UseShadingNormals);
	}

	{
		const RayMeshTriangle t(0, 1, 2, 666, RayMesh_NoShadingNormals);
		testAssert(t.vertex_indices[0] == 0);
		testAssert(t.vertex_indices[1] == 1);
		testAssert(t.vertex_indices[2] == 2);
		testAssert(t.getTriMatIndex() == 666);
		testAssert(t.getUseShadingNormals() == RayMesh_NoShadingNormals);
	}



	Geometry::BuildOptions build_options;

	EmbreeDeviceHandle embree_device(rtcNewDevice("verbose=3,frequency_level=simd128"));
	build_options.embree_device = embree_device.ptr();

	Indigo::TaskManager task_manager;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	Reference<Material> diffuse_mat = new Diffuse(
		new ConstantSpectrumMatParameter(new UniformSpectrum(0.6f)), // albedo
		NULL, // bump_param
		NULL, // displacement_param
		NULL, // base_emission_param
		NULL, // emission_param
		NULL, // normal map param
		0, // layer
		false, // random tri cols
		Material::MaterialArgs()
	);

	// Test computation of shading normals from triangles
	{
		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(0, 1, 0));
		const uint32 uv[3] = { 0, 0, 0 };
		const uint32 tri_0_v[3] = { 0, 1, 2 };
		indigo_mesh.addTriangle(tri_0_v, uv, 0);
		const uint32 tri_1_v[3] = { 0, 2, 3 };
		indigo_mesh.addTriangle(tri_1_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);
			
		// Test vertex shading normals
		for(int i=0; i<4; ++i)
			testEqual(mesh.getVertices()[i].normal, Vec3f(0, 0, 1));

		// Test geometric normals.
		// Note that getGeometricNormalAndMatIndex() returns a normal vector with length = element area.
		unsigned int mat_index;
		testEqual(mesh.getGeometricNormalAndMatIndex(HitInfo(0, Vec2f(0.f, 0.f)), mat_index), Vec4f(0, 0, 0.5f, 0)); // Test for tri 0
		testEqual(mat_index, 0u);
		testEqual(mesh.getGeometricNormalAndMatIndex(HitInfo(1, Vec2f(0.f, 0.f)), mat_index), Vec4f(0, 0, 0.5f, 0)); // Test for tri 1
		testEqual(mat_index, 0u);
	}

	// Test with shading normals supplied, and make sure they are used.
	{
		Indigo::Vec3f shading_normal = normalise(Indigo::Vec3f(0, 1, 1));

		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(0, 1, 0), shading_normal);
		const uint32 uv[3] = { 0, 0, 0 };
		const uint32 tri_0_v[3] = { 0, 1, 2 };
		indigo_mesh.addTriangle(tri_0_v, uv, 0);
		const uint32 tri_1_v[3] = { 0, 2, 3 };
		indigo_mesh.addTriangle(tri_1_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

		// Test vertex shading normals - the should be the same as the supplied shading_normal.
		for(int i=0; i<4; ++i)
			testEpsEqual(mesh.getVertices()[i].normal, Vec3f(shading_normal.x, shading_normal.y, shading_normal.z));
	}

	// Test computation of shading normals from quads
	{
		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(0, 1, 0));
		const uint32 uv[4] = { 0, 0, 0, 0 };
		const uint32 quad_0_v[4] = { 0, 1, 2, 3 };
		indigo_mesh.addQuad(quad_0_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

		testAssert(mesh.getNumQuads() == 0); // Should have been converted to tris.
		testAssert(mesh.getNumTris() == 2);

		// Test vertex shading normals
		for(int i=0; i<4; ++i)
			testEqual(mesh.getVertices()[i].normal, Vec3f(0, 0, 1));

		// Test geometric normals.
		// Note that getGeometricNormalAndMatIndex() returns a normal vector with length = element area.
		unsigned int mat_index;
		testEqual(mesh.getGeometricNormalAndMatIndex(HitInfo(0, Vec2f(0.f, 0.f)), mat_index), Vec4f(0, 0, 0.5f, 0)); // Test for tri 0
		testEqual(mat_index, 0u);
		testEqual(mesh.getGeometricNormalAndMatIndex(HitInfo(1, Vec2f(0.f, 0.f)), mat_index), Vec4f(0, 0, 0.5f, 0)); // Test for tri 1
		testEqual(mat_index, 0u);
	}

	// Test quad mesh with shading normals supplied, and make sure they are used.
	{
		Indigo::Vec3f shading_normal = normalise(Indigo::Vec3f(0, 1, 1));

		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0), shading_normal);
		indigo_mesh.addVertex(Indigo::Vec3f(0, 1, 0), shading_normal);
		const uint32 uv[4] = { 0, 0, 0, 0 };
		const uint32 quad_0_v[4] = { 0, 1, 2, 3 };
		indigo_mesh.addQuad(quad_0_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

		testAssert(mesh.getNumQuads() == 0); // Should have been converted to tris.
		testAssert(mesh.getNumTris() == 2);

		// Test vertex shading normals - the should be the same as the supplied shading_normal.
		for(int i=0; i<4; ++i)
			testEpsEqual(mesh.getVertices()[i].normal, Vec3f(shading_normal.x, shading_normal.y, shading_normal.z));
	}

	// Test a degenerate quad - a quad that when triangulated, will have a zero area triangle.
	// Such meshes have been observed 'in the wild'.
	// See https://bugs.glaretechnologies.com/issues/594 and degenerate_quad_normal_test.igs.
	// We want to make sure the computed vertex normals are valid (and not NaNs).
	{
		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0)); // Same as vert 2
		const uint32 uv[4] = { 0, 0, 0, 0 };
		const uint32 quad_0_v[4] = { 0, 1, 2, 3 };
		indigo_mesh.addQuad(quad_0_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

		testAssert(mesh.getNumQuads() == 0); // Should have been converted to tris.
		testAssert(mesh.getNumTris() == 2);

		// Test vertex shading normals
		for(int i=0; i<4; ++i)
			testEqual(mesh.getVertices()[i].normal, Vec3f(0, 0, 1));
	}


	// Do some performance tests
	if(false)
	{
		// Make a grid mesh
		Indigo::Mesh m;
		m.num_uv_mappings = 1;
		const int N = 500;
		for(int y=0; y<N; ++y)
		{
			const float v = (float)y/(float)N;
			for(int x=0; x<N; ++x)
			{
				const float u = (float)x/(float)N;
				m.vert_positions.push_back(Indigo::Vec3f(u, v, 0.f));
				m.vert_normals.push_back(Indigo::Vec3f(0, 0, 1));
				m.uv_pairs.push_back(Indigo::Vec2f(u, v));
				
				if(x < N-1 && y < N-1)
				{
					m.quads.push_back(Indigo::Quad());
					m.quads.back().mat_index = 0;
					m.quads.back().vertex_indices[0] = m.quads.back().uv_indices[0] = y    *N + x;
					m.quads.back().vertex_indices[1] = m.quads.back().uv_indices[1] = y    *N + x+1;
					m.quads.back().vertex_indices[2] = m.quads.back().uv_indices[2] = (y+1)*N + x+1;
					m.quads.back().vertex_indices[3] = m.quads.back().uv_indices[3] = (y+1)*N + x;
				}
			}
		}

		const int num_trials = 20;
		double min_time = 1.0e20;
		for(int i=0; i<num_trials; ++i)
		{
			Timer timer;

			RayMesh mesh("mesh", /*use shading normals=*/true);
			mesh.fromIndigoMesh(m);
			mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
				print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
			//mesh.build(build_options, print_output, /*verbose=*/false, task_manager);

			min_time = myMin(min_time, timer.elapsed());
		}

		conPrint("min_time: " + ::doubleToStringNSigFigs(min_time, 5) + " s");
	}

	//==================== Perf test getInfoForHit() ========================
	if(false)
	{
		Indigo::Mesh indigo_mesh;
		indigo_mesh.addVertex(Indigo::Vec3f(0, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 0, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(1, 1, 0));
		indigo_mesh.addVertex(Indigo::Vec3f(0, 1, 0));
		const uint32 uv[3] = { 0, 0, 0 };
		const uint32 tri_0_v[3] = { 0, 1, 2 };
		indigo_mesh.addTriangle(tri_0_v, uv, 0);
		const uint32 tri_1_v[3] = { 0, 2, 3 };
		indigo_mesh.addTriangle(tri_1_v, uv, 0);
		indigo_mesh.endOfModel();

		RayMesh mesh("mesh", /*use shading normals=*/true);
		mesh.fromIndigoMesh(indigo_mesh);
		mesh.subdivideAndDisplace(task_manager, ArrayRef<MaterialRef>(&diffuse_mat, 1), Matrix4f::identity(), 0.01f, std::vector<Planef>(), std::vector<Planef>(),
			print_output, /*verbose=*/false, /*should_cancel_callback=*/NULL);
		mesh.build(build_options, should_cancel_callback, print_output, /*verbose=*/false, task_manager);

		HitInfo hitinfo;
		hitinfo.sub_elem_index = 0;
		hitinfo.sub_elem_coords.set(0.3f, 0.6f);

		Timer timer;
		int N = 1000000;
		Vec4f sum(0.0f);
		for(int i=0; i<N; ++i)
		{
			Vec4f N_g_os;
			Vec4f pre_bump_N_s_os_unnormed;
			Vec4f pos_os;
			float pos_os_abs_error;
			unsigned int mat_index;
			
			Vec2f uv0;
			mesh.getInfoForHit(hitinfo,
				N_g_os,
				pre_bump_N_s_os_unnormed,
				mat_index,
				pos_os,
				pos_os_abs_error,
				uv0);

			sum += N_g_os;
		}
			
		double elapsed = timer.elapsed();
		double scalarsum = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];

		conPrint("getInfoForHit() time: " + ::toString(1.0e9 * elapsed / N) + " ns");
		TestUtils::silentPrint(::toString(scalarsum));
	}


	conPrint("RayMeshTests::test() done.");
}


#endif // BUILD_TESTS
