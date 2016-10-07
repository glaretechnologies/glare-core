/*=====================================================================
OpenGLEngineTests.cpp
---------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "OpenGLEngineTests.h"


#include "OpenGLEngine.h"
#include "../indigo/TestUtils.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"


namespace OpenGLEngineTests
{

#if BUILD_TESTS


static void doPerfTest(const std::string& indigo_base_dir, const std::string& mesh_path)
{
	//--------------------- Do perf tests ----------------------------
	try
	{
		Indigo::MeshRef mesh = new Indigo::Mesh();
		Indigo::Mesh::readFromFile(toIndigoString(mesh_path), *mesh);
		conPrint(mesh_path + ": " + toString(mesh->triangles.size()) + " tris, " + toString(mesh->quads.size()) + " quads, " + toString(mesh->vert_positions.size()) + " verts");
	
		const int NUM_TRIALS = 1;
		for(int i=0; i<NUM_TRIALS; ++i)
		{
			Timer timer;
			Reference<OpenGLMeshRenderData> mesh_renderdata = OpenGLEngine::buildIndigoMesh(mesh,
				true // skip opengl calls
			);
			conPrint("Build time for '" + mesh_path + "': " + timer.elapsedStringNSigFigs(5));
		}
		conPrint("");
	}
	catch(Indigo::IndigoException& e)
	{
		failTest(toStdString(e.what()));
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


void test(const std::string& indigo_base_dir)
{
	//--------------------- Do perf tests ----------------------------
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/green velvet test_meshes/mesh_1401557802_33447.igmesh"); // ~32 KB mesh
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Supercar_Benchmark_Optimised/mesh_3732024865775885879.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");
}


#endif // BUILD_TESTS


} // end namespace OpenGLEngineTests
