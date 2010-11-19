/*=====================================================================
formatdecoderobj.cpp
--------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "formatdecoderobj.h"


#include "../utils/namemap.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"
#include "../utils/timer.h"
#include "../utils/Parser.h"

#include "../public/IndigoMesh.h"


using namespace Indigo;


FormatDecoderObj::FormatDecoderObj()
{	
}


FormatDecoderObj::~FormatDecoderObj()
{	
}


const std::string FormatDecoderObj::getExtensionType() const
{
	return "obj";
}


void FormatDecoderObj::streamModel(const std::string& filename, Indigo::IndigoMesh& handler, 
		float scale) // throws ModelFormatDecoderExcep
{
	Timer load_timer;

	bool encountered_uvs = false;

	NameMap<int> materials;

	int current_mat_index = -1;

	std::vector<IndigoVec2f> uv_vector(1);

	/// Read .obj file from disk into RAM ///
	std::vector<unsigned char> data;

	readEntireFile(filename, data); // throws ModelFormatDecoderExcep
	
	Parser parser((const char*)&(data[0]), (unsigned int)data.size());

	const unsigned int MAX_NUM_FACE_VERTICES = 256;
	std::vector<unsigned int> face_vertex_indices(MAX_NUM_FACE_VERTICES);
	std::vector<unsigned int> face_normal_indices(MAX_NUM_FACE_VERTICES);
	std::vector<unsigned int> face_uv_indices(MAX_NUM_FACE_VERTICES, 0);

	unsigned int num_vertices_added = 0;

	std::vector<IndigoVec3f> vert_positions;
	std::vector<IndigoVec3f> vert_normals;

	int linenum = 0;
	std::string token;
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
			parser.parseSpacesAndTabs();

			std::string material_name;
			parser.parseNonWSToken(material_name);

			/// See if material has already been created, create it if it hasn't been ///
			if(materials.isInserted(material_name))
				current_mat_index = materials.getValue(material_name);
			else
			{
				conPrint("\tFound reference to material '" + material_name + "'.");
				current_mat_index = materials.size();
				materials.insert(material_name, current_mat_index);
				handler.addMaterialUsed(material_name);
			}
		}
		else if(token == "v")//vertex position
		{
			IndigoVec3f pos(0,0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(pos.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(pos.y);
			parser.parseSpacesAndTabs();
			const bool r3 = parser.parseFloat(pos.z);

			if(!r1 || !r2 || !r3)
				throw ModelFormatDecoderExcep("Parse error while reading position on line " + toString(linenum));

			pos *= scale;
			
			//handler.addVertex(pos);//, Vec3f(0,0,1));
			vert_positions.push_back(pos);
		}
		else if(token == "vt")//vertex tex coordinate
		{
			IndigoVec2f texcoord(0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(texcoord.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(texcoord.y);

			if(!r1 || !r2)
				throw ModelFormatDecoderExcep("Parse error while reading tex coord on line " + toString(linenum));


			// Assume one texcoord per vertex.
			if(!encountered_uvs)
			{
				handler.setMaxNumTexcoordSets(1);
				handler.addUVSetExposition("default", 0);
				encountered_uvs = true;
			}

			assert(uv_vector.size() == 1);
			uv_vector[0] = texcoord;
			handler.addUVs(uv_vector);
		}
		else if(token == "vn") // vertex normal
		{
			IndigoVec3f normal(0,0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(normal.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(normal.y);
			parser.parseSpacesAndTabs();
			const bool r3 = parser.parseFloat(normal.z);

			if(!r1 || !r2 || !r3)
				throw ModelFormatDecoderExcep("Parse error while reading normal on line " + toString(linenum));

			vert_normals.push_back(normal);
		}
		else if(token == "f")//face
		{
			int numfaceverts = 0;

			for(int i=0; i<MAX_NUM_FACE_VERTICES; ++i)//for each vert in face polygon
			{
				parser.parseSpacesAndTabs();
				if(parser.eof() || parser.current() == '\n' || parser.current() == '\r')
					break; // end of line, we're done parsing this vertex.

				//------------------------------------------------------------------------
				//Parse vert, texcoord, normal indices
				//------------------------------------------------------------------------
				bool read_normal_index = false;

				// Read vertex position index
				if(parser.parseUnsignedInt(face_vertex_indices[i]))
				{
					numfaceverts++;

					// Bounds check
					if(face_vertex_indices[i] == 0)
						throw ModelFormatDecoderExcep("Position index invalid. (index '" + toString(face_vertex_indices[i]) + "' out of bounds, on line " + toString(linenum) + ")");

					face_vertex_indices[i]--; // make index 0-based

					// Try and read vertex texcoord index
					if(parser.parseChar('/'))
					{
						if(parser.parseUnsignedInt(face_uv_indices[i]))
						{
							if(face_uv_indices[i] == 0)
								throw ModelFormatDecoderExcep("Invalid tex coord index. (index '" + toString(face_uv_indices[i]) + "' out of bounds, on line " + toString(linenum) + ")");
							
							face_uv_indices[i]--; // make index 0-based
							//got_texcoord = true;
						}

						// Try and read vertex normal index
						if(parser.parseChar('/'))
						{
							//unsigned int vert_normal_index; // don't actually do anything with this.

							if(!parser.parseUnsignedInt(face_normal_indices[i]))
								throw ModelFormatDecoderExcep("syntax error: no integer following '/' (line " + toString(linenum) + ")");
						
							if(face_normal_indices[i] == 0)
								throw ModelFormatDecoderExcep("Invalid normal index. (index '" + toString(face_normal_indices[i]) + "' out of bounds, on line " + toString(linenum) + ")");
							
							face_normal_indices[i]--; // make index 0 based
							read_normal_index = true;
						}
					}
				}
				else 
					throw ModelFormatDecoderExcep("syntax error: no integer following 'f' (line " + toString(linenum) + ")");

				// Add the vertex to the mesh

				if(face_vertex_indices[i] >= vert_positions.size())
					throw ModelFormatDecoderExcep("Position index invalid. (index '" + toString(face_vertex_indices[i]) + "' out of bounds, on line " + toString(linenum) + ")");

				if(read_normal_index)
				{
					if(face_normal_indices[i] >= vert_normals.size())
						throw ModelFormatDecoderExcep("Normal index invalid. (index '" + toString(face_normal_indices[i]) + "' out of bounds, on line " + toString(linenum) + ")");

					handler.addVertex(vert_positions[face_vertex_indices[i]], vert_normals[face_normal_indices[i]]);
				}
				else
				{
					handler.addVertex(vert_positions[face_vertex_indices[i]]);
				}

			}//end for each vertex

			if(numfaceverts >= MAX_NUM_FACE_VERTICES)
				conPrint("Warning, maximum number of verts per face reached or exceeded.");

			if(numfaceverts < 3)
				throw ModelFormatDecoderExcep("Invalid number of vertices in face: " + toString(numfaceverts) + " (line " + toString(linenum) + ")");

			//------------------------------------------------------------------------
			//Check current material index
			//------------------------------------------------------------------------
			if(current_mat_index < 0)
			{
				conPrint("WARNING: found faces without a 'usemtl' line first.  Using material 'default'");
				current_mat_index = 0;
				materials.insert("default", current_mat_index);
				handler.addMaterialUsed("default");
			}

			if(numfaceverts == 3)
			{
				const unsigned int v_indices[3] = { num_vertices_added, num_vertices_added + 1, num_vertices_added + 2 };
				const unsigned int tri_uv_indices[3] = { face_uv_indices[0], face_uv_indices[1], face_uv_indices[2] };
				handler.addTriangle(v_indices, tri_uv_indices, current_mat_index);
			}
			else if(numfaceverts == 4)
			{
				const unsigned int v_indices[4] = { num_vertices_added, num_vertices_added + 1, num_vertices_added + 2, num_vertices_added + 3 };
				const unsigned int uv_indices[4] = { face_uv_indices[0], face_uv_indices[1], face_uv_indices[2], face_uv_indices[3] };
				handler.addQuad(v_indices, uv_indices, current_mat_index);
			}
			else
			{
				// Add all tris needed to make up the face polygon
				for(int i=2; i<numfaceverts; ++i)
				{
					//const unsigned int v_indices[3] = { face_vertex_indices[0], face_vertex_indices[i-1], face_vertex_indices[i] };
					const unsigned int v_indices[3] = { num_vertices_added, num_vertices_added + i - 1, num_vertices_added + i };
					const unsigned int tri_uv_indices[3] = { face_uv_indices[0], face_uv_indices[i-1], face_uv_indices[i] };
					handler.addTriangle(v_indices, tri_uv_indices, current_mat_index);
				}
			}

			num_vertices_added += numfaceverts;

			// Finished handling face.
		}

		parser.advancePastLine();
	}

	handler.endOfModel();
	conPrint("\tOBJ parse took " + toString(load_timer.getSecondsElapsed()) + "s");
}






#if 0
//------------------------------------------------------------------------
//Utility class and comparison operator
//------------------------------------------------------------------------
/*class FormatDecoderObjVertex
{
public:
	FormatDecoderObjVertex(){};
	~FormatDecoderObjVertex(){};
	Vec3f position;
	Vec3f normal;
	Vec2f texcoord;
};*/

/*class CompareFormatDecoderObjVertex
{
public:
	bool operator () (const FormatDecoderObjVertex& a, const FormatDecoderObjVertex& b) const
	{
		if(a.position < b.position)
			return true;
		else if(b.position < a.position)
			return false;
		else
		{	
			assert(a.position == b.position);
			if(a.normal < b.normal)
				return true;
			else if(b.normal < a.normal)
				return false;
			else
			{
				assert(a.normal == b.normal);
				return a.texcoord < b.texcoord;
			}
		}
	}
};*/


//TEMP HACK
//#define NON_COLLAPSED_VERTEX_LOAD 1

void FormatDecoderObj::streamModel(const std::string& filename, ModelLoadingStreamHandler& handler, 
		float scale)// throw (ModelFormatDecoderExcep)
{
	Timer load_timer;

	//Assume just one texcoord per vertex.
	handler.setMaxNumTexcoordSets(1);
	handler.addUVSetExposition("default", 0);

	NameMap<int> materials;

	int current_mat_index = -1;

	// OLD: put dummy values at start because .obj files use 1-based indexing.
	//std::vector<Vec3f> positions;//(1, Vec3(0,0,0));
	//std::vector<Vec3f> normals;//(1, Vec3(0,0,1));
	//std::vector<Vec2f> texcoords;//(1, Vec2(0,0));


	//Map from complete vertex structure to final index of vertex
	//std::map<FormatDecoderObjVertex, unsigned int, CompareFormatDecoderObjVertex> verts;
	//unsigned int verts_size = 0;

	std::vector<Vec2f> uv_vector(1);

	/// Read .obj file from disk into RAM ///
	std::vector<unsigned char> data;

	readEntireFile(filename, data);

	
	Parser parser((char*)&(data[0]), (unsigned int)data.size());

	const unsigned int MAX_NUM_FACE_VERTICES = 256;
	std::vector<unsigned int> face_vertex_indices(MAX_NUM_FACE_VERTICES);
	std::vector<unsigned int> face_uv_indices(MAX_NUM_FACE_VERTICES);

	//int num_bad_normals = 0;

	//unsigned int num_verts_created = 0;

	//Timer timer;
	int linenum = 0;
	std::string token;
	while(!parser.eof())
	{
		linenum++;

		/*if(linenum % 100000 == 0)
		{
			conPrint("linenum: " + toString(linenum));
			printVar(positions.size());
			printVar(normals.size());
			printVar(texcoords.size());
			//conPrint(::toString((double)linenum / timer.getSecondsElapsed()) + " lines / sec");
		}*/

		parser.parseSpacesAndTabs();
		
		if(parser.current() == '#')
		{
			parser.advancePastLine();
			continue;
		}

		parser.parseAlphaToken(token);

		//parser.parseWhiteSpace();
		if(token == "g") //group
		{			
		}
		else if(token == "mtllib") //material library file
		{
		}
		else if(token == "usemtl")  //material to use for subsequent faces
		{
			/// Parse material name ///
			parser.parseSpacesAndTabs();

			std::string material_name;
			parser.parseNonWSToken(material_name);

			/// See if material has already been created, create it if it hasn't been ///
			if(materials.isInserted(material_name))
				current_mat_index = materials.getValue(material_name);
			else
			{
				conPrint("\tFound reference to material '" + material_name + "'.");
				current_mat_index = materials.size();
				materials.insert(material_name, current_mat_index);
				handler.addMaterialUsed(material_name);
			}
		}
		else if(token == "vn")//normal
		{
			Vec3f normal(0,0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(normal.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(normal.y);
			parser.parseSpacesAndTabs();
			const bool r3 = parser.parseFloat(normal.z);

			if(!r1 || !r2 || !r3)
				throw ModelFormatDecoderExcep("Parse error while reading normal on line " + toString(linenum));

			/*if(!normal.isUnitLength())
			{	
				const int MAX_NUM_ERROR_MESSAGES = 100;
				if(num_bad_normals == MAX_NUM_ERROR_MESSAGES)
					conPrint("WARNING: Reached max num bad normal error messages for mesh.  Bad mesh!!!");

				
				if(normal == Vec3f(0.f, 0.f, 0.f))
				{
					if(num_bad_normals < MAX_NUM_ERROR_MESSAGES)
						conPrint("WARNING: normal was zero, setting to (0,0,1).");
					normal = Vec3f(0.f, 0.f, 1.0f);
				}
				else
				{
					if(num_bad_normals < MAX_NUM_ERROR_MESSAGES)
						conPrint("WARNING: normal does not have unit length; normalising.");
					normal.normalise();
				}

				num_bad_normals++;
			}*/
			//normals.push_back(normal);
		}
		else if(token == "v")//vertex position
		{
			Vec3f pos(0,0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(pos.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(pos.y);
			parser.parseSpacesAndTabs();
			const bool r3 = parser.parseFloat(pos.z);

			if(!r1 || !r2 || !r3)
				throw ModelFormatDecoderExcep("Parse error while reading position on line " + toString(linenum));

			//TEMP:
			/*const char* cur = parser.text + parser.currentpos;
			unsigned int d = strcspn(cur, "\n");
			for(unsigned int z=0; z<d; ++z)
				tmp[z] = cur[z];
			tmp[z] = '\0';*/
			/*char* current = parser.text + parser.currentpos;
			
			while(*current != '\n')
				current++;

			*current = '\0';

			sscanf(parser.text + parser.currentpos, " %f %f %f", &pos.x, &pos.y, &pos.z);


			parser.currentpos = current - parser.text;
			parser.currentpos++;*/


			pos *= scale;
			
			//positions.push_back(pos);

			handler.addVertex(pos, Vec3f(0,0,1));

//#ifdef NON_COLLAPSED_VERTEX_LOAD
//			handler.addVertex(pos, Vec3f(0,0,1), texcoord_vector);
//#endif
		}
		else if(token == "vt")//vertex tex coordinate
		{
			Vec2f texcoord(0,0);
			parser.parseSpacesAndTabs();
			const bool r1 = parser.parseFloat(texcoord.x);
			parser.parseSpacesAndTabs();
			const bool r2 = parser.parseFloat(texcoord.y);

			if(!r1 || !r2)
				throw ModelFormatDecoderExcep("Parse error while reading tex coord on line " + toString(linenum));

			//texcoords.push_back(texcoord);
			uv_vector[0] = texcoord;
			handler.addUVs(uv_vector);
		}
		else if(token == "f")//face
		{
			int numfaceverts = 0;

			for(int i=0; i<MAX_NUM_FACE_VERTICES; ++i)//for each vert in face polygon
			{
				parser.parseSpacesAndTabs();
				if(parser.eof() || parser.current() == '\n' || parser.current() == '\r')
					break;//end of line, we're done parsing this vertex.

				//------------------------------------------------------------------------
				//Parse vert, texcoord, normal indices
				//------------------------------------------------------------------------
				unsigned int vert_pos_index = 0;
				unsigned int vert_texcoord_index = 0;
				unsigned int vert_normal_index = 0;
				bool got_texcoord = false;
				bool got_normal = false;

				//Read vertex position index
				if(parser.parseUnsignedInt(vert_pos_index))
				{
					numfaceverts++;

					// Bounds check
					if(vert_pos_index == 0)//TEMP || vert_pos_index > positions.size())
						throw ModelFormatDecoderExcep("Position index invalid. (index '" + toString(vert_pos_index) + "' out of bounds, on line " + toString(linenum) + ")");

					vert_pos_index--;//make 0-based

					//Try and read vertex texcoord index
					if(parser.parseChar('/'))
					{
						/*if(!parser.parseInt(vert_texcoord_index))
							throw ModelFormatDecoderExcep("syntax error: no integer following '/' (line " + toString(linenum) + ")");
						
						if(vert_texcoord_index == 0 || vert_texcoord_index > texcoords.size())
							throw ModelFormatDecoderExcep("Invalid tex coord index. (index '" + toString(vert_texcoord_index) + "' out of bounds, on line " + toString(linenum) + ")");
						*/
						if(parser.parseUnsignedInt(vert_texcoord_index))
						{
							if(vert_texcoord_index == 0)// || vert_texcoord_index > texcoords.size())
								throw ModelFormatDecoderExcep("Invalid tex coord index. (index '" + toString(vert_texcoord_index) + "' out of bounds, on line " + toString(linenum) + ")");
							
							vert_texcoord_index--;//make 0-based
							got_texcoord = true;
						}

						//Try and read vertex normal index
						if(parser.parseChar('/'))
						{
							if(!parser.parseUnsignedInt(vert_normal_index))
								throw ModelFormatDecoderExcep("syntax error: no integer following '/' (line " + toString(linenum) + ")");
						
							if(vert_normal_index == 0)//TEMP || vert_normal_index > normals.size())
								throw ModelFormatDecoderExcep("Invalid normal index. (index '" + toString(vert_normal_index) + "' out of bounds, on line " + toString(linenum) + ")");
							
							vert_normal_index--;//make 0 based
							got_normal = true;
						}
					}
				}
				else 
					throw ModelFormatDecoderExcep("syntax error: no integer following 'f' (line " + toString(linenum) + ")");


//#ifdef NON_COLLAPSED_VERTEX_LOAD
				//TEMP: just create new vertex
				//vertex_indices[i] = vert_pos_index;//NOTE: should add 1 to make 1-based?
//#else
				
				//------------------------------------------------------------------------
				//Find or insert vertex
				//------------------------------------------------------------------------
				/*FormatDecoderObjVertex target_vertex;
				target_vertex.position = positions[vert_pos_index];
				target_vertex.normal = got_normal ? normals[vert_normal_index] : Vec3f(0,0,1);
				target_vertex.texcoord = got_texcoord ? texcoords[vert_texcoord_index] : Vec2f(0,0);
				
				std::map<FormatDecoderObjVertex, unsigned int, CompareFormatDecoderObjVertex>::const_iterator result = verts.find(target_vertex);
				if(result == verts.end())
				{
					//vert not found, so insert it
					//conPrint("Vert does not already exist, creating it...");
					vertex_indices[i] = verts_size;

					verts.insert(std::make_pair(target_vertex, verts_size));

					verts_size++;
					assert(verts_size == verts.size());

					//Add new vertex to mesh
					texcoord_vector[0] = target_vertex.texcoord;//texcoords[vert_texcoord_index];
					assert(texcoord_vector.size() == 1);
					handler.addVertex(target_vertex.position, target_vertex.normal, texcoord_vector);
				}
				else
				{
					//vert already created, so use it
					//conPrint("Vert already exists");
					vertex_indices[i] = (*result).second;
				}*/

				
				// Add a vertex for this polygon vertex.
				/*handler.addVertex(
					positions[vert_pos_index], 
					got_normal ? normals[vert_normal_index] : Vec3f(0,0,1)
					);*/



				face_vertex_indices[i] = vert_pos_index; // record the index of the vertex we created
				face_uv_indices[i] = vert_texcoord_index;

				//num_verts_created++;

//#endif				
			}//end for each vertex

			if(numfaceverts >= MAX_NUM_FACE_VERTICES)
				conPrint("Warning, maximum number of verts per face reached or exceeded.");

			if(numfaceverts < 3)
				throw ModelFormatDecoderExcep("Invalid number of vertices in face: " + toString(numfaceverts) + " (line " + toString(linenum) + ")");

			//------------------------------------------------------------------------
			//Check current material index
			//------------------------------------------------------------------------
			if(current_mat_index < 0)
			{
				//throw ModelFormatDecoderExcep("Face defined without specifying the material first. (Missing 'usemtl' line)");
				conPrint("WARNING: found faces without a 'usemtl' line first.  Using material 'default'");
				current_mat_index = 0;
				materials.insert("default", current_mat_index);
				handler.addMaterialUsed("default");
			}

			//add all tris needed to make up the face
			for(int i=2; i<numfaceverts; ++i)
			{
				//const unsigned int v_indices[3] = {vertex_indices[0], vertex_indices[i-1], vertex_indices[i]};
				//const unsigned int tri_uv_indices[3] = {uv_indices[0], uv_indices[i-1], uv_indices[i]};
				//handler.addTriangle(v_indices, tri_uv_indices, current_mat_index);

				const unsigned int v_indices[3] = {face_vertex_indices[0], face_vertex_indices[i-1], face_vertex_indices[i]};
				const unsigned int tri_uv_indices[3] = {face_uv_indices[0], face_uv_indices[i-1], face_uv_indices[i]};
				handler.addTriangle(v_indices, tri_uv_indices, current_mat_index);
			}

			//finished handling face.
		}

		parser.advancePastLine();
	}

	conPrint("\tOBJ parse took " + toString(load_timer.getSecondsElapsed()) + "s");
}








#endif





#if 0
void FormatDecoderObj::buildModel(const void* data, int datalen, const std::string& filename, 
		CS::Model& model_out) throw (ModelFormatDecoderExcep)
{
	std::string str((const char*)data, datalen);
	//std::istringstream stream(str);

	std::vector<Vec3> vertpositions;
	std::vector<Vec3> normals;	
	std::vector<Vec2> texcoords;
	int smoothinggroup = 0;	

	std::string mtllib;

	NameMap<int> parts;
	//CS::ModelPart* currentpart = NULL;
	int currentpart = 0;


	//Make a default mat
	{
	model_out.model_parts.push_back(CS::ModelPart());
	CS::Material& mat = model_out.model_parts.back().material;
	mat.setName("indigo_default");
	parts.insert("indigo_default", 0);
	mat.diffuse_colour = Colour3(0.8f);
	mat.specular_colour = Colour3(0.0f);
	mat.opacity = 1.0f;
	mat.textured = false;
	}

	std::string token;
	Parser parser(str);
	Timer timer;
	int linenum = 0;
	while(!parser.eof())
	{
		linenum++;

		if(linenum % 1000000 == 0)
		{

			conPrint("linenum: " + toString(linenum));
			conPrint(::toString((double)linenum / timer.getSecondsElapsed()) + " lines / sec");
		}

		

		parser.parseSpacesAndTabs();
		
		if(parser.current() == '#')
		{
			parser.advancePastLine();
			continue;
		}

		parser.parseAlphaToken(token);

		//parser.parseWhiteSpace();

		if(token == "g") 
		{			
			smoothinggroup++;
		}
		else if(token == "mtllib")
		{
			parser.parseSpacesAndTabs();
			parser.parseNonWSToken(mtllib);
		}
		else if(token == "usemtl")
		{
			//currentpart = NULL;

			/*std::string matname;
			ls >> matname;

			if(!parts.isInserted(matname))
			{
				parts.insert(matname, model_out.model_parts.size());
				currentpart = model_out.model_parts.size();

				//create new part with given mat name
				model_out.model_parts.push_back(CS::ModelPart());
				model_out.model_parts.back().autogen_normals = true;
				model_out.model_parts.back().material.setName(matname);			
			}
			else
			{
				currentpart = parts.getValue(matname);
			}*/
		}
		else if(token == "vn")//normal
		{
			/*Vec3 normal(0,0,0);
			ls >> normal.x;
			ls >> normal.y;
			ls >> normal.z;

			if(!::epsEqual(normal.length(), 1.0f, 0.001f))
				throw ModelFormatDecoderExcep("bad normal");

			if(!ls.good())
				throw ModelFormatDecoderExcep("error parsing normal.");
			
			normals.push_back(normal);
			*/
				
		}
		else if(token == "v")//vertex position
		{
			Vec3 pos(0,0,0);
			parser.parseSpacesAndTabs();
			parser.parseFloat(pos.x);
			parser.parseSpacesAndTabs();
			parser.parseFloat(pos.y);
			parser.parseSpacesAndTabs();
			parser.parseFloat(pos.z);
			
			/*ls >> pos.x;
			ls >> pos.y;
			ls >> pos.z;*/

			/*const float max_comp_val = 1e5;
			if(pos.x < -max_comp_val || pos.x > max_comp_val || pos.y < -max_comp_val || pos.y > max_comp_val ||
				pos.z < -max_comp_val || pos.z > max_comp_val)
				throw ModelFormatDecoderExcep("bad vertex");*/
				

			//if(ls.bad())//!ls.good())
			//	throw ModelFormatDecoderExcep("error parsing vertex pos.");
				
			vertpositions.push_back(pos);

			//TEMP
			model_out.model_parts[currentpart].getVerts().push_back(CS::Vertex(pos, Vec3(0,0,0), Vec2(0,0)));
		}
		else if(token == "vt")//vertex tex coordinate
		{
			/*Vec2 texcoord(0,0);
			ls >> texcoord.x;
			ls >> texcoord.y;

			if(!ls.good())
				throw ModelFormatDecoderExcep("error parsing tex coord.");

			texcoords.push_back(texcoord);*/
		}
		else if(token == "f")//face
		{
			if(currentpart < 0 || currentpart >= (int)model_out.model_parts.size())
			{
				assert(0);
				throw ModelFormatDecoderExcep("internal parser error");
			}
			const int totalnumverts = (int)model_out.getModelPart(currentpart).getVerts().size();


			int numtriverts = 0;
			int vertindices[4];

			for(int i=0; i<4; ++i)//for each vert
			{
				//ls >> vertindices[i];
				parser.parseSpacesAndTabs();
				if(parser.parseInt(vertindices[i]))
				{
					numtriverts++;
					vertindices[i]--;

					assert(vertindices[i] >= 0 && vertindices[i] < (int)model_out.model_parts[currentpart].getNumVerts());
				}
			}

			/*if(vertindex < 0 || vertindex >= (int)vertpositions.size())
				throw ModelFormatDecoderExcep("invalid vertex index");
			if(texcoordindex < 0 || texcoordindex >= (int)texcoords.size())
				throw ModelFormatDecoderExcep("invalid texcoord index");
			if(normalindex < 0 || normalindex >= (int)normals.size())
				throw ModelFormatDecoderExcep("invalid normal index");*/
				
			model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(vertindices[0], vertindices[1], vertindices[2]));
			if(numtriverts >= 4)
				model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(vertindices[0], vertindices[2], vertindices[3]));

			/*for(int i=0; i<3; ++i)//for each vert
			{
				int vertindex;
				ls >> vertindex;

				char slash = ls.get();
				if(slash != '/') throw ModelFormatDecoderExcep("parser error");

				int texcoordindex;
				ls >> texcoordindex;

				slash = ls.get();
				if(slash != '/') throw ModelFormatDecoderExcep("parser error");


				int normalindex;
				ls >> normalindex;

				if(!ls.good())
					throw ModelFormatDecoderExcep("error parsing vertex.");

	
				//convert to 0-based indices
				vertindex--;
				texcoordindex--;
				normalindex--;

				if(vertindex < 0 || vertindex >= (int)vertpositions.size())
					throw ModelFormatDecoderExcep("invalid vertex index");
				if(texcoordindex < 0 || texcoordindex >= (int)texcoords.size())
					throw ModelFormatDecoderExcep("invalid texcoord index");
				if(normalindex < 0 || normalindex >= (int)normals.size())
					throw ModelFormatDecoderExcep("invalid normal index");

				model_out.model_parts[currentpart].getVerts().push_back(CS::Vertex(vertpositions[vertindex],
									normals[normalindex], texcoords[texcoordindex]));
			}

			model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(numverts, numverts+1, numverts+2));
			*/
		//TEMP	model_out.model_parts[currentpart].getTris().back().smoothing_group = smoothinggroup++;
		}

		parser.advancePastLine();
	}

	//const std::string matfilepath = FileUtils::getDirectory(filename) + mtllib;
	//readMatFile(matfilepath, model_out, parts);






//------------------------------------------------------------------------
	////------------------------------------------------------------------------
	//
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------


	std::string line;
	std::string token;

	Timer timer;
	int linenum = 0;
	while(1)
	{
		linenum++;

		if(linenum % 10000 == 0)
		{

			conPrint("linenum: " + toString(linenum));
			conPrint(::toString((double)linenum / timer.getSecondsElapsed()) + " lines / sec");
		}

		

		std::getline(stream, line);

		if(!stream.good())
			break;

		std::istringstream ls(line);

		line = ::eatSuffix(line, "\r");

		ls >> token;

		if(!ls.good()) continue;

		if(token.empty()) continue;

		if(token[0] == '#') continue;//comment

		if(token == "g") 
		{			
			smoothinggroup++;
		}
		else if(token == "mtllib")
		{
			ls >> mtllib;
		}
		else if(token == "usemtl")
		{
			//currentpart = NULL;

			std::string matname;
			ls >> matname;

			if(!parts.isInserted(matname))
			{
				parts.insert(matname, model_out.model_parts.size());
				currentpart = model_out.model_parts.size();

				//create new part with given mat name
				model_out.model_parts.push_back(CS::ModelPart());
				model_out.model_parts.back().autogen_normals = true;
				model_out.model_parts.back().material.setName(matname);			
			}
			else
			{
				currentpart = parts.getValue(matname);
			}
		}
		else if(token == "vn")//normal
		{
			Vec3 normal(0,0,0);
			ls >> normal.x;
			ls >> normal.y;
			ls >> normal.z;

			if(!::epsEqual(normal.length(), 1.0f, 0.001f))
				throw ModelFormatDecoderExcep("bad normal");

			if(!ls.good())
				throw ModelFormatDecoderExcep("error parsing normal.");
			
			normals.push_back(normal);
			
				
		}
		else if(token == "v")//vertex position
		{
			Vec3 pos(0,0,0);
			ls >> pos.x;
			ls >> pos.y;
			ls >> pos.z;

			const float max_comp_val = 1e5;
			if(pos.x < -max_comp_val || pos.x > max_comp_val || pos.y < -max_comp_val || pos.y > max_comp_val ||
				pos.z < -max_comp_val || pos.z > max_comp_val)
				throw ModelFormatDecoderExcep("bad vertex");
				

			if(ls.bad())//!ls.good())
				throw ModelFormatDecoderExcep("error parsing vertex pos.");
				
			vertpositions.push_back(pos);

			//TEMP
			model_out.model_parts[currentpart].getVerts().push_back(CS::Vertex(pos, Vec3(0,0,0), Vec2(0,0)));
		}
		else if(token == "vt")//vertex tex coordinate
		{
			Vec2 texcoord(0,0);
			ls >> texcoord.x;
			ls >> texcoord.y;

			if(!ls.good())
				throw ModelFormatDecoderExcep("error parsing tex coord.");

			texcoords.push_back(texcoord);
		}
		else if(token == "f")//face
		{
			if(currentpart < 0 || currentpart >= (int)model_out.model_parts.size())
			{
				assert(0);
				throw ModelFormatDecoderExcep("internal parser error");
			}
			const int numverts = (int)model_out.getModelPart(currentpart).getVerts().size();

			int vertindices[4];

			for(int i=0; i<4; ++i)//for each vert
			{
				ls >> vertindices[i];
				vertindices[i]--;

				assert(vertindices[i] >= 0 && vertindices[i] < (int)model_out.model_parts[currentpart].getNumVerts());
			}

			/*if(vertindex < 0 || vertindex >= (int)vertpositions.size())
				throw ModelFormatDecoderExcep("invalid vertex index");
			if(texcoordindex < 0 || texcoordindex >= (int)texcoords.size())
				throw ModelFormatDecoderExcep("invalid texcoord index");
			if(normalindex < 0 || normalindex >= (int)normals.size())
				throw ModelFormatDecoderExcep("invalid normal index");*/
				
			model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(vertindices[0], vertindices[1], vertindices[2]));
			model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(vertindices[0], vertindices[2], vertindices[3]));

			/*for(int i=0; i<3; ++i)//for each vert
			{
				int vertindex;
				ls >> vertindex;

				char slash = ls.get();
				if(slash != '/') throw ModelFormatDecoderExcep("parser error");

				int texcoordindex;
				ls >> texcoordindex;

				slash = ls.get();
				if(slash != '/') throw ModelFormatDecoderExcep("parser error");


				int normalindex;
				ls >> normalindex;

				if(!ls.good())
					throw ModelFormatDecoderExcep("error parsing vertex.");

	
				//convert to 0-based indices
				vertindex--;
				texcoordindex--;
				normalindex--;

				if(vertindex < 0 || vertindex >= (int)vertpositions.size())
					throw ModelFormatDecoderExcep("invalid vertex index");
				if(texcoordindex < 0 || texcoordindex >= (int)texcoords.size())
					throw ModelFormatDecoderExcep("invalid texcoord index");
				if(normalindex < 0 || normalindex >= (int)normals.size())
					throw ModelFormatDecoderExcep("invalid normal index");

				model_out.model_parts[currentpart].getVerts().push_back(CS::Vertex(vertpositions[vertindex],
									normals[normalindex], texcoords[texcoordindex]));
			}

			model_out.model_parts[currentpart].getTris().push_back(CS::Triangle(numverts, numverts+1, numverts+2));
			*/
		//TEMP	model_out.model_parts[currentpart].getTris().back().smoothing_group = smoothinggroup++;
		}

		
	}

	//const std::string matfilepath = FileUtils::getDirectory(filename) + mtllib;
	//readMatFile(matfilepath, model_out, parts);


}

void FormatDecoderObj::readMatFile(const std::string& matfilename, CS::Model& model,
									NameMap<int>& namemap)
{
	std::ifstream file(matfilename.c_str());

	if(!file)
		throw ModelFormatDecoderExcep("failed to open '" + matfilename + "'.");

	std::string matname;
	Colour3 diffuse;
	Colour3 specular;
	float opacity;
	std::string texname;
	
	while(1)//read lines
	{
		std::string line;
		std::getline(file, line);

		if(!file.good())
			break;

		std::istringstream ls(line);

		line = ::eatSuffix(line, "\r");

		std::string token;
		ls >> token;

		if(!ls.good() || token.empty())
		{	
			//treat as end of material spec

			if(!namemap.isInserted(matname))
			{
				//ignore
			}
			else
			{
				//lookup mat we read in 
				int partindex = namemap.getValue(matname);
				assert(partindex >= 0 && partindex < (int)model.model_parts.size());

				CS::Material& mat = model.model_parts[partindex].material;
				mat.diffuse_colour = diffuse;
				mat.specular_colour = specular;
				mat.setTexFileName(texname);
				mat.textured = !texname.empty();	


				matname = "";
				diffuse.set(1,1,1);
				specular.set(1,1,1);
				texname = "";

			}
		}
		else if(token[0] == '#') //comment
		{
		}
		else if(token == "newmtl")
		{
			ls >> matname;
		}
		else if(token == "Ka")
		{
			/*Colour col;
			ls >> col.rgb.r;
			ls >> col.rgb.g;
			ls >> col.rgb.b;*/
		}
		else if(token == "Kd")
		{
			ls >> diffuse.r;
			ls >> diffuse.g;
			ls >> diffuse.b;
		}
		else if(token == "Ks")
		{
			ls >> specular.r;
			ls >> specular.g;
			ls >> specular.b;
		}
		else if(token == "map_Kd")
		{
			//diffuse map
			ls >> texname;
		}
		else if(token == "d")
		{
			ls >> opacity;
			opacity = 1.0f - opacity;
		}
	}


}

#endif



