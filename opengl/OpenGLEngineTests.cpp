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
#ifndef NO_GIF_SUPPORT
#include <graphics/GifDecoder.h>
#endif
#include <set>


namespace OpenGLEngineTests
{

#if BUILD_TESTS


//==================================== Gaussian splat cloud ordering ====================================
//
// Tests orderSplatCloudsBackToFront(), which puts splat clouds in back-to-front draw order by recursively partitioning
// them on axis-aligned planes.  See the comments on it in OpenGLEngine.cpp.
//
// What makes an order correct is a property, not a particular permutation, and it is weaker than "sorted by distance":
// a pair of clouds only constrains the order if a ray from the camera can hit both of them.  See
// splatCloudMustBeDrawnAfter() below, which is what these check against.


// All the ordering pass reads off a cloud is its world-space AABB, so that's all these carry.
static GLObjectRef makeSplatCloudTestOb(const Vec4f& min_ws, const Vec4f& max_ws)
{
	GLObjectRef ob = new GLObject();
	ob->aabb_ws = js::AABBox(min_ws, max_ws);
	return ob;
}


static void orderSplatCloudTestObs(const std::vector<GLObjectRef>& obs, const Vec4f& campos_ws, js::Vector<const GLObject*, 16>& clouds_out)
{
	clouds_out.resize(obs.size());
	for(size_t i=0; i<obs.size(); ++i)
		clouds_out[i] = obs[i].ptr();

	js::Vector<SplatCloudRange, 16> range_stack;
	orderSplatCloudsBackToFront(clouds_out.data(), clouds_out.size(), campos_ws, range_stack);
}


// The ordering permutes the array in place, so whatever else it does, the result has to be the clouds it started with -
// no cloud dropped, none drawn twice.
static void checkSplatCloudOrderIsPermutation(const std::vector<GLObjectRef>& obs, const js::Vector<const GLObject*, 16>& clouds)
{
	testAssert(clouds.size() == obs.size());

	std::set<const GLObject*> in, out;
	for(size_t i=0; i<obs.size(); ++i)
		in.insert(obs[i].ptr());
	for(size_t i=0; i<clouds.size(); ++i)
		out.insert(clouds[i]);

	testAssert(in == out);
}


/*
Returns whether cloud a has to be drawn after cloud b - that is, whether a is the nearer of the two along some ray from
the camera that hits both.

Two clouds only constrain each other where a ray can hit both of them, and disjoint clouds often can't be: on an axis
that separates them a ray crosses the gap at most once, so whichever cloud is on the camera's side is hit first, but if
the camera sits *inside* the gap then rays reaching one travel away from the other and no ray hits both.  Those pairs
may be drawn in either order.

So this is deliberately weaker than "is nearer".  A correct order can put a farther cloud second - what it can't do is
put a cloud in front of one that occludes it.  Asserting on distance alone would fail valid orders: with the camera in
among a row of clouds, a partition can legitimately draw an entire far group before a near group that contains a cloud
farther away than some of it.

Where two axes both separate the pair and disagree about which is nearer, no ray can hit both either, so that is a
don't-care as well.  Checking every axis rather than the first one that separates them keeps this independent of the
axis-priority convention splatCloudIsNearer() uses.
*/
static bool splatCloudMustBeDrawnAfter(const js::AABBox& a, const js::AABBox& b, const Vec4f& campos_ws)
{
	bool a_nearer = false, b_nearer = false;
	for(int axis=0; axis<3; ++axis)
	{
		float gap_min, gap_max;
		bool a_is_low;
		if(a.max_[axis] < b.min_[axis])
		{
			gap_min = a.max_[axis]; gap_max = b.min_[axis]; a_is_low = true;
		}
		else if(b.max_[axis] < a.min_[axis])
		{
			gap_min = b.max_[axis]; gap_max = a.min_[axis]; a_is_low = false;
		}
		else
			continue; // The two overlap on this axis, so it doesn't separate them.

		if(campos_ws[axis] > gap_min && campos_ws[axis] < gap_max)
			return false; // Camera inside the gap: no ray hits both clouds, so either order will do.

		if((campos_ws[axis] <= gap_min) == a_is_low) // Camera on a's side of the gap, so a is hit first along any ray that hits both:
			a_nearer = true;
		else
			b_nearer = true;
	}

	return a_nearer && !b_nearer;
}


// Checks the order is back-to-front: no cloud is drawn before one it would be composited over.
static void checkSplatCloudOrderIsBackToFront(const js::Vector<const GLObject*, 16>& clouds, const Vec4f& campos_ws)
{
	for(size_t i=0; i<clouds.size(); ++i)
		for(size_t j=i+1; j<clouds.size(); ++j)
			testAssert(!splatCloudMustBeDrawnAfter(clouds[i]->aabb_ws, clouds[j]->aabb_ws, campos_ws));
}


static void testSplatCloudOrderingIsValid(const std::vector<GLObjectRef>& obs, const Vec4f& campos_ws)
{
	js::Vector<const GLObject*, 16> clouds;
	orderSplatCloudTestObs(obs, campos_ws, clouds);

	checkSplatCloudOrderIsPermutation(obs, clouds);
	checkSplatCloudOrderIsBackToFront(clouds, campos_ws);
}


/*
Places clouds in distinct cells of a grid, each one a random box inside its cell.

Two constraints on the boxes, both of which the arrangements this has to model already satisfy:

Every box is inset from its cell walls, so clouds in adjacent cells have a real gap between them rather than touching.
That is the invariant GaussianSplatRenderer maintains, by merging any two clouds whose bounds intersect.

Every box also covers the middle of its cell on each axis, so two clouds sharing a cell coordinate on some axis always
overlap on that axis rather than being separated on it, and the pair is ordered on the first axis where their cells
differ.  That keeps the arrangements close enough to a plain grid that a valid draw order exists for all of them - none
of the sets this generates with the seed and counts below is cyclic.  That was measured, not proved: two clouds sharing
a cell can still have different extents on that axis, and so order differently against a third cloud elsewhere, which
is a cycle.  Worth knowing if the counts or the trial count are ever changed, since a cyclic set has no order able to
satisfy it and would fail.
*/
static void makeRandomSplatCloudGrid(PCG32& rng, size_t num_obs, int grid_res, std::vector<GLObjectRef>& obs_out)
{
	const size_t num_cells = (size_t)grid_res * grid_res * grid_res;
	std::vector<size_t> cells(num_cells);
	for(size_t i=0; i<num_cells; ++i)
		cells[i] = i;
	for(size_t i=0; i+1<num_cells; ++i) // Shuffle, so the clouds land in unrelated cells rather than in scan order.
		std::swap(cells[i], cells[i + (size_t)rng.nextUInt((uint32)(num_cells - i))]);

	obs_out.resize(0);
	for(size_t i=0; i<num_obs; ++i)
	{
		const size_t cell = cells[i];
		const Vec4f cell_min((float)(cell % grid_res), (float)((cell / grid_res) % grid_res), (float)(cell / ((size_t)grid_res * grid_res)), 0);

		Vec4f box_min(1.f), box_max(1.f);
		for(int axis=0; axis<3; ++axis)
		{
			box_min[axis] = cell_min[axis] + 0.05f + rng.unitRandom() * 0.25f; // In [0.05, 0.3] within the cell.
			box_max[axis] = cell_min[axis] + 0.70f + rng.unitRandom() * 0.25f; // In [0.70, 0.95] within the cell.
		}
		obs_out.push_back(makeSplatCloudTestOb(box_min, box_max));
	}
}


static void testSplatCloudOrdering()
{
	conPrint("testSplatCloudOrdering()");

	//------------ Two clouds separated on x: the order has to flip as the camera crosses the gap ------------
	{
		std::vector<GLObjectRef> obs;
		obs.push_back(makeSplatCloudTestOb(Vec4f(0,0,0,1), Vec4f(1,1,1,1)));
		obs.push_back(makeSplatCloudTestOb(Vec4f(3,0,0,1), Vec4f(4,1,1,1)));

		js::Vector<const GLObject*, 16> clouds;

		orderSplatCloudTestObs(obs, Vec4f(-10, 0.5f, 0.5f, 1), clouds);
		testAssert(clouds[0] == obs[1].ptr()); // Camera off to -x, so the +x cloud is the far one and is drawn first.

		orderSplatCloudTestObs(obs, Vec4f(10, 0.5f, 0.5f, 1), clouds);
		testAssert(clouds[0] == obs[0].ptr()); // Camera off to +x, so the order reverses.
	}

	//------------ A cloud straddling a candidate plane can't be assigned a side ------------
	// A spans x=5 and B lies entirely beyond it, but the two are disjoint only on y.  A partition on x that put the
	// straddler A on the camera's side would draw B first, and yet a ray from this camera hits B and then A, so A is
	// the far one.  This is why findSplatCloudSplitPlane() only ever splits on an empty gap.
	{
		std::vector<GLObjectRef> obs;
		obs.push_back(makeSplatCloudTestOb(Vec4f(4, 0, 0, 1), Vec4f(6, 1, 1, 1))); // A
		obs.push_back(makeSplatCloudTestOb(Vec4f(5, 2, 0, 1), Vec4f(8, 3, 1, 1))); // B

		js::Vector<const GLObject*, 16> clouds;
		orderSplatCloudTestObs(obs, Vec4f(4, 10, 0.5f, 1), clouds);
		testAssert(clouds[0] == obs[0].ptr()); // A drawn first.
	}

	//------------ One wide cloud covering the middle of the extent ------------
	// The plane starts at the middle of the clouds' extent on the axis, which the wide cloud spans, so walking up from
	// there runs off the end of the axis.  The gap on x lies below the middle, so x only yields a split if the search
	// also walks down.
	{
		std::vector<GLObjectRef> obs;
		for(int i=0; i<40; ++i) // A row of small clouds down at the low end of x.
			obs.push_back(makeSplatCloudTestOb(Vec4f(0, (float)i * 2, 0, 1), Vec4f(1, (float)i * 2 + 1, 1, 1)));
		obs.push_back(makeSplatCloudTestOb(Vec4f(10, 0, 10, 1), Vec4f(1000, 100, 11, 1))); // Wide, and separated from the row on x and z.

		testSplatCloudOrderingIsValid(obs, Vec4f(-50, 30, 0.5f, 1));
		testSplatCloudOrderingIsValid(obs, Vec4f(2000, 30, 0.5f, 1));
		testSplatCloudOrderingIsValid(obs, Vec4f(5, 30, 5, 1)); // Camera in between.
	}

	//------------ No separating plane on any axis ------------
	// Four clouds in a pinwheel: on every axis their projections form one connected run, so there is no gap anywhere to
	// split on and findSplatCloudSplitPlane() gives up on all three.  A pinwheel is also where cycles in "is nearer"
	// come from - with one, no order satisfies every pair - so only the permutation property is checked here.  The
	// point of the case is that the fallback path runs and doesn't lose clouds.
	{
		std::vector<GLObjectRef> obs;
		obs.push_back(makeSplatCloudTestOb(Vec4f(0, 0, 0, 1), Vec4f(3, 1, 1, 1)));
		obs.push_back(makeSplatCloudTestOb(Vec4f(3, 0, 0, 1), Vec4f(4, 3, 1, 1)));
		obs.push_back(makeSplatCloudTestOb(Vec4f(1, 3, 0, 1), Vec4f(4, 4, 1, 1)));
		obs.push_back(makeSplatCloudTestOb(Vec4f(0, 1, 0, 1), Vec4f(1, 4, 1, 1)));

		PCG32 rng(1);
		for(int t=0; t<200; ++t)
		{
			const Vec4f campos((rng.unitRandom() - 0.5f) * 20, (rng.unitRandom() - 0.5f) * 20, (rng.unitRandom() - 0.5f) * 20, 1);

			js::Vector<const GLObject*, 16> clouds;
			orderSplatCloudTestObs(obs, campos, clouds);
			checkSplatCloudOrderIsPermutation(obs, clouds);
		}
	}

	//------------ Random disjoint clouds ------------
	// This is the case that caught the partitioning bottoming out into a pairwise sort of the last dozen clouds: those
	// sets have perfectly good draw orders, but a cyclic "is nearer" relation, so sorting on it produced orders that
	// violated pairs which really were observable.  Around 2% of the sets below fail if that shortcut comes back.
	{
		PCG32 rng(1);
		const size_t counts[] = { 0, 1, 2, 3, 5, 11, 12, 13, 17, 40, 137, 300 };
		for(size_t c=0; c<staticArrayNumElems(counts); ++c)
			for(int t=0; t<40; ++t)
			{
				std::vector<GLObjectRef> obs;
				makeRandomSplatCloudGrid(rng, counts[c], /*grid_res=*/8, obs);

				// Cameras among the clouds, just outside them, and a long way off.
				const float spread = (t % 3 == 0) ? 8.f : ((t % 3 == 1) ? 30.f : 1000.f);
				const Vec4f campos((rng.unitRandom() - 0.25f) * spread, (rng.unitRandom() - 0.25f) * spread, (rng.unitRandom() - 0.25f) * spread, 1);

				testSplatCloudOrderingIsValid(obs, campos);
			}
	}
}


static void doTest(const std::string& /*indigo_base_dir*/, const std::string& mesh_path)
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

				const size_t index_type_size = (mesh_renderdata->getIndexType() == GL_UNSIGNED_BYTE) ? 1 : ((mesh_renderdata->getIndexType() == GL_UNSIGNED_SHORT) ? 2 : 4);

				size_t num_indices = 0;
				size_t expected_cur_offset = 0;
				for(size_t i=0; i<mesh_renderdata->batches.size(); ++i)
				{
					testAssert(expected_cur_offset == mesh_renderdata->batches[i].prim_start_offset_B);

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

	const bool use_compression = opengl_engine->DXTTextureCompressionSupportedAndEnabled();
	Reference<TextureData> texture_data = TextureProcessing::buildTextureData(map.ptr(), engine.mem_allocator.ptr(), engine.getMainTaskManager(), use_compression, /*build mipmaps=*/true, /*convert_float_to_half=*/true);

	OpenGLTextureLoadingProgress loading_progress;
	testAssert(!loading_progress.loadingInProgress());
	TextureParams texture_params;
	TextureLoading::initialiseTextureLoadingProgress(Reference<OpenGLEngine>(&engine), OpenGLTextureKey(key), texture_params, texture_data, loading_progress);
	testAssert(loading_progress.loadingInProgress());

	const int MAX_ITERS = 100000;
	int i = 0;
	for(; i<MAX_ITERS; ++i)
	{
		testAssert(loading_progress.loadingInProgress());
		size_t total_bytes_uploaded = 0;
		TextureLoading::partialLoadTextureIntoOpenGL(loading_progress, total_bytes_uploaded, max_total_upload_bytes);
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
	{
#ifndef NO_GIF_SUPPORT
		map = GIFDecoder::decodeImageSequence(key);
#endif
	}
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

	testSplatCloudOrdering(); // Doesn't need a GL context or any test data.
#if 0

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
#endif

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
			Vec2f sample = ((samples[i] * 0.001f) - Vec2f(0.5f, 0.5f)) * (4.0f / 2048.0);
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
