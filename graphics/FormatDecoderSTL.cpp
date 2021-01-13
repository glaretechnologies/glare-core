/*=====================================================================
FormatDecoderSTL.cpp
--------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "FormatDecoderSTL.h"


#include "../dll/include/IndigoMesh.h"
#include "../utils/Exception.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Parser.h"
#include <assert.h>
#include <vector>


void FormatDecoderSTL::streamModel(const std::string& pathname, Indigo::Mesh& mesh, float scale)
{
	MemMappedFile file(pathname);
	const uint8* const data = (const uint8*)file.fileData();

	// Try and determine if this is a ASCII or binary STL file.  If it starts with the string 'solid', treat as ASCII.
	if(file.fileSize() < 5)
		throw Indigo::Exception("Invalid file.  (file size too small)");
	if(data[0] == 's' && data[1] == 'o' && data[2] == 'l' && data[3] == 'i' && data[4] == 'd')
	{
		// Treat as an ASCII file.

		Parser parser((const char*)data, (uint32)file.fileSize());

		if(!parser.parseCString("solid"))
			throw Indigo::Exception("Invalid file, expected 'solid'");
		parser.advancePastLine();
		parser.parseSpacesAndTabs();

		string_view token;
		if(!parser.parseAlphaToken(token))
			throw Indigo::Exception("Invalid file, expected 'facet' or 'endsolid'");

		while(token == "facet")
		{
			parser.parseSpacesAndTabs();
			if(!parser.parseCString("normal"))
				throw Indigo::Exception("Invalid file, expected 'normal'");
			parser.parseSpacesAndTabs();

			// Parse normal
			Indigo::Vec3f normal;
			parser.parseFloat(normal.x);
			parser.parseSpacesAndTabs();
			parser.parseFloat(normal.y);
			parser.parseSpacesAndTabs();
			parser.parseFloat(normal.z);

			parser.parseWhiteSpace();
			
			if(!parser.parseCString("outer"))
				throw Indigo::Exception("Invalid file, expected 'outer'");
			parser.parseSpacesAndTabs();
			if(!parser.parseCString("loop"))
				throw Indigo::Exception("Invalid file, expected 'loop'");
			parser.parseWhiteSpace();
			
			// Add triangle
			const uint32 num_verts = (uint32)mesh.vert_positions.size();
			Indigo::Triangle tri;
			tri.tri_mat_index = 0;
			for(int c=0; c<3; ++c)
				tri.uv_indices[c] = 0;
			for(int c=0; c<3; ++c)
				tri.vertex_indices[c] = num_verts + c;
			mesh.triangles.push_back(tri);

			for(int v=0; v<3; ++v)
			{
				if(!parser.parseCString("vertex"))
					throw Indigo::Exception("Invalid file, expected 'vertex'");

				parser.parseSpacesAndTabs();

				// Parse vertex
				Indigo::Vec3f vert;
				parser.parseFloat(vert.x);
				parser.parseSpacesAndTabs();
				parser.parseFloat(vert.y);
				parser.parseSpacesAndTabs();
				parser.parseFloat(vert.z);

				parser.parseWhiteSpace();

				// Add vertex position
				mesh.vert_positions.push_back(vert * scale);
			}

			if(!parser.parseCString("endloop"))
				throw Indigo::Exception("Invalid file, expected 'endloop'");
			parser.parseWhiteSpace();
			if(!parser.parseCString("endfacet"))
				throw Indigo::Exception("Invalid file, expected 'endfacet'");
			parser.parseWhiteSpace();

			parser.parseAlphaToken(token); // Parse next token, should be either 'facet' or 'endsolid'.
		}

		if(token != "endsolid")
			throw Indigo::Exception("Invalid file, expected 'endsolid'");
	}
	else
	{
		// Else treat as a binary file

		const size_t HEADER_SIZE = 80;

		if(file.fileSize() < HEADER_SIZE + sizeof(uint32))
			throw Indigo::Exception("Invalid file.  (file size too small)");

		uint32 num_faces;
		std::memcpy(&num_faces, data + HEADER_SIZE, sizeof(uint32));

		const size_t expected_min_file_size = HEADER_SIZE + sizeof(uint32) + num_faces * (sizeof(float)*12 + 2);
		if(file.fileSize() < expected_min_file_size)
			throw Indigo::Exception("Invalid file.  (file size too small)");

		//mesh.vert_normals.resize(num_faces * 3);
		mesh.vert_positions.resize(num_faces * 3);
		mesh.triangles.resize(num_faces);

		float cur_data[12];
		for(uint32 i=0; i<num_faces; ++i)
		{
			// Note that cur_data_uint8 may not be a multiple of 4, so can't just cast to float* and read directly.  memcpy instead.
			const uint8* const cur_data_uint8 = data + HEADER_SIZE + sizeof(uint32) + i * (sizeof(float)*12 + 2);
			
			std::memcpy(cur_data, cur_data_uint8, sizeof(float) * 12);
			
			//const Indigo::Vec3f normal(cur_data[0], cur_data[1], cur_data[2]);
			const Indigo::Vec3f v1(cur_data[3], cur_data[4], cur_data[5]);
			const Indigo::Vec3f v2(cur_data[6], cur_data[7], cur_data[8]);
			const Indigo::Vec3f v3(cur_data[9], cur_data[10], cur_data[11]);

			//mesh.vert_normals[i*3 + 0] = normal;
			//mesh.vert_normals[i*3 + 1] = normal;
			//mesh.vert_normals[i*3 + 2] = normal;

			mesh.vert_positions[i*3 + 0] = v1 * scale; // Note: could de-duplicate verts
			mesh.vert_positions[i*3 + 1] = v2 * scale;
			mesh.vert_positions[i*3 + 2] = v3 * scale;
			mesh.triangles[i].tri_mat_index = 0;
			for(int c=0; c<3; ++c)
				mesh.triangles[i].uv_indices[c] = 0;
			for(int c=0; c<3; ++c)
				mesh.triangles[i].vertex_indices[c] = i*3 + c;
		}
	}

	mesh.endOfModel();
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"


void FormatDecoderSTL::test()
{
	conPrint("FormatDecoderSTL::test()");

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/stl/cube.stl";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);

		testAssert(mesh.vert_positions.size() == 12*3);
		testAssert(mesh.uv_pairs.size() == 0);
		testAssert(mesh.num_uv_mappings == 0);
		testAssert(mesh.vert_normals.size() == 0);
		testAssert(mesh.triangles.size() == 12);
		testAssert(mesh.quads.size() == 0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/stl/cube_binary.stl";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);

		testAssert(mesh.vert_positions.size() == 12*3);
		testAssert(mesh.uv_pairs.size() == 0);
		testAssert(mesh.num_uv_mappings == 0);
		testAssert(mesh.vert_normals.size() == 0);
		testAssert(mesh.triangles.size() == 12);
		testAssert(mesh.quads.size() == 0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/stl/block100.stl";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);

		testAssert(mesh.vert_positions.size() == 12*3);
		testAssert(mesh.uv_pairs.size() == 0);
		testAssert(mesh.num_uv_mappings == 0);
		testAssert(mesh.vert_normals.size() == 0);
		testAssert(mesh.triangles.size() == 12);
		testAssert(mesh.quads.size() == 0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/stl/humanoid_tri.stl";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);

		testAssert(mesh.vert_positions.size() == 192*3);
		testAssert(mesh.uv_pairs.size() == 0);
		testAssert(mesh.num_uv_mappings == 0);
		testAssert(mesh.vert_normals.size() == 0);
		testAssert(mesh.triangles.size() == 192);
		testAssert(mesh.quads.size() == 0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	// Test handling of invalid files
	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/empty_file";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);
		failTest("Should have failed to read file.");
	}
	catch(Indigo::Exception&)
	{}

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);
		failTest("Should have failed to read file.");
	}
	catch(Indigo::Exception&)
	{}
}


#endif // BUILD_TESTS
