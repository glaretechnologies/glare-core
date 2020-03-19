/*=====================================================================
OpenGLEngineTests.cpp
---------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "OpenGLEngineTests.h"


#include "OpenGLEngine.h"
#include "../graphics/ImageMap.h"
#include "../graphics/imformatdecoder.h"
#include "../indigo/TestUtils.h"
#include "../indigo/TextureServer.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"


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

		if(mesh->triangles.empty() && mesh->quads.empty())
		{
			conPrint("mesh is empty.");
			return;
		}

		const int NUM_TRIALS = 1;
		for(int t=0; t<NUM_TRIALS; ++t)
		{
			Timer timer;
			Reference<OpenGLMeshRenderData> mesh_renderdata = OpenGLEngine::buildIndigoMesh(mesh,
				true // skip opengl calls
			);
			conPrint("Build time for '" + mesh_path + "': " + timer.elapsedStringNSigFigs(5));
			

			// Check resulting batches
			{
				const size_t expected_num_indices = mesh->triangles.size() * 3 + mesh->quads.size() * 6;

				const size_t index_type_size = (mesh_renderdata->index_type == GL_UNSIGNED_BYTE) ? 1 : ((mesh_renderdata->index_type == GL_UNSIGNED_SHORT) ? 2 : 4);

				size_t num_indices = 0;
				size_t expected_cur_offset = 0;
				for(size_t i=0; i<mesh_renderdata->batches.size(); ++i)
				{
					testAssert(expected_cur_offset == mesh_renderdata->batches[i].prim_start_offset);

					if(i > 0)
						testAssert(mesh_renderdata->batches[i].material_index != mesh_renderdata->batches[i - 1].material_index);

					num_indices += mesh_renderdata->batches[i].num_indices;
					expected_cur_offset += mesh_renderdata->batches[i].num_indices * index_type_size;
				}

				testAssert(num_indices == expected_num_indices);
			}
		}
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
	//BuildUInt8MapTextureDataScratchState state;

	conPrint("OpenGLEngineTests::loadAndUnloadTexture(): " + toString(W) + " x " + toString(H) + ", num_comp: " + toString(num_comp));
	ImageMapUInt8Ref map = new ImageMapUInt8(W, H, num_comp);
	map->set(0);

	for(int i=0; i<num_iters; ++i)
	{
		Timer timer;

		Reference<OpenGLTexture> opengl_tex = engine.getOrLoadOpenGLTexture(OpenGLTextureKey("somekey"), *map/*, OpenGLTexture::Filtering_Nearest*//*, state*/);

		engine.removeOpenGLTexture(OpenGLTextureKey("somekey"));

		opengl_tex = NULL; // destroy tex

		conPrint("Tex load and destroy took " + timer.elapsedString());
	}
}


static void doTextureLoadingAndInsertionTests(OpenGLEngine& engine, bool use_canonical_paths)
{
	engine.getTextureServer()->setUseCanonicalPathKeys(use_canonical_paths);

	{
		//----------------- Load and insert texture into OpenGL Engine.-----------------
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg";

		const std::string key = engine.getTextureServer()->keyForPath(path); // Get canonical path.  May throw TextureServerExcep

		Reference<Map2D> map = ImFormatDecoder::decodeImage(".", key); // Load texture from disk and decode it.

		testAssert(dynamic_cast<const ImageMapUInt8*>(map.ptr()));
		const ImageMapUInt8* imagemap = map.downcastToPtr<ImageMapUInt8>();

		Reference<TextureData> texture_data = TextureLoading::buildUInt8MapTextureData(imagemap, &engine, /*multithread=*/true);

		// Give data to OpenGL engine
		engine.texture_data_manager->insertBuiltTextureData(key, texture_data);

		//----------------- Now query engine for texture and make sure we get a texture back .-----------------
		Reference<OpenGLTexture> opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key));
		testAssert(opengl_tex.nonNull());

		// Query again
		opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key));
		testAssert(opengl_tex.nonNull());
	}


	{
		//----------------- Make an object using a texture, insert into engine -----------------
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg";

		GLObjectRef ob = new GLObject();
		ob->materials.resize(1);
		ob->materials[0].tex_path = path;
		ob->ob_to_world_matrix = Matrix4f::identity();
		ob->mesh_data = engine.getCubeMeshData();

		engine.addObject(ob);
		testAssert(ob->materials[0].albedo_texture.isNull()); // Texture shouldn't have been loaded yet.

		//----------------- Load and insert texture into OpenGL Engine.-----------------
		const std::string key = engine.getTextureServer()->keyForPath(path); // Get canonical path.  May throw TextureServerExcep
		Reference<Map2D> map = ImFormatDecoder::decodeImage(".", key); // Load texture from disk and decode it.
		testAssert(dynamic_cast<const ImageMapUInt8*>(map.ptr()));
		const ImageMapUInt8* imagemap = map.downcastToPtr<ImageMapUInt8>();
		Reference<TextureData> texture_data = TextureLoading::buildUInt8MapTextureData(imagemap, &engine, /*multithread=*/true);

		engine.texture_data_manager->insertBuiltTextureData(key, texture_data); // Give data to OpenGL engine

		//----------------- query engine for texture and make sure we get a texture back .-----------------
		Reference<OpenGLTexture> opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key));
		testAssert(opengl_tex.nonNull());


		//----------------- Notify the opengl engine that the texture was loaded, and check the object has had the texture assigned. -----------------
		engine.textureLoaded(path, OpenGLTextureKey(key));

		testAssert(ob->materials[0].albedo_texture.nonNull());

		//----------------- Now query engine for texture and make sure we get a texture back .-----------------
		opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key));
		testAssert(opengl_tex.nonNull());
	}
}


void doTextureLoadingTests(OpenGLEngine& engine)
{
	const bool original_use_canonical_paths = engine.getTextureServer()->useCanonicalPaths();

	doTextureLoadingAndInsertionTests(engine, /*use_canonical_paths=*/false);
	doTextureLoadingAndInsertionTests(engine, /*use_canonical_paths=*/true);

	engine.getTextureServer()->setUseCanonicalPathKeys(original_use_canonical_paths);


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
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/arrow.igmesh"); // Has both tris and quads
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/poolparty_reduced/mesh_18276362613739127974.igmesh"); // ~100 KB mesh
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Supercar_Benchmark_Optimised/mesh_3732024865775885879.igmesh");
	doPerfTest(indigo_base_dir, TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");


	// Run on all IGMESH files in testscenes.
	{
		const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getIndigoTestReposDir() + "/testscenes", "igmesh");

		for(size_t i=0; i<paths.size(); ++i)
		{
			conPrint(paths[i]);
			doPerfTest(indigo_base_dir, paths[i]);
		}
	}

	conPrint("OpenGLEngineTests::test() done.");
}


#endif // BUILD_TESTS


} // end namespace OpenGLEngineTests
