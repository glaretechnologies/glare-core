/*=====================================================================
MeshSimplification.cpp
----------------------
Copyright Glare Technologies Limited 2022
=====================================================================*/
#include "MeshSimplification.h"


#include "BatchedMesh.h"
#include "../meshoptimizer/src/meshoptimizer.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"


namespace MeshSimplification
{


// target_error represents the error relative to mesh extents that can be tolerated, e.g. 0.01 = 1% deformation
BatchedMeshRef buildSimplifiedMesh(const BatchedMesh& mesh, float target_reduction_ratio, float target_error, bool sloppy)
{
	assert(target_reduction_ratio >= 1);
	assert(target_error >= 0 && target_error <= 1);

	BatchedMeshRef simplified_mesh = new BatchedMesh();
	simplified_mesh->vert_attributes = mesh.vert_attributes;
	simplified_mesh->aabb_os = mesh.aabb_os;

	const size_t vertex_size = mesh.vertexSize(); // In bytes
	 
	js::Vector<uint32, 16> new_indices;
	new_indices.reserve(mesh.numIndices());
	js::Vector<uint8, 16> new_vertex_data(mesh.vertex_data.size());
	uint32 new_vertex_data_write_i = 0;
	std::vector<BatchedMesh::IndicesBatch> new_batches;

	std::vector<unsigned int> new_vert_index(mesh.numVerts(), 0xFFFFFFFFu); // Map from old vert index to new vert index, or -1 if not used

	const BatchedMesh::VertAttribute& pos_attr = mesh.getAttribute(BatchedMesh::VertAttribute_Position);
	if(pos_attr.component_type != BatchedMesh::ComponentType_Float)
		throw glare::Exception("Mesh simplification needs float position type.");

	js::Vector<uint32, 16> temp_batch_indices;
	js::Vector<uint32, 16> simplified_indices;
	for(size_t b=0; b<mesh.batches.size(); ++b)
	{
		const BatchedMesh::IndicesBatch& batch = mesh.batches[b];

		if((batch.num_indices % 3) != 0)
			throw glare::Exception("Mesh simplification requires batch num indices to be a multiple of 3.");

		// Build vector of uint32 indices for this batch
		temp_batch_indices.resizeNoCopy(batch.num_indices);
		for(size_t i=0; i<(size_t)batch.num_indices; ++i)
			temp_batch_indices[i] = mesh.getIndexAsUInt32(batch.indices_start + i);

		const size_t target_index_count = (size_t)(temp_batch_indices.size() / target_reduction_ratio);
		float result_error = 0;

		simplified_indices.resizeNoCopy(temp_batch_indices.size());
		size_t res_num_indices;
		if(sloppy)
		{
			res_num_indices = meshopt_simplifySloppy(/*destination=*/simplified_indices.data(), temp_batch_indices.data(), temp_batch_indices.size(),
				(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
				mesh.numVerts(), // vert count
				mesh.vertexSize(), // vert stride
				target_index_count,
				target_error,
				&result_error
			);
		}
		else
		{
			res_num_indices = meshopt_simplify(/*destination=*/simplified_indices.data(), temp_batch_indices.data(), temp_batch_indices.size(),
				(const float*)&mesh.vertex_data[pos_attr.offset_B], // vert positions
				mesh.numVerts(), // vert count
				mesh.vertexSize(), // vert stride
				target_index_count,
				target_error,
				&result_error
			);
		}

		assert(res_num_indices <= simplified_indices.size());

		//js::Vector<uint8, 16> vertex_data;
		//std::vector<uint32> indices;
		/*meshopt_optimizeVertexFetch(
			vertex_data.data(), // destination - resulting vertex buffer
			simplified_indices.data(), // indices is used both as an input and as an output index buffer
			simplified_indices.size(), // index_count 
			vertex_data.data(), // vertices (source vertices?)
			num_vertices, // vertex count
			vertex_size); // vertex_size
		*/

		// Copy vertices used by the simplified indices to new_vertex_data, if they are not in there already.
		// Update simplified_indices to use the new indices as we do this.
		for(size_t i=0; i<res_num_indices; ++i)
		{
			const uint32 v_i = simplified_indices[i];
			uint32 new_v_i;
			if(new_vert_index[v_i] == 0xFFFFFFFFu)
			{
				new_v_i = new_vertex_data_write_i++;
				new_vert_index[v_i] = new_v_i;

				// Copy vert data to new_vertex_data
				std::memcpy(&new_vertex_data[new_v_i * vertex_size], &mesh.vertex_data[v_i * vertex_size], vertex_size);
			}
			else
				new_v_i = new_vert_index[v_i];

			simplified_indices[i] = new_v_i;
		}


		// Add simplified indices to new_indices
		const size_t write_i = new_indices.size();
		new_indices.resize(write_i + res_num_indices);
		for(size_t i=0; i<res_num_indices; ++i)
			new_indices[write_i + i] = simplified_indices[i];

		BatchedMesh::IndicesBatch new_batch;
		new_batch.indices_start = (uint32)write_i;
		new_batch.material_index = batch.material_index;
		new_batch.num_indices = (uint32)res_num_indices;
		new_batches.push_back(new_batch);
	}

	const uint32 new_num_verts = new_vertex_data_write_i;
	new_vertex_data.resize(new_num_verts * vertex_size); // Trim down to verts actually used.

#ifndef NDEBUG
	// Check all indices are in-bounds
	for(size_t i=0; i<new_indices.size(); ++i)
	{
		assert(new_indices[i] < new_num_verts);
	}
#endif

	// Copy new index data
	simplified_mesh->setIndexDataFromIndices(new_indices, new_num_verts);

	// Copy new vertex data
	simplified_mesh->vertex_data = new_vertex_data;

	simplified_mesh->batches = new_batches;

	simplified_mesh->animation_data = mesh.animation_data;

	if(false)
	{
		conPrint("-------------- simplified mesh ------------");
		conPrint("Original num indices: " + toString(mesh.numIndices()));
		conPrint("new num indices:      " + toString(simplified_mesh->numIndices()));
		conPrint("reduction ratio:      " + toString((float)mesh.numIndices() / simplified_mesh->numIndices()));
		conPrint("Original num verts:   " + toString(mesh.numVerts()));
		conPrint("new num verts:        " + toString(simplified_mesh->numVerts()));
		conPrint("reduction ratio:      " + toString((float)mesh.numVerts() / simplified_mesh->numVerts()));
	}

	return simplified_mesh;
}


} // end namespace MeshSimplification


#if BUILD_TESTS


#include "BatchedMesh.h"
#include "FormatDecoderGLTF.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Exception.h"
#include "../utils/Timer.h"


namespace MeshSimplification
{


void testOnBMesh(const std::string& src_path)
{
	try
	{
		BatchedMeshRef batched_mesh = BatchedMesh::readFromFile(src_path);

		BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, 10.f, 0.02f, /*sloppy=*/false);

		const std::string dest_path = eatExtension(FileUtils::getFilename(src_path)) + "_lod1.bmesh";

		simplified_mesh->writeToFile(dest_path);

		const size_t src_size = FileUtils::getFileSize(src_path);
		const size_t new_size = FileUtils::getFileSize(dest_path);
		conPrint("src size: " + toString(src_size) + " B");
		conPrint("simplified size: " + toString(new_size) + " B");
		conPrint("Reduction ratio: " + toString((float)src_size / new_size));
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void buildLODVersions(const std::string& src_path)
{
	try
	{
		conPrint("===========================================");
		conPrint("Creating LOD models for " + src_path + "...");
		BatchedMeshRef batched_mesh = BatchedMesh::readFromFile(src_path);

		// Create lod1
		{
			BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/false);

			const std::string dest_path = removeDotAndExtension(src_path) + "_lod1.bmesh";

			simplified_mesh->writeToFile(dest_path);

			const size_t src_size = FileUtils::getFileSize(src_path);
			const size_t new_size = FileUtils::getFileSize(dest_path);
			conPrint("src size: " + toString(src_size) + " B");
			conPrint("simplified size: " + toString(new_size) + " B");
			conPrint("Reduction ratio: " + toString((float)src_size / new_size));
		}

		// Create lod2
		{
			BatchedMeshRef simplified_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, /*target_reduction_ratio=*/100.f, /*target_error=*/0.08f, /*sloppy=*/true);

			const std::string dest_path = removeDotAndExtension(src_path) + "_lod2.bmesh";

			simplified_mesh->writeToFile(dest_path);

			const size_t src_size = FileUtils::getFileSize(src_path);
			const size_t new_size = FileUtils::getFileSize(dest_path);
			conPrint("src size: " + toString(src_size) + " B");
			conPrint("simplified size: " + toString(new_size) + " B");
			conPrint("Reduction ratio: " + toString((float)src_size / new_size));
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#if 0
// Command line:
// C:\fuzz_corpus\mesh_simplification

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	Clock::init();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		BatchedMesh batched_mesh;
		BatchedMesh::readFromData(data, size, batched_mesh);

		batched_mesh.checkValidAndSanitiseMesh();

		// From generateLODModel() in LodGeneration.cpp:

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/false);

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/10.f, /*target_error=*/0.02f, /*sloppy=*/true);

		buildSimplifiedMesh(batched_mesh, /*target_reduction_ratio=*/100.f, /*target_error=*/0.08f, /*sloppy=*/true);
	}
	catch(glare::Exception& )
	{
	}
	return 0;  // Non-zero return values are reserved for future use.
}
#endif


void test()
{
	buildLODVersions("D:\\models\\dancedevil_glb_16934124793649044515.bmesh");

	/*try
	{
		Indigo::Mesh indigo_mesh;
		Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getTestReposDir()) + "/testscenes/zomb_head.igmesh", indigo_mesh);

		BatchedMesh batched_mesh;
		batched_mesh.buildFromIndigoMesh(indigo_mesh);

		batched_mesh.writeToFile("zomb_head.bmesh");

		BatchedMeshRef simplified_mesh = buildSimplifiedMesh(batched_mesh, 100.f, 0.1f, true);

		simplified_mesh->writeToFile("zomb_head_simplified.bmesh");

		conPrint("zomb_head_simplified.bmesh size: " + toString(FileUtils::getFileSize("zomb_head_simplified.bmesh")) + " B");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}*/

	
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\i_robot_obj_obj_14262372365043307937.bmesh");
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\banksyflower_gltf_6744734537774616040_bmesh_6744734537774616040.bmesh");
	//testOnBMesh("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources\\2020_07_19_20_24_11_obj_6467811551951867739_bmesh_6467811551951867739.bmesh");

	//TEMP: Made LOD versions of all meshes in cyberspace resources dir
	/*{
		const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPaths("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources", "bmesh");

		for(size_t i=0; i<paths.size(); ++i)
		{
			buildLODVersions(paths[i]);
		}
	}*/

	conPrint("Done");
}


} // end namespace MeshSimplification


#endif // BUILD_TESTS
