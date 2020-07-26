/*=====================================================================
BatchedMeshTests.cpp
--------------------
Copyright Glare Technologies Limited 2020
=====================================================================*/
#include "BatchedMeshTests.h"


#if BUILD_TESTS


#include "BatchedMesh.h"
#include "FormatDecoderGLTF.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"


static void testWritingAndReadingMesh(const BatchedMesh& batched_mesh)
{
	try
	{
		const std::string temp_path = PlatformUtils::getTempDirPath() + "/temp678.bmesh";

		// Write without compression, read back from disk, and check unchanged in round trip.
		{
			BatchedMesh::WriteOptions write_options;
			write_options.use_compression = false;
			batched_mesh.writeToFile(temp_path, write_options);

			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);

			testAssert(batched_mesh == batched_mesh2);
		}

		// Write with compression, read back from disk, and check unchanged in round trip.
		{
			BatchedMesh::WriteOptions write_options;
			write_options.use_compression = true;
			batched_mesh.writeToFile(temp_path, write_options);

			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);

			testAssert(batched_mesh.index_data.size() == batched_mesh2.index_data.size());
			if(batched_mesh.index_data != batched_mesh2.index_data)
			{
				for(size_t i=0; i<batched_mesh.index_data.size(); ++i)
				{
					conPrint("");
					conPrint("batched_mesh .index_data[" + toString(i) + "]: " + toString(batched_mesh.index_data[i]));
					conPrint("batched_mesh2.index_data[" + toString(i) + "]: " + toString(batched_mesh2.index_data[i]));
				}
			}
			testAssert(batched_mesh.index_data == batched_mesh2.index_data);
			testAssert(batched_mesh.vertex_data == batched_mesh2.vertex_data);

			testAssert(batched_mesh == batched_mesh2);
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


static void testIndigoMeshConversion(const BatchedMesh& batched_mesh)
{
	try
	{
		// Test conversion to Indigo mesh
		Indigo::Mesh indigo_mesh;
		batched_mesh.buildIndigoMesh(indigo_mesh);

		testAssert(indigo_mesh.vert_positions.size() == batched_mesh.numVerts());
		testAssert(indigo_mesh.vert_normals.size() == 0 || batched_mesh.numVerts());
		testAssert(indigo_mesh.triangles.size() == batched_mesh.numIndices() / 3);

		// Convert Indigo mesh back to batched mesh
		BatchedMesh batched_mesh2;
		batched_mesh2.buildFromIndigoMesh(indigo_mesh);

		bool has_NaN = false;
		for(size_t i=0; i<indigo_mesh.uv_pairs.size(); ++i)
			if(isNAN(indigo_mesh.uv_pairs[i].x) || isNAN(indigo_mesh.uv_pairs[i].y))
				has_NaN = true;

		if(!has_NaN)
		{
			testAssert(batched_mesh2.vertexSize() == batched_mesh.vertexSize());
			testAssert(batched_mesh2.numVerts() == batched_mesh.numVerts());
			testAssert(batched_mesh2.numIndices() == batched_mesh.numIndices());
		}
		else
			conPrint("Mesh has NaN UVs!");
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


static void perfTestWithMesh(const std::string& path)
{
	conPrint("");
	conPrint("===================================================");
	conPrint("Perf test with " + path);
	conPrint("===================================================");
	Indigo::Mesh indigo_mesh;
	if(hasExtension(path, "glb"))
	{
		GLTFMaterials mats;
		FormatDecoderGLTF::loadGLBFile(path, indigo_mesh, 1.f, mats);
	}
	else if(hasExtension(path, "gltf"))
	{
		GLTFMaterials mats;
		FormatDecoderGLTF::streamModel(path, indigo_mesh, 1.f, mats);
	}
	else if(hasExtension(path, "igmesh"))
	{
		Indigo::Mesh::readFromFile(toIndigoString(path), indigo_mesh);
	}


	BatchedMesh batched_mesh;
	batched_mesh.buildFromIndigoMesh(indigo_mesh);

	const std::string temp_path = PlatformUtils::getTempDirPath() + "/temp6789.bmesh";

	BatchedMesh::WriteOptions write_options;
	write_options.use_compression = true;

	const int compression_levels[] ={ 1, 3, 6 };// , 9, 20};

	for(int i=0; i<staticArrayNumElems(compression_levels); ++i)
	{
		write_options.compression_level = compression_levels[i];

		conPrint("\nWriting with compression level " + toString(write_options.compression_level));
		conPrint("----------------------------------------------");
		batched_mesh.writeToFile(temp_path, write_options);

		// Load from disk to get decompression speed.
		{
			Timer timer;
			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);
			conPrint("readFromFile() time: " + timer.elapsedStringNSigFigs(4));
		}
	}
}


void BatchedMeshTests::test()
{
	conPrint("BatchedMeshTests::test()");

	try
	{
		{
			Indigo::Mesh m;
			m.num_uv_mappings = 1;
			m.used_materials = Indigo::Vector<Indigo::String>(1, Indigo::String("mat1"));

			m.vert_positions.resize(8);
			for(size_t i=0; i<8; ++i)
				m.vert_positions[i] = Indigo::Vec3f((float)i, 2.f, 3.f);
			m.vert_normals = Indigo::Vector<Indigo::Vec3f>(8, Indigo::Vec3f(0.f, 1.f, 0.f));

			m.uv_pairs.resize(3);
			m.uv_pairs[0] = Indigo::Vec2f(1.f, 2.f);
			m.uv_pairs[1] = Indigo::Vec2f(4.f, 5.f);
			m.uv_pairs[2] = Indigo::Vec2f(8.f, 9.f);

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 0; m.triangles.back().vertex_indices[1] = 1; m.triangles.back().vertex_indices[2] = 2;
			m.triangles.back().uv_indices[0] = 0;     m.triangles.back().uv_indices[1] = 0;     m.triangles.back().uv_indices[2] = 0;
			m.triangles.back().tri_mat_index = 0;

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 5; m.triangles.back().vertex_indices[1] = 2; m.triangles.back().vertex_indices[2] = 7;
			m.triangles.back().uv_indices[0] = 0; m.triangles.back().uv_indices[1] = 0; m.triangles.back().uv_indices[2] = 0;
			m.triangles.back().tri_mat_index = 1;

			m.quads.push_back(Indigo::Quad());
			m.quads.back().vertex_indices[0] = 1; m.quads.back().vertex_indices[1] = 2; m.quads.back().vertex_indices[2] = 2, m.quads.back().vertex_indices[2] = 3;
			m.quads.back().uv_indices[0] = 0; m.quads.back().uv_indices[1] = 0; m.quads.back().uv_indices[2] = 0, m.quads.back().uv_indices[3] = 0;
			m.quads.back().mat_index = 2;

			m.quads.push_back(Indigo::Quad());
			m.quads.back().vertex_indices[0] = 5; m.quads.back().vertex_indices[1] = 6; m.quads.back().vertex_indices[2] = 7, m.quads.back().vertex_indices[2] = 4;
			m.quads.back().uv_indices[0] = 0; m.quads.back().uv_indices[1] = 0; m.quads.back().uv_indices[2] = 0, m.quads.back().uv_indices[3] = 0;
			m.quads.back().mat_index = 3;

			m.endOfModel();


			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(m);

			testWritingAndReadingMesh(batched_mesh);

			// Test conversion back to Indigo mesh
			Indigo::Mesh indigo_mesh2;
			batched_mesh.buildIndigoMesh(indigo_mesh2);
			testAssert(indigo_mesh2.vert_positions.size() == m.vert_positions.size());
			testAssert(indigo_mesh2.vert_normals.size() == m.vert_normals.size());
			testAssert(indigo_mesh2.uv_pairs.size() == 8); // Will get expanded to one per vert.
			testAssert(indigo_mesh2.triangles.size() == 6); // Quads will get converted to tris.
			testAssert(indigo_mesh2.triangles[0].vertex_indices[0] == 0 && indigo_mesh2.triangles[0].vertex_indices[1] == 1 && indigo_mesh2.triangles[0].vertex_indices[2] == 2);

			testIndigoMeshConversion(batched_mesh);
		}

		// Check that vertex merging gets done properly in buildFromIndigoMesh().
		// Make an Indigo mesh with two triangles that use the same vertex indices, and the same UVs at each vertex.
		// Make sure only 3 vertices are created for the BatchedMesh.
		{
			Indigo::Mesh m;
			m.num_uv_mappings = 1;
			m.used_materials = Indigo::Vector<Indigo::String>(1, Indigo::String("mat1"));

			m.vert_positions.resize(6);
			for(size_t i=0; i<6; ++i)
				m.vert_positions[i] = Indigo::Vec3f((float)i, 2.f, 3.f);
			m.vert_normals = Indigo::Vector<Indigo::Vec3f>(6, Indigo::Vec3f(0.f, 1.f, 0.f));

			m.uv_pairs = Indigo::Vector<Indigo::Vec2f>(6, Indigo::Vec2f(5.f, 1.f));

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 0; m.triangles.back().vertex_indices[1] = 1; m.triangles.back().vertex_indices[2] = 2;
			m.triangles.back().uv_indices[0]     = 0; m.triangles.back().uv_indices[1]     = 1; m.triangles.back().uv_indices[2]     = 2;
			m.triangles.back().tri_mat_index = 0;

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 0; m.triangles.back().vertex_indices[1] = 1; m.triangles.back().vertex_indices[2] = 2;
			m.triangles.back().uv_indices[0]     = 3; m.triangles.back().uv_indices[1]     = 4; m.triangles.back().uv_indices[2]     = 5;
			m.triangles.back().tri_mat_index = 0;

			m.endOfModel();


			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(m);

			testEqual(batched_mesh.numVerts(), (size_t)3);

			testWritingAndReadingMesh(batched_mesh);
			testIndigoMeshConversion(batched_mesh);
		}


		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getIndigoTestReposDir()) + "/testscenes/mesh_2107654449_802486.igmesh", indigo_mesh);
			testAssert(indigo_mesh.triangles.size() == 7656);

			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(indigo_mesh);

			testWritingAndReadingMesh(batched_mesh);
			testIndigoMeshConversion(batched_mesh);
		}

		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getIndigoTestReposDir()) + "/testscenes/quad_mesh_500x500_verts.igmesh", indigo_mesh);
			testAssert(indigo_mesh.quads.size() == 249001);

			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(indigo_mesh);

			testWritingAndReadingMesh(batched_mesh);
			testIndigoMeshConversion(batched_mesh);
		}

		// This mesh as 2 uvs sets.
		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getIndigoTestReposDir()) + "/testscenes/bump_ridge_test_saved_meshes/mesh_16314352959183639561.igmesh", indigo_mesh);
			testAssert(indigo_mesh.num_uv_mappings == 2);

			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(indigo_mesh);
			testAssert(batched_mesh.findAttribute(BatchedMesh::VertAttribute_UV_1) != NULL);

			testWritingAndReadingMesh(batched_mesh);
			testIndigoMeshConversion(batched_mesh);
		}


		// For all IGMESH files in testscenes, convert to BatchedMesh then test saving and loading.
		{
			std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getIndigoTestReposDir() + "/testscenes", "igmesh");
			std::sort(paths.begin(), paths.end());

			for(size_t i=0; i<paths.size(); ++i)
			{
				const std::string path = paths[i];
				conPrint(path);
				Indigo::Mesh indigo_mesh;
				Indigo::Mesh::readFromFile(toIndigoString(path), indigo_mesh);

				BatchedMesh batched_mesh;
				batched_mesh.buildFromIndigoMesh(indigo_mesh);

				testWritingAndReadingMesh(batched_mesh);
				testIndigoMeshConversion(batched_mesh);
			}
		}


		// Perf test - test compression and decompression speed at varying compression levels
		if(false)
		{
			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/testfiles/gltf/2CylinderEngine.glb");

			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/testfiles/gltf/duck/Duck.gltf");

			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");
		}

		/*
		Some results, CPU = Intel(R) Core(TM) i7-8700K CPU:

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/2CylinderEngine.glb
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     830798 B
		Compression ratio:   3.3852561031682793
		Compression took     0.007963000040035695 s (420.87852534918136 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     748540 B
		Compression ratio:   3.757266144761803
		Compression took     0.01277719996869564 s (226.54076412086405 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     721103 B
		Compression ratio:   3.9002250718690674
		Compression took     0.030561399995349348 s (91.6447029163987 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     710084 B
		Compression ratio:   3.9607483058342394
		Compression took     0.04172229999676347 s (66.01593639085978 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     620024 B
		Compression ratio:   4.536056668774112
		Compression took     0.4796337999869138 s (5.6213213240073445 MB/s)

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/duck/Duck.gltf
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     64346 B
		Compression ratio:   1.2875392409784603
		Compression took     0.001195400021970272 s (248.85044571488913 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     63666 B
		Compression ratio:   1.3012911129959477
		Compression took     0.0013697000104002655 s (122.42022596370255 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61667 B
		Compression ratio:   1.3434738190604376
		Compression took     0.0025105999666266143 s (44.31296226204313 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61568 B
		Compression ratio:   1.3456340956340955
		Compression took     0.0031196000054478645 s (41.82415407470109 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     60575 B
		Compression ratio:   1.3676929426330995
		Compression took     0.012172200018540025 s (7.119429927248082 MB/s)

		===================================================
		Perf test with O:\indigo\trunk/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     47793085 B
		Compression ratio:   2.8499493598289374
		Compression took     0.21159790002275258 s (618.2025166677035 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     46020837 B
		Compression ratio:   2.9597000158862823
		Compression took     0.4927970999851823 s (264.5570254046427 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45541753 B
		Compression ratio:   2.9908350695240036
		Compression took     2.107264499994926 s (61.670013889674586 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45542444 B
		Compression ratio:   2.9907896906015847
		Compression took     2.1111218000296503 s (61.59432004020065 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     40178301 B
		Compression ratio:   3.390085409534863
		Compression took     12.154803499986883 s (10.687562677819832 MB/s)
		BatchedMesh::test() done.





		With separate compression of the index and vertex data:




		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/2CylinderEngine.glb
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     829078 B
		Compression ratio:   3.3922791341707295
		Compression took     0.006650500057730824 s (403.3042116132777 MB/s)
		Decompression took 0.003736 (718.0MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     748083 B
		Compression ratio:   3.7595614390381815
		Compression took     0.011418199981562793 s (234.90345999791134 MB/s)
		Decompression took 0.003739 (717.3MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     720478 B
		Compression ratio:   3.903608437731617
		Compression took     0.028009999950882047 s (95.75775392076446 MB/s)
		Decompression took 0.003587 (747.9MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     709443 B
		Compression ratio:   3.964326943813668
		Compression took     0.038837999978568405 s (69.06057686022106 MB/s)
		Decompression took 0.003411 (786.3MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     620023 B
		Compression ratio:   4.536063984723147
		Compression took     0.4449350000359118 s (6.028239366201136 MB/s)
		Decompression took 0.004185 (640.9MB/s)

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/duck/Duck.gltf
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61888 B
		Compression ratio:   1.3386763185108583
		Compression took     0.0003358999965712428 s (235.21884659759843 MB/s)
		Decompression took 0.0001917 (412.2MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61590 B
		Compression ratio:   1.3451534339990259
		Compression took     0.0005754000158049166 s (137.31318664477152 MB/s)
		Decompression took 0.0001941 (407.1MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59877 B
		Compression ratio:   1.383636454732201
		Compression took     0.0016341999871656299 s (48.347821800354204 MB/s)
		Decompression took 0.0001986 (397.8MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59744 B
		Compression ratio:   1.3867166577396894
		Compression took     0.0016995000187307596 s (46.490149393838884 MB/s)
		Decompression took 0.0002542 (310.8MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59582 B
		Compression ratio:   1.3904870598502903
		Compression took     0.010535200068261474 s (7.499621198808736 MB/s)
		Decompression took 0.0002182 (362.1MB/s)

		===================================================
		Perf test with O:\indigo\trunk/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     47807468 B
		Compression ratio:   2.8490919452165926
		Compression took     0.20913650002330542 s (621.1156311991194 MB/s)
		Decompression took 0.1487 (873.5MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45860861 B
		Compression ratio:   2.9700243089635845
		Compression took     0.48414219997357577 s (268.3053640559318 MB/s)
		Decompression took 0.1516 (856.8MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45540911 B
		Compression ratio:   2.9908903666859015
		Compression took     1.7893327999627218 s (72.59574586765315 MB/s)
		Decompression took 0.1620 (801.8MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45541879 B
		Compression ratio:   2.9908267948276794
		Compression took     2.1417220999719575 s (60.65116908512585 MB/s)
		Decompression took 0.1543 (841.7MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     40175387 B
		Compression ratio:   3.3903312991110703
		Compression took     12.293474600010086 s (10.566414577262268 MB/s)
		Decompression took 0.1857 (699.5MB/s)

		*/
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	catch(Indigo::IndigoException& e)
	{
		failTest(toStdString(e.what()));
	}

	conPrint("BatchedMeshTests::test() done.");
}


#endif // BUILD_TESTS
