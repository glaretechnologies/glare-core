/*=====================================================================
formatdecoderobj.cpp
--------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "formatdecoderobj.h"


#include "modelformatdecoder.h" // For ModelFormatDecoderExcep
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../indigo/globals.h"
#include "../utils/namemap.h"
#include "../utils/StringUtils.h"
#include "../utils/timer.h"
#include "../utils/Parser.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"


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


void FormatDecoderObj::streamModel(const std::string& filename, Indigo::Mesh& handler, float scale)
{
	// Timer load_timer;

	bool encountered_uvs = false;

	NameMap<int> materials;

	int current_mat_index = -1;

	Indigo::Vector<Indigo::Vec2f> uv_vector(1);

	try
	{
		MemMappedFile file(filename);
	
		Parser parser((const char*)file.fileData(), (unsigned int)file.fileSize());

		const unsigned int MAX_NUM_FACE_VERTICES = 256;
		std::vector<unsigned int> face_vertex_indices(MAX_NUM_FACE_VERTICES);
		std::vector<unsigned int> face_normal_indices(MAX_NUM_FACE_VERTICES);
		std::vector<unsigned int> face_uv_indices(MAX_NUM_FACE_VERTICES, 0);

		unsigned int num_vertices_added = 0;

		std::vector<Indigo::Vec3f> vert_positions;
		std::vector<Indigo::Vec3f> vert_normals;

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
					//conPrint("\tFound reference to material '" + material_name + "'.");
					current_mat_index = materials.size();
					materials.insert(material_name, current_mat_index);
					handler.addMaterialUsed(toIndigoString(material_name));
				}
			}
			else if(token == "v")//vertex position
			{
				Indigo::Vec3f pos(0,0,0);
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
				Indigo::Vec2f texcoord(0,0);
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
					//handler.addUVSetExposition("default", 0);
					encountered_uvs = true;
				}

				assert(uv_vector.size() == 1);
				uv_vector[0] = texcoord;
				handler.addUVs(uv_vector);
			}
			else if(token == "vn") // vertex normal
			{
				Indigo::Vec3f normal(0,0,0);
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

				//if(numfaceverts >= MAX_NUM_FACE_VERTICES)
				//	conPrint("Warning, maximum number of verts per face reached or exceeded.");

				if(numfaceverts < 3)
					throw ModelFormatDecoderExcep("Invalid number of vertices in face: " + toString(numfaceverts) + " (line " + toString(linenum) + ")");

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
		// conPrint("\tOBJ parse took " + toString(load_timer.getSecondsElapsed()) + "s");
	}
	catch(Indigo::Exception& e)
	{
		throw ModelFormatDecoderExcep(e.what());
	}
}
