/*=====================================================================
formatdecoderobj.cpp
--------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "formatdecoderobj.h"


#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../indigo/globals.h"
#include "../utils/NameMap.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/Parser.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"
#include <unordered_map>


struct Vert
{
	unsigned int vert_i;
	unsigned int norm_i;

	inline bool operator < (const Vert& b) const
	{
		if(vert_i == b.vert_i)
			return norm_i < b.norm_i;
		else
			return vert_i < b.vert_i;
	}
	inline bool operator == (const Vert& b) const
	{
		return vert_i == b.vert_i && norm_i == b.norm_i;
	}
};


// Hash function for Vert
class VertHash
{
public:
	inline size_t operator()(const Vert& v) const
	{	
		// hash _Keyval to size_t value by pseudorandomizing transform.
		// Since there shouldn't be a lot of vertices with the same position but different normals, this should hopefully be adequate as a hash function.
		return (size_t)v.vert_i;
	}
};


// Skip whitespace, including a possible line continuation character (backslash)
static inline void skipWhitespace(Parser& parser)
{
	while(1)
	{
		if(parser.eof())
			return;
		else if(parser.getText()[parser.currentPos()] == ' ' || parser.getText()[parser.currentPos()] == '\t')
			parser.advance(); // consume whitespace char
		else if(parser.current() == '\\') // Line continuation character.  Consume next newline then continue.
		{
			parser.advance(); // consume backslash char.
			parser.advancePastLine();
		}
		else
		{
			// Current char is neither whitespace nor backslash.
			break;
		}
	}
}


void FormatDecoderObj::streamModel(const std::string& filename, Indigo::Mesh& handler, float scale)
{
	// Timer load_timer;

	bool encountered_uvs = false;

	NameMap<int> materials;

	int current_mat_index = -1;

	Indigo::Vector<Indigo::Vec2f> uv_vector(1);

	MemMappedFile file(filename);
	
	Parser parser((const char*)file.fileData(), (unsigned int)file.fileSize());

	const unsigned int MAX_NUM_FACE_VERTICES = 256;
	std::vector<unsigned int> face_uv_indices(MAX_NUM_FACE_VERTICES, 0);

	// Positions, normals and UVs as parsed from the OBJ file.
	std::vector<Indigo::Vec3f> vert_positions;
	std::vector<Indigo::Vec3f> vert_normals;
	std::vector<Indigo::Vec2f> uvs;

	std::unordered_map<Vert, unsigned int, VertHash> added_verts; // Hash map from Vertex to index of where the vertex was added to the Indigo Mesh.
	unsigned int num_verts_added = 0;
	std::vector<unsigned int> face_added_vert_indices(MAX_NUM_FACE_VERTICES);


	int linenum = 0;
	string_view token;
	while(!parser.eof())
	{
		linenum++;

		parser.parseSpacesAndTabs();
		
		if(parser.current() == '#')
		{
			parser.advancePastLine();
			continue;
		}

		parser.parseAlphaToken(token);

		if(token == "usemtl")  //material to use for subsequent faces
		{
			/// Parse material name ///
			skipWhitespace(parser);

			string_view material_name;
			parser.parseNonWSToken(material_name);

			/// See if material has already been created, create it if it hasn't been ///
			if(materials.isInserted(material_name.to_string()))
				current_mat_index = materials.getValue(material_name.to_string());
			else
			{
				//conPrint("\tFound reference to material '" + material_name + "'.");
				current_mat_index = materials.size();
				materials.insert(material_name.to_string(), current_mat_index);
				handler.addMaterialUsed(toIndigoString(material_name.to_string()));
			}
		}
		else if(token == "v") // vertex position
		{
			Indigo::Vec3f pos;
			skipWhitespace(parser);
			const bool r1 = parser.parseFloat(pos.x);
			skipWhitespace(parser);
			const bool r2 = parser.parseFloat(pos.y);
			skipWhitespace(parser);
			const bool r3 = parser.parseFloat(pos.z);

			if(!r1 || !r2 || !r3)
				throw Indigo::Exception("Parse error while reading position on line " + toString(linenum));

			pos *= scale;
			
			vert_positions.push_back(pos);
		}
		else if(token == "vt") // vertex tex coordinate
		{
			Indigo::Vec2f texcoord;
			skipWhitespace(parser);
			const bool r1 = parser.parseFloat(texcoord.x);
			skipWhitespace(parser);
			const bool r2 = parser.parseFloat(texcoord.y);

			if(!r1 || !r2)
				throw Indigo::Exception("Parse error while reading tex coord on line " + toString(linenum));


			// Assume one texcoord per vertex.
			if(!encountered_uvs)
			{
				handler.setMaxNumTexcoordSets(1);
				encountered_uvs = true;
			}

			assert(uv_vector.size() == 1);
			uv_vector[0] = texcoord;
			handler.addUVs(uv_vector);
			uvs.push_back(texcoord);
		}
		else if(token == "vn") // vertex normal
		{
			Indigo::Vec3f normal;
			skipWhitespace(parser);
			const bool r1 = parser.parseFloat(normal.x);
			skipWhitespace(parser);
			const bool r2 = parser.parseFloat(normal.y);
			skipWhitespace(parser);
			const bool r3 = parser.parseFloat(normal.z);

			if(!r1 || !r2 || !r3)
				throw Indigo::Exception("Parse error while reading normal on line " + toString(linenum));

			vert_normals.push_back(normal);
		}
		else if(token == "f") // face
		{
			int numfaceverts = 0;

			for(int i=0; i<(int)MAX_NUM_FACE_VERTICES; ++i)//for each vert in face polygon
			{
				skipWhitespace(parser);
				if(parser.eof() || parser.current() == '\n' || parser.current() == '\r')
					break; // end of line, we're done parsing this face.

				//------------------------------------------------------------------------
				//Parse vert, texcoord, normal indices
				//------------------------------------------------------------------------
				bool read_normal_index = false;
				int zero_based_vert_index;
				int zero_based_normal_index = 0;

				// Read vertex position index
				int vert_index;
				if(parser.parseInt(vert_index))
				{
					numfaceverts++;

					if(vert_index < 0)
						zero_based_vert_index = (unsigned int)vert_positions.size() + vert_index;
					else if(vert_index > 0)
						zero_based_vert_index = vert_index - 1; // Convert to 0-based index
					else
						throw Indigo::Exception("Position index invalid. (index '" + toString(vert_index) + "' out of bounds, on line " + toString(linenum) + ")");

					// Try and read vertex texcoord index
					if(parser.parseChar('/'))
					{
						int uv_index;
						if(parser.parseInt(uv_index))
						{
							if(uv_index < 0)
								face_uv_indices[i] = (unsigned int)uvs.size() + uv_index;
							else if(uv_index > 0)
								face_uv_indices[i] = uv_index - 1; // Convert to 0-based index
							else
								throw Indigo::Exception("Invalid tex coord index. (index '" + toString(uv_index) + "' out of bounds, on line " + toString(linenum) + ")");
						}

						// Try and read vertex normal index
						if(parser.parseChar('/'))
						{
							int normal_index;
							if(!parser.parseInt(normal_index))
								throw Indigo::Exception("syntax error: no integer following '/' (line " + toString(linenum) + ")");

							if(normal_index < 0)
								zero_based_normal_index = (unsigned int)vert_normals.size() + normal_index;
							else if(normal_index > 0)
								zero_based_normal_index = normal_index - 1; // Convert to 0-based index
							else
								throw Indigo::Exception("Invalid normal index. (index '" + toString(normal_index) + "' out of bounds, on line " + toString(linenum) + ")");
						
							read_normal_index = true;
						}
					}
				}
				else 
					throw Indigo::Exception("syntax error: no integer following 'f' (line " + toString(linenum) + ")");

				// Add the vertex to the mesh, if it hasn't been added already.

				if(zero_based_vert_index >= (int)vert_positions.size())
					throw Indigo::Exception("Position index invalid. (index '" + toString(zero_based_vert_index) + "' out of bounds, on line " + toString(linenum) + ")");

				if(read_normal_index)
				{
					if(zero_based_normal_index >= (int)vert_normals.size())
						throw Indigo::Exception("Normal index invalid. (index '" + toString(zero_based_normal_index) + "' out of bounds, on line " + toString(linenum) + ")");

					Vert v;
					v.vert_i = zero_based_vert_index;
					v.norm_i = zero_based_normal_index;
					const std::unordered_map<Vert, unsigned int, VertHash>::iterator res = added_verts.find(v);
					if(res == added_verts.end())
					{
						// Not added yet, add:
						handler.addVertex(vert_positions[zero_based_vert_index], vert_normals[zero_based_normal_index]);
						added_verts.insert(std::make_pair(v, num_verts_added));
						face_added_vert_indices[i] = num_verts_added;
						num_verts_added++;
					}
					else
						face_added_vert_indices[i] = res->second;
				}
				else
				{
					Vert v;
					v.vert_i = zero_based_vert_index;
					v.norm_i = 0;
					const std::unordered_map<Vert, unsigned int, VertHash>::iterator res = added_verts.find(v);
					if(res == added_verts.end())
					{
						// Not added yet, add:
						handler.addVertex(vert_positions[zero_based_vert_index]);
						added_verts.insert(std::make_pair(v, num_verts_added));
						face_added_vert_indices[i] = num_verts_added;
						num_verts_added++;
					}
					else
						face_added_vert_indices[i] = res->second;
				}

			}//end for each vertex

			//if(numfaceverts >= MAX_NUM_FACE_VERTICES)
			//	conPrint("Warning, maximum number of verts per face reached or exceeded.");

			if(numfaceverts < 3)
				throw Indigo::Exception("Invalid number of vertices in face: " + toString(numfaceverts) + " (line " + toString(linenum) + ")");

			//------------------------------------------------------------------------
			//Check current material index
			//------------------------------------------------------------------------
			if(current_mat_index < 0)
			{
				//conPrint("WARNING: found faces without a 'usemtl' line first.  Using material 'default'");
				current_mat_index = 0;
				materials.insert("default", current_mat_index);
				handler.addMaterialUsed("default");
			}

			if(numfaceverts == 3)
			{
				handler.addTriangle(&face_added_vert_indices[0], &face_uv_indices[0], current_mat_index);
			}
			else if(numfaceverts == 4)
			{
				handler.addQuad(&face_added_vert_indices[0], &face_uv_indices[0], current_mat_index);
			}
			else
			{
				// Add all tris needed to make up the face polygon
				for(int i=2; i<numfaceverts; ++i)
				{
					const unsigned int v_indices[3] = { face_added_vert_indices[0], face_added_vert_indices[i - 1], face_added_vert_indices[i] };
					const unsigned int tri_uv_indices[3] = { face_uv_indices[0], face_uv_indices[i-1], face_uv_indices[i] };
					handler.addTriangle(v_indices, tri_uv_indices, current_mat_index);
				}
			}

			// Finished handling face.
		}

		parser.advancePastLine();
	}

	handler.endOfModel();
	// conPrint("\tOBJ parse took " + toString(load_timer.getSecondsElapsed()) + "s");
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"


void FormatDecoderObj::test()
{
	conPrint("FormatDecoderObj::test()");

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/a_test_mesh.obj";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);

		testAssert(mesh.vert_positions.size() == 4);
		testAssert(mesh.uv_pairs.size() == 2);
		testAssert(mesh.num_uv_mappings == 1);
		testAssert(mesh.vert_normals.size() == 4);
		testAssert(mesh.triangles.size() == 0);
		testAssert(mesh.quads.size() == 2);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	// Load sphere.obj, also sphere_with_backslash.obj (which has some backslashes inserted) and make sure they give the same mesh.
	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/sphere.obj";
		Indigo::Mesh sphere_mesh_ref;
		streamModel(path, sphere_mesh_ref, 1.0);

		const std::string path2 = TestUtils::getIndigoTestReposDir() + "/testfiles/sphere_with_backslashes.obj";
		Indigo::Mesh sphere_mesh_2;
		streamModel(path2, sphere_mesh_2, 1.0);

		testAssert(sphere_mesh_ref.checksum() == sphere_mesh_2.checksum());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string path = TestUtils::getIndigoTestReposDir() + "/testfiles/teapot.obj";
		Indigo::Mesh mesh;
		streamModel(path, mesh, 1.0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
