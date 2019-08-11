/*=====================================================================
OpenGLEngineTests.cpp
---------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "OpenGLEngineTests.h"


#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
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


void loadAndUnloadTexture(OpenGLEngine& engine, int W, int H, int num_comp, int num_iters = 1)
{
	conPrint("OpenGLEngineTests::loadAndUnloadTexture(): " + toString(W) + " x " + toString(H) + ", num_comp: " + toString(num_comp));
	ImageMapUInt8Ref map = new ImageMapUInt8(W, H, num_comp);
	map->set(0);

	for(int i=0; i<num_iters; ++i)
	{
		Timer timer;

		Reference<OpenGLTexture> opengl_tex = engine.getOrLoadOpenGLTexture(OpenGLTextureKey("somekey"), *map/*, OpenGLTexture::Filtering_Nearest*/);

		engine.removeOpenGLTexture(OpenGLTextureKey("somekey"));

		opengl_tex = NULL; // destroy tex

		conPrint("Tex load and destroy took " + timer.elapsedString());
	}
}


void doTextureLoadingTests(OpenGLEngine& engine)
{
	loadAndUnloadTexture(engine, 256, 8, 3);
	loadAndUnloadTexture(engine, 8, 256, 3);
	loadAndUnloadTexture(engine, 1, 1, 3);
	loadAndUnloadTexture(engine, 255, 255, 3);
	loadAndUnloadTexture(engine, 257, 257, 3);

	loadAndUnloadTexture(engine, 256, 8, 4);
	loadAndUnloadTexture(engine, 8, 256, 4);
	loadAndUnloadTexture(engine, 1, 1, 4);
	loadAndUnloadTexture(engine, 255, 255, 4);
	loadAndUnloadTexture(engine, 257, 257, 4);

	loadAndUnloadTexture(engine, 3000, 2600, 3, 4);
	loadAndUnloadTexture(engine, 3000, 2600, 4, 4);
}


void test(const std::string& indigo_base_dir)
{
	conPrint("OpenGLEngineTests::test()");

	//--------------------- Do perf tests ----------------------------
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/poolparty_reduced/mesh_18276362613739127974.igmesh"); // ~100 KB mesh
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Supercar_Benchmark_Optimised/mesh_3732024865775885879.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");

	conPrint("OpenGLEngineTests::test() done.");
}


#endif // BUILD_TESTS


} // end namespace OpenGLEngineTests
