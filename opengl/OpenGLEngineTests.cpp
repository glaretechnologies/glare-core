/*=====================================================================
OpenGLEngineTests.cpp
---------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "OpenGLEngineTests.h"


#include "OpenGLEngine.h"
#include "GLMeshBuilding.h"
#include "../graphics/TextureProcessing.h"
#include "../graphics/ImageMap.h"
#include "../graphics/imformatdecoder.h"
#include "../graphics/bitmap.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/TestUtils.h"
#include "../indigo/TextureServer.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include "../utils/IncludeHalf.h"
#include <graphics/GifDecoder.h>


namespace OpenGLEngineTests
{

#if BUILD_TESTS


static void doTest(const std::string& indigo_base_dir, const std::string& mesh_path)
{
	//--------------------- Do perf and functionality tests ----------------------------
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

			Reference<OpenGLMeshRenderData> mesh_renderdata = GLMeshBuilding::buildIndigoMesh(/*allocator=*/NULL, mesh,
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
	catch(glare::Exception& e)
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

		Reference<OpenGLTexture> opengl_tex = engine.getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("somekey"), *map/*, OpenGLTexture::Filtering_Nearest*//*, state*/);

		engine.removeOpenGLTexture(OpenGLTextureKey("somekey"));

		opengl_tex = NULL; // destroy tex

		conPrint("Tex load and destroy took " + timer.elapsedString());
	}
}


static void doTextureChunkedLoadingTestForMap2D(OpenGLEngine& engine, const std::string& tex_path, Reference<Map2D> map, size_t max_total_upload_bytes)
{
	const Reference<OpenGLEngine> opengl_engine(&engine);

	const std::string key = engine.getTextureServer()->keyForPath(tex_path); // Get canonical path.  May throw TextureServerExcep

	const bool use_compression = opengl_engine->textureCompressionSupportedAndEnabled();
	Reference<TextureData> texture_data = TextureProcessing::buildTextureData(map.ptr(), &engine.general_mem_allocator, &engine.getTaskManager(), use_compression);

	const bool use_sRGB = true;
	OpenGLTextureLoadingProgress loading_progress;
	testAssert(!loading_progress.loadingInProgress());
	TextureLoading::initialiseTextureLoadingProgress(tex_path, Reference<OpenGLEngine>(&engine), OpenGLTextureKey(key), use_sRGB, texture_data, loading_progress);
	testAssert(loading_progress.loadingInProgress());

	const int MAX_ITERS = 100000;
	int i = 0;
	for(; i<MAX_ITERS; ++i)
	{
		testAssert(loading_progress.loadingInProgress());
		size_t total_bytes_uploaded = 0;
		TextureLoading::partialLoadTextureIntoOpenGL(Reference<OpenGLEngine>(&engine), loading_progress, total_bytes_uploaded, max_total_upload_bytes);
		if(loading_progress.done())
			break;
	}
	testAssert(i < MAX_ITERS);
	testAssert(loading_progress.opengl_tex.nonNull());
	testAssert(loading_progress.opengl_tex->xRes() == map->getMapWidth());
	testAssert(loading_progress.opengl_tex->yRes() == map->getMapHeight());
}


static void doTextureChunkedLoadingTestForPath(OpenGLEngine& engine, const std::string& tex_path)
{
	const std::string key = engine.getTextureServer()->keyForPath(tex_path); // Get canonical path.  May throw TextureServerExcep

	Reference<Map2D> map;
	if(hasExtension(key, "gif"))
		map = GIFDecoder::decodeImageSequence(key);
	else
		map = ImFormatDecoder::decodeImage(".", key);

	size_t max_upload_size_B = 2000;
	doTextureChunkedLoadingTestForMap2D(engine, tex_path, map, max_upload_size_B);
}

static void doTextureChunkedLoadingTestForUInt8MapWithDims(OpenGLEngine& engine, size_t W, size_t H, size_t N)
{
	ImageMapUInt8Ref map = new ImageMapUInt8(W, H, N);
	map->set(128);
	doTextureChunkedLoadingTestForMap2D(engine, "dummy_path_uint8_" + toString(W) + "_" + toString(H) + "_" + toString(N), map, /*max_upload_size_B=*/2000);
}


static void doTextureChunkedLoadingTestForHalfMapWithDims(OpenGLEngine& engine, size_t W, size_t H, size_t N)
{
	Reference<ImageMap<half, HalfComponentValueTraits>> map = new ImageMap<half, HalfComponentValueTraits>(W, H, N);
	map->set(0.5f);
	doTextureChunkedLoadingTestForMap2D(engine, "dummy_path_half_" + toString(W) + "_" + toString(H) + "_" + toString(N), map, /*max_upload_size_B=*/2000);
}


static void doTextureChunkedLoadingTestForFloatMapWithDims(OpenGLEngine& engine, size_t W, size_t H, size_t N)
{
	Reference<ImageMapFloat> map = new ImageMapFloat(W, H, N);
	map->set(0.5f);
	doTextureChunkedLoadingTestForMap2D(engine, "dummy_path_float_" + toString(W) + "_" + toString(H) + "_" + toString(N), map, /*max_upload_size_B=*/2000);
}



static void doTextureChunkedLoadingTests(OpenGLEngine& engine)
{
	// Test uint8 maps
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 256, 1, 3); // Will be considerd a palette texture, so not compressed.

	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 2, 2, 3);
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 256, 255, 3); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 255, 256, 3); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 255, 255, 3); // Test with a texture with an odd number of columns.

	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 2, 2, 4);
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 256, 255, 4); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 255, 256, 4); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForUInt8MapWithDims(engine, 255, 255, 4); // Test with a texture with an odd number of columns.

	// Test half map
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 256, 1, 3); // palette-size texture

	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 2, 2, 1);
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 256, 255, 1); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 255, 256, 1); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 255, 255, 1); // Test with a texture with an odd number of columns.

	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 2, 2, 3);
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 256, 255, 3); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 255, 256, 3); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForHalfMapWithDims(engine, 255, 255, 3); // Test with a texture with an odd number of columns.

	// Test float map
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 256, 1, 3); // palette-size texture

	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 2, 2, 1);
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 256, 255, 1); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 255, 256, 1); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 255, 255, 1); // Test with a texture with an odd number of columns.

	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 2, 2, 3);
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 256, 255, 3); // Test with a texture with an odd number of rows.
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 255, 256, 3); // Test with a texture with an odd number of columns.
	doTextureChunkedLoadingTestForFloatMapWithDims(engine, 255, 255, 3); // Test with a texture with an odd number of columns.

	// Test some pre-compressed data in KTX files.
	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX");
	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_no_mipmap.KTX2");
	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX");
	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/lightmap_BC6H_with_mipmaps.KTX2");

	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/ktxtest-master/ktx/valid/compression/format_bc1_rgb_unorm.ktx"); // BC1 = DXT1
	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/ktxtest-master/ktx/valid/compression/format_bc3_unorm.ktx"); // BC3 = DXT5


	// Test with a very small max_upload_size_B, to make sure we can still make progress.
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(256, 1, 4); // 256 * 4 = 1024 bytes per row.
		map->set(128);
		doTextureChunkedLoadingTestForMap2D(engine, "test1", map, /*max_upload_size_B=*/100);
	}


	//doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/ktx/ob_51_lightmap_13576612190308084812.ktx2");

	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");

	doTextureChunkedLoadingTestForPath(engine, TestUtils::getTestReposDir() + "/testfiles/pngs/palette_image.png");

#if 0
	{
		//----------------- Load and insert texture into OpenGL Engine.-----------------
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg";

		const std::string key = engine.getTextureServer()->keyForPath(path); // Get canonical path.  May throw TextureServerExcep

		Reference<Map2D> map = ImFormatDecoder::decodeImage(".", key); // Load texture from disk and decode it.

		testAssert(dynamic_cast<const ImageMapUInt8*>(map.ptr()));
		const ImageMapUInt8* imagemap = map.downcastToPtr<ImageMapUInt8>();

		Reference<TextureData> texture_data = TextureLoading::buildUInt8MapTextureData(imagemap, &engine, &engine.getTaskManager());

		// Give data to OpenGL engine
		engine.texture_data_manager->insertBuiltTextureData(key, texture_data);

		//----------------- Now query engine for texture and make sure we get a texture back .-----------------
		Reference<OpenGLTexture> opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key), /*use_sRGB=*/true);
		testAssert(opengl_tex.nonNull());

		// Query again
		opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key), /*use_sRGB=*/true);
		testAssert(opengl_tex.nonNull());
	}


	{
		//----------------- Make an object using a texture, insert into engine -----------------
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/checker.jpg";

		GLObjectRef ob = engine.allocateObject();
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
		Reference<TextureData> texture_data = TextureLoading::buildUInt8MapTextureData(imagemap, &engine, &engine.getTaskManager());

		engine.texture_data_manager->insertBuiltTextureData(key, texture_data); // Give data to OpenGL engine

		//----------------- query engine for texture and make sure we get a texture back .-----------------
		Reference<OpenGLTexture> opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key), /*use_sRGB=*/true);
		testAssert(opengl_tex.nonNull());


		//----------------- Notify the opengl engine that the texture was loaded, and check the object has had the texture assigned. -----------------
		engine.textureLoaded(path, OpenGLTextureKey(key), /*use_sRGB=*/true, /*use_mipmaps=*/true);

		testAssert(ob->materials[0].albedo_texture.nonNull());

		//----------------- Now query engine for texture and make sure we get a texture back .-----------------
		opengl_tex = engine.getTextureIfLoaded(OpenGLTextureKey(key), /*use_sRGB=*/true);
		testAssert(opengl_tex.nonNull());
	}
#endif
	
}


void doTextureLoadingTests(OpenGLEngine& engine)
{
	try
	{
		doTextureChunkedLoadingTests(engine);

		const bool original_use_canonical_paths = engine.getTextureServer()->useCanonicalPaths();
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
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void test(const std::string& indigo_base_dir)
{
	conPrint("OpenGLEngineTests::test()");

	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/testscenes/arrow.igmesh"); // Has both tris and quads
	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/testscenes/poolparty_reduced/mesh_18276362613739127974.igmesh"); // ~100 KB mesh
	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/testscenes/quad_mesh_500x500_verts.igmesh");
	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/dist/benchmark_scenes/Supercar_Benchmark_Optimised/mesh_3732024865775885879.igmesh");
	doTest(indigo_base_dir, TestUtils::getTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");


	// Run on all IGMESH files in testscenes.
	{
		const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir() + "/testscenes", "igmesh");

		for(size_t i=0; i<paths.size(); ++i)
		{
			conPrint(paths[i]);
			doTest(indigo_base_dir, paths[i]);
		}
	}

	conPrint("OpenGLEngineTests::test() done.");
}




void buildData()
{
	try
	{
		Vec2f samples[] = {
			Vec2f(327, 128),
			Vec2f(789, 168),
			Vec2f(507, 219),
			Vec2f(200, 439),
			Vec2f(409, 392),
			Vec2f(599, 401),
			Vec2f(490, 470),
			Vec2f(387, 574),
			Vec2f(546, 535),
			Vec2f(686, 530),
			Vec2f(814, 545),
			Vec2f(496, 648),
			Vec2f(42, 724),
			Vec2f(383, 802),
			Vec2f(599, 716),
			Vec2f(865, 367)
		};

		const int W = 500;
		Bitmap bitmap(W, W, 3, NULL);
		bitmap.zero();

		Array2D<float> density(W, W);
		density.setAllElems(0.f);

		for(int y = 0; y < W; ++y)
		for(int x = 0; x < W; ++x)
		for(int i = 0; i < 16; ++i)
		{
			Vec2f p((float)x / W, (float)y / W);
			const Vec2f sample(samples[i].x * 0.001f, samples[i].y * 0.001f);

			const float d2 = p.getDist(sample);
			const float v = exp(-4 * d2);

			density.elem(x, y) += v * 0.1f;

		}

		for(int y = 0; y < W; ++y)
		for(int x = 0; x < W; ++x)
		{
			const float v = density.elem(x, y);
			bitmap.getPixelNonConst(x, y)[0] = (uint8)(v * 255.f);
			bitmap.getPixelNonConst(x, y)[1] = (uint8)(v * 255.f);
			bitmap.getPixelNonConst(x, y)[2] = (uint8)(v * 255.f);
		}

		PNGDecoder::write(bitmap, "samples.png"); 

		for(int i = 0; i < 16; ++i)
		{
			Vec2f sample = ((samples[i] * 0.001) - Vec2f(0.5, 0.5)) * (4.0 / 2048.0);
			conPrint("vec2(" + floatToStringNSigFigs(sample.x, 5) + ", " + floatToStringNSigFigs(sample.y, 5) + "),");
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS


} // end namespace OpenGLEngineTests
