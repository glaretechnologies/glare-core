/*=====================================================================
formatdecoderobj.cpp
--------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "formatdecoderobj.h"


#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../indigo/globals.h"
#include "../utils/NameMap.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/FileUtils.h"
#include "../utils/Parser.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"
#include "../utils/HashMap.h"
#include "../utils/Hasher.h"
#include <unordered_map>


MTLTexMap::MTLTexMap()
:	origin(0.f),
	size(1.f)
{}


MTLMaterial::MTLMaterial()
:	Ka(0.f), // Set some reasonable defaults.
	Kd(0.8f), 
	Ks(0.33333f),
	Tf(1.0f),
	Ns_exponent(500.f),
	Ni_ior(1.5f),
	d_opacity(1.f)
{}


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
	inline bool operator != (const Vert& b) const
	{
		return vert_i != b.vert_i || norm_i != b.norm_i;
	}
};


// Hash function for Vert
class VertHash
{
public:
	inline size_t operator()(const Vert& v) const
	{	
		// Since there shouldn't be a lot of vertices with the same position but different normals, this should hopefully be adequate as a hash function.
		return hashBytes((const uint8*)&v, sizeof(Vert));
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


void FormatDecoderObj::streamModel(const std::string& filename, Indigo::Mesh& handler, float scale, bool parse_mtllib, MLTLibMaterials& mtllib_mats_out)
{
	MemMappedFile file(filename);

	loadModelFromBuffer((const uint8*)file.fileData(), file.fileSize(), filename, handler, scale, parse_mtllib, mtllib_mats_out);
}


static std::string sanitiseString(const std::string& s)
{
	std::string res = s;
	for(size_t i=0; i<s.size(); ++i)
		if(!(::isAlphaNumeric(s[i]) || s[i] == '-' || s[i] == '.'))
			res[i] = '_';
	return res;
}


void FormatDecoderObj::loadModelFromBuffer(const uint8* data, size_t len, const std::string& filename, Indigo::Mesh& handler, float scale, bool parse_mtllib, MLTLibMaterials& mtllib_mats_out) // Throws glare::Exception on failure.
{
	// NOTE: pretty crufty and dubious old code.
	// 
	// Timer load_timer;
	try
	{
		bool encountered_uvs = false;

		NameMap<int> materials;

		int current_mat_index = -1;

		Indigo::Vector<Indigo::Vec2f> uv_vector(1);

		Parser parser((const char*)data, len);

		const unsigned int MAX_NUM_FACE_VERTICES = 256;
		std::vector<unsigned int> face_uv_indices(MAX_NUM_FACE_VERTICES, 0);

		// Positions, normals and UVs as parsed from the OBJ file.
		std::vector<Indigo::Vec3f> vert_positions;
		std::vector<Indigo::Vec3f> vert_normals;
		std::vector<Indigo::Vec2f> uvs;

		//std::unordered_map<Vert, unsigned int, VertHash> added_verts; // Hash map from Vertex to index of where the vertex was added to the Indigo Mesh.
		Vert empty_key;
		empty_key.vert_i = std::numeric_limits<unsigned int>::max();
		empty_key.norm_i = std::numeric_limits<unsigned int>::max();

		HashMap<Vert, unsigned int, VertHash> added_verts(empty_key,
			45000
		);

		unsigned int num_verts_added = 0;
		std::vector<unsigned int> face_added_vert_indices(MAX_NUM_FACE_VERTICES);


		int linenum = 0;
		string_view token;
		while(parser.notEOF())
		{
			linenum++;

			parser.parseSpacesAndTabs();

			if(parser.currentIsChar('#')) // Skip comments
			{
				parser.advancePastLine();
				continue;
			}

			if(parser.notEOF() && isAlphabetic(parser.current()))
			{
				const bool parsed_token = parser.parseAlphaToken(token);
				if(!parsed_token)
					throw glare::Exception("Failed to parse token at line " + toString(linenum));

				if(token == "v") // vertex position
				{
					Indigo::Vec3f pos;
					skipWhitespace(parser);
					const bool r1 = parser.parseFloat(pos.x);
					skipWhitespace(parser);
					const bool r2 = parser.parseFloat(pos.y);
					skipWhitespace(parser);
					const bool r3 = parser.parseFloat(pos.z);

					if(!r1 || !r2 || !r3)
						throw glare::Exception("Parse error while reading position on line " + toString(linenum));

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
						throw glare::Exception("Parse error while reading tex coord on line " + toString(linenum));


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
						throw glare::Exception("Parse error while reading normal on line " + toString(linenum));

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
								throw glare::Exception("Position index invalid. (index '" + toString(vert_index) + "' out of bounds, on line " + toString(linenum) + ")");

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
										throw glare::Exception("Invalid tex coord index. (index '" + toString(uv_index) + "' out of bounds, on line " + toString(linenum) + ")");
								}

								// Try and read vertex normal index
								if(parser.parseChar('/'))
								{
									int normal_index;
									if(!parser.parseInt(normal_index))
										throw glare::Exception("syntax error: no integer following '/' (line " + toString(linenum) + ")");

									if(normal_index < 0)
										zero_based_normal_index = (unsigned int)vert_normals.size() + normal_index;
									else if(normal_index > 0)
										zero_based_normal_index = normal_index - 1; // Convert to 0-based index
									else
										throw glare::Exception("Invalid normal index. (index '" + toString(normal_index) + "' out of bounds, on line " + toString(linenum) + ")");
						
									read_normal_index = true;
								}
							}
						}
						else 
							throw glare::Exception("syntax error: no integer following 'f' (line " + toString(linenum) + ")");

						// Add the vertex to the mesh, if it hasn't been added already.

						if((zero_based_vert_index < 0) || (zero_based_vert_index >= (int)vert_positions.size()))
							throw glare::Exception("Position index invalid. (index '" + toString(zero_based_vert_index) + "' out of bounds, on line " + toString(linenum) + ")");

						if(read_normal_index)
						{
							if((zero_based_normal_index < 0) || (zero_based_normal_index >= (int)vert_normals.size()))
								throw glare::Exception("Normal index invalid. (index '" + toString(zero_based_normal_index) + "' out of bounds, on line " + toString(linenum) + ")");

							Vert v;
							v.vert_i = zero_based_vert_index;
							v.norm_i = zero_based_normal_index;

							const auto insert_res = added_verts.insert(std::make_pair(v, num_verts_added)); // Try and add to map
							if(insert_res.second)
							{
								// Vert was not in map, but is added now.
								handler.addVertex(vert_positions[zero_based_vert_index], vert_normals[zero_based_normal_index]);
								face_added_vert_indices[i] = num_verts_added;
								num_verts_added++;
							}
							else
							{
								// Vert was in map already, and insert_res.first is an iterator referring to the existing item.
								face_added_vert_indices[i] = insert_res.first->second;
							}
					
						}
						else
						{
							Vert v;
							v.vert_i = zero_based_vert_index;
							v.norm_i = 0;
							const auto res = added_verts.find(v);
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
						throw glare::Exception("Invalid number of vertices in face: " + toString(numfaceverts) + " (line " + toString(linenum) + ")");

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
				else if(token == "mtllib")
				{
					skipWhitespace(parser);

					string_view mtllib_path;
					parser.parseNonWSToken(mtllib_path);

					if(parse_mtllib)
					{
						// NOTE: what's the best way to handle this?  Should we allow a "./" prefix?
						const std::string safe_mtl_path = sanitiseString(toString(mtllib_path));

						// If .mtl file does not exist, just skip trying to parse it instead of throwing an exception.
						const std::string mtl_fullpath = FileUtils::join(FileUtils::getDirectory(filename), safe_mtl_path);
						if(FileUtils::fileExists(mtl_fullpath))
							parseMTLLib(mtl_fullpath, mtllib_mats_out);
					}
				}
				else if(token == "usemtl")  //material to use for subsequent faces
				{
					/// Parse material name ///
					skipWhitespace(parser);

					string_view material_name;
					parser.parseNonWSToken(material_name);

					/// See if material has already been created, create it if it hasn't been ///
					if(materials.isInserted(toString(material_name)))
						current_mat_index = materials.getValue(toString(material_name));
					else
					{
						//conPrint("\tFound reference to material '" + material_name + "'.");
						current_mat_index = (int)materials.size();
						materials.insert(toString(material_name), current_mat_index);
						handler.addMaterialUsed(toIndigoString(toString(material_name)));
					}
				}
			}

			parser.advancePastLine();
		}

		handler.endOfModel();
		// conPrint("\tOBJ parse took " + toString(load_timer.getSecondsElapsed()) + "s");
	}
	catch(Indigo::IndigoException& e)
	{
		throw glare::Exception(toStdString(e.what()));
	}
}


static Colour3f parseCol3(Parser& parser, int& linenum)
{
	Colour3f col;
	skipWhitespace(parser);
	if(!parser.parseFloat(col.r))
		throw glare::Exception("Parse error while reading colour on line " + toString(linenum));
	skipWhitespace(parser);
	if(!parser.parseFloat(col.g))
		throw glare::Exception("Parse error while reading colouur on line " + toString(linenum));
	skipWhitespace(parser);
	if(!parser.parseFloat(col.b))
		throw glare::Exception("Parse error while reading colour on line " + toString(linenum));
	return col;
}


void FormatDecoderObj::parseMTLLib(const std::string& filename, MLTLibMaterials& mtllib_mats_out)
{
	MemMappedFile file(filename);

	parseMTLLibFromBuffer((const uint8*)file.fileData(), file.fileSize(), filename, mtllib_mats_out);
}


void FormatDecoderObj::parseMTLLibFromBuffer(const uint8* data, size_t len, const std::string& filename, MLTLibMaterials& mtllib_mats_out)
{
	mtllib_mats_out.mtl_file_path = filename;
	mtllib_mats_out.materials.resize(0);

	Parser parser((const char*)data, len);

	int linenum = 0;
	string_view token;
	while(!parser.eof())
	{
		linenum++;

		parser.parseSpacesAndTabs();

		if(parser.eof())
			break;
		
		if(parser.current() == '#') // Skip comments
		{
			parser.advancePastLine();
			continue;
		}

		if(parser.parseIdentifier(token))
		{
			// conPrint(token.to_string());

			if(token == "newmtl") // new material definition
			{
				skipWhitespace(parser);

				string_view material_name;
				parser.parseNonWSToken(material_name);

				mtllib_mats_out.materials.push_back(MTLMaterial());
				mtllib_mats_out.materials.back().name = toString(material_name);
			}
			else if(token == "Ka")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				mtllib_mats_out.materials.back().Ka = parseCol3(parser, linenum);
			}
			else if(token == "Kd")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				mtllib_mats_out.materials.back().Kd = parseCol3(parser, linenum);
			}
			else if(token == "Ks")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				mtllib_mats_out.materials.back().Ks = parseCol3(parser, linenum);
			}
			else if(token == "Tf")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				mtllib_mats_out.materials.back().Tf = parseCol3(parser, linenum);
			}
			else if(token == "Ns")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				skipWhitespace(parser);
				if(!parser.parseFloat(mtllib_mats_out.materials.back().Ns_exponent))
					throw glare::Exception("Parse error while reading Ns on line " + toString(linenum));
			}
			else if(token == "Ni")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				skipWhitespace(parser);
				if(!parser.parseFloat(mtllib_mats_out.materials.back().Ni_ior))
					throw glare::Exception("Parse error while reading Ni on line " + toString(linenum));
			}
			else if(token == "d")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				skipWhitespace(parser);
				if(!parser.parseFloat(mtllib_mats_out.materials.back().d_opacity))
					throw glare::Exception("Parse error while reading d on line " + toString(linenum));
			}
			else if(token == "map_Kd")
			{
				if(mtllib_mats_out.materials.empty())
					throw glare::Exception("No material specified yet. (line " + toString(linenum) + ")");

				skipWhitespace(parser);

				std::string texpath;
				while(!parser.eof() && !isWhitespace(parser.current()))
				{
					texpath += parser.current();
					parser.advance();
				}

				mtllib_mats_out.materials.back().map_Kd.path = texpath;
			}
		}

		parser.advancePastLine();
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"


#if 0
// Command line:
// C:\fuzz_corpus\obj -max_len=1000000 -seed=1
// or
// C:\fuzz_corpus\obj -max_len=1000000 -jobs=16

#if 1
// Fuzz obj loading:
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		Indigo::Mesh mesh;
		MLTLibMaterials mtllib_mats;
		FormatDecoderObj::loadModelFromBuffer(data, size, "dummy_filename", mesh, 1.f, /*parse mtllib=*/false, mtllib_mats);
	}
	catch(glare::Exception&)
	{}
	return 0;  // Non-zero return values are reserved for future use.
}

#else

// Fuzz MTL loading:
// C:\fuzz_corpus\mtl -max_len=1000000 -seed=1
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		MLTLibMaterials mtllib_mats;
		FormatDecoderObj::parseMTLLibFromBuffer(data, size, "dummy_filename", mtllib_mats);
	}
	catch(glare::Exception&)
	{}
	return 0;  // Non-zero return values are reserved for future use.
}

#endif

#endif


void FormatDecoderObj::test()
{
	conPrint("FormatDecoderObj::test()");


	// vert index out of bounds
	try
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/obj/neg pos indices.obj";
		Indigo::Mesh mesh;
		MLTLibMaterials mats;
		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);

		failTest("Expected excep");
	}
	catch(glare::Exception&)
	{}

	// normal index out of bounds
	try
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/obj/neg normal indices.obj";
		Indigo::Mesh mesh;
		MLTLibMaterials mats;
		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);

		failTest("Expected excep");
	}
	catch(glare::Exception&)
	{}

	// uv index out of bounds
	try
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/obj/neg uv indices.obj";
		Indigo::Mesh mesh;
		MLTLibMaterials mats;
		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);

		failTest("Expected excep");
	}
	catch(glare::Exception&)
	{}


	if(false)
	{
		MLTLibMaterials mats;

		//{
		//	// Perf test

		//	const int N = 100;

		//	const std::string path = TestUtils::getTestReposDir() + "/testscenes/infinite_head/Infinite-Level_02.OBJ";

		//	Timer timer;
		//	for(int i=0; i<N; ++i)
		//	{
		//		Indigo::Mesh mesh;
		//		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);
		//	}
		//	const double elapsed = timer.elapsed();
		//	const double time_each = elapsed / N;
		//	conPrint("infinte head av elapsed: " + doubleToStringNSigFigs(time_each, 5) + " s");
		//}
		{
			// Perf test

			const int N = 100;

			const std::string path = TestUtils::getTestReposDir() + "/testscenes/elephant.obj";

			Timer timer;
			for(int i=0; i<N; ++i)
			{
				Indigo::Mesh mesh;
				streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);
			}
			const double elapsed = timer.elapsed();
			const double time_each = elapsed / N;
			conPrint("elephant av elapsed: " + doubleToStringNSigFigs(time_each, 5) + " s");
		}
	}
	//return;

	try
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/a_test_mesh.obj";
		Indigo::Mesh mesh;
		MLTLibMaterials mats;
		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);

		testAssert(mesh.vert_positions.size() == 4);
		testAssert(mesh.uv_pairs.size() == 2);
		testAssert(mesh.num_uv_mappings == 1);
		testAssert(mesh.vert_normals.size() == 4);
		testAssert(mesh.triangles.size() == 0);
		testAssert(mesh.quads.size() == 2);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Load sphere.obj, also sphere_with_backslash.obj (which has some backslashes inserted) and make sure they give the same mesh.
	try
	{
		MLTLibMaterials mats;
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/sphere.obj";
		Indigo::Mesh sphere_mesh_ref;
		streamModel(path, sphere_mesh_ref, 1.0, /*parse mtllib=*/false, mats);

		const std::string path2 = TestUtils::getTestReposDir() + "/testfiles/sphere_with_backslashes.obj";
		Indigo::Mesh sphere_mesh_2;
		streamModel(path2, sphere_mesh_2, 1.0, /*parse mtllib=*/false, mats);

		testAssert(sphere_mesh_ref.checksum() == sphere_mesh_2.checksum());
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/teapot.obj";
		Indigo::Mesh mesh;
		MLTLibMaterials mats;
		streamModel(path, mesh, 1.0, /*parse mtllib=*/false, mats);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//=================================== Test parsing of .mtl files =====================================
	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/obj/eames_elephant.mtl", mats);
		
		testAssert(mats.materials.size() == 2);

		testAssert(mats.materials[0].name == "Plastic_red");
		testAssert(mats.materials[0].Kd == Colour3f(0.874510f, 0.000000f, 0.000000f));
		testAssert(mats.materials[0].Ks == Colour3f(0.330000f, 0.330000f, 0.330000f));
		testAssert(mats.materials[0].Tf == Colour3f(1,1,1));
		testAssert(mats.materials[0].d_opacity == 1.f);
		testAssert(mats.materials[0].Ni_ior == 1.5f);

		testAssert(mats.materials[1].name == "chrome_shiny");
		testAssert(mats.materials[1].Kd == Colour3f(0.109804f, 0.109804f, 0.109804f));
		testAssert(mats.materials[1].Ks == Colour3f(0.330000f, 0.330000f, 0.330000f));
		testAssert(mats.materials[1].Tf == Colour3f(1,1,1));
		testAssert(mats.materials[1].d_opacity == 1.f);
		testAssert(mats.materials[1].Ni_ior == 1.5f);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/obj/elephant.mtl", mats);
		
		testAssert(mats.materials.size() == 4);

		testAssert(mats.materials[0].name == "ElphSkin");
		testAssert(mats.materials[0].Kd == Colour3f(0.0000f, 0.0000f, 0.0000f));
		testAssert(mats.materials[0].Ks == Colour3f(0.3700f, 0.3700, 0.3700f));
		testAssert(mats.materials[0].Tf == Colour3f(1,1,1));
		testAssert(mats.materials[0].Ns_exponent == 48.0000f);
		testAssert(mats.materials[0].Ni_ior == 1.5f);
		testAssert(mats.materials[0].d_opacity == 1.f);
		testAssert(mats.materials[0].map_Kd.path == "maps\\ElphSkin.jpg");

		testAssert(mats.materials[1].name == "Ele_Tuskh");
		testAssert(mats.materials[1].Kd == Colour3f(0.9608f, 0.9294f, 0.8431f));
		testAssert(mats.materials[1].Ks == Colour3f(0.3900f, 0.3900f, 0.3900f));
		testAssert(mats.materials[1].Tf == Colour3f(1,1,1));
		testAssert(mats.materials[1].Ns_exponent == 40.0000f);
		testAssert(mats.materials[1].Ni_ior == 1.5f);
		testAssert(mats.materials[1].d_opacity == 1.f);
		testAssert(mats.materials[1].map_Kd.path == "maps\\EleTusk.jpg");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	
	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/obj/EU22y.mtl", mats);
		
		testAssert(mats.materials.size() == 3);
		testAssert(mats.materials[0].name == "Bark");
		testAssert(mats.materials[1].name == "Branch");
		testAssert(mats.materials[2].name == "Needle");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	
	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/obj/Infinite-Level_02.mtl", mats);
		
		testAssert(mats.materials.size() == 1);
		testAssert(mats.materials[0].name == "defaultMat");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test invalid path
	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/obj/NOT_A_FILE", mats);
		failTest("Shouldn't get here.");
	}
	catch(glare::Exception&)
	{
	}

	// Test empty file
	try
	{
		MLTLibMaterials mats;
		parseMTLLib(TestUtils::getTestReposDir() + "/testfiles/empty_file", mats);
		testAssert(mats.materials.size() == 0);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

}


#endif // BUILD_TESTS
