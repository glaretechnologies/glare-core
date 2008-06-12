/*=====================================================================
Lib3dsFormatDecoder.cpp
-----------------------
File created by ClassTemplate on Fri Jun 17 04:11:35 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "Lib3dsFormatDecoder.h"


#define LIB3DS_SUPPORT 1


#ifdef LIB3DS_SUPPORT
#include <lib3ds/file.h>
#include <lib3ds/io.h>
#include <lib3ds/mesh.h>
#include <lib3ds/material.h>
#endif
#include <assert.h>
#include "../indigo/globals.h"
#include "../simpleraytracer/ModelLoadingStreamHandler.h"
#include "../maths/vec2.h"
#include "../maths/vec3.h"

Lib3dsFormatDecoder::Lib3dsFormatDecoder()
{
}


Lib3dsFormatDecoder::~Lib3dsFormatDecoder()
{
}


#ifdef LIB3DS_SUPPORT

void Lib3dsFormatDecoder::streamModel(const std::string& filename, ModelLoadingStreamHandler& handler, float scale) // throw (ModelFormatDecoderExcep)
{
	Lib3dsFile* file = lib3ds_file_load(filename.c_str());
	if(!file)
		throw ModelFormatDecoderExcep("Failed to read 3ds file '" + filename + "'.");

	
	bool encountered_uvs = false;

	//------------------------------------------------------------------------
	//load the materials
	//------------------------------------------------------------------------
	std::map<std::string, size_t> mat_name_to_index;
	int i=0;
	Lib3dsMaterial* material = file->materials;
	if(!material)
	{
		//throw ModelFormatDecoderExcep("No materials in file.");
		
		//Make a dummy mat
		/*model_out.model_parts.push_back(CS::ModelPart());
		CS::Material& mat = model_out.model_parts.back().material;
		mat.setName("dummy");
		mat_to_part_map["dummy"] = 0;
		mat.diffuse_colour = Colour3(0.8f);
		mat.specular_colour = Colour3(0.0f);
		mat.opacity = 1.0f;
		mat.textured = false;*/
		//handler.addMaterial("dummy", Colour3(0.8f), 1.0f);
		mat_name_to_index["dummy"] = 0;
		handler.addMaterialUsed("dummy");
	}

	while(material)
	{
		const std::string srcmatname = material->name;

		//create a new model part
		/*model_out.model_parts.push_back(CS::ModelPart());
		CS::ModelPart& part = model_out.model_parts.back();

		CS::Material& mat = part.material;

		mat.setName(srcmatname);

		mat.diffuse_colour.set(
			material->diffuse[0],
			material->diffuse[1],
			material->diffuse[2]);

		mat.specular_colour.set(
			material->specular[0],
			material->specular[1],
			material->specular[2]);

		mat.opacity = 1.0f - material->transparency;

		mat.specular_amount = material->shininess;

		if(material->texture1_map.name[0] != 0)
		{
			mat.textured = true;
			mat.setTexFileName(material->texture1_map.name);
		}

		mat_to_part_map[srcmatname] = i++;*/
		//handler.addMaterial(srcmatname, Colour3(material->diffuse[0], material->diffuse[1], material->diffuse[2]), 1.0f - material->transparency);

		const unsigned int mat_index = mat_name_to_index.size();
		mat_name_to_index[srcmatname] = mat_index;
		handler.addMaterialUsed(srcmatname);
	
		material = material->next;
	}

	std::vector<Vec2f> texcoords(1);
	//std::vector<float> normals;//(mesh->faces * 9);
	unsigned int num_verts_added = 0;
	for(Lib3dsMesh* mesh = file->meshes; mesh; mesh = mesh->next)
	{
		//------------------------------------------------------------------------
		//calculate normals for mesh
		//------------------------------------------------------------------------
		//void lib3ds_mesh_calculate_normals(Lib3dsMesh *mesh, Lib3dsVector *normalL)
		//normals.resize(mesh->faces * 9);
		//if(mesh->faces > 0)//otherwise will crash when faces == 0
		//	lib3ds_mesh_calculate_normals(mesh, (Lib3dsVector*)&normals[0]);

		for(unsigned int f=0; f<mesh->faces; ++f)//for each tri
		{
			std::string facemat = mesh->faceL[f].material;//the material for this triangle
			if(facemat == "")
				facemat = "dummy";//assign to dummy material if not specified


			if(mesh->texels > 0)
				if(!encountered_uvs)
				{
					handler.setMaxNumTexcoordSets(1);
					handler.addUVSetExposition("albedo", 0);
					handler.addUVSetExposition("default", 0);
				}

			//find the part for this material
			std::map<std::string, size_t>::iterator result = mat_name_to_index.find(facemat);
			if(result != mat_name_to_index.end())//found the mat
			{
				//CS::ModelPart& part = model_out.model_parts[(*result).second];
				const size_t material_index = (*result).second;

				//------------------------------------------------------------------------
				//add verts
				//------------------------------------------------------------------------
				for(int v=0; v<3; ++v)
				{
					const unsigned int srcvertindex = mesh->faceL[f].points[v];

					const Vec3f pos(
							mesh->pointL[srcvertindex].pos[0],
							mesh->pointL[srcvertindex].pos[1],
							mesh->pointL[srcvertindex].pos[2]);

					if(mesh->texels > srcvertindex)
					{
						//texcoords.resize(1);
						assert(texcoords.size() == 1);

						texcoords[0].set(
							(mesh->texelL[srcvertindex])[0],
							(mesh->texelL[srcvertindex])[1]);

						handler.addUVs(texcoords);
					}

					/*const Vec3f normal(
						normals[f*9+v*3],
						normals[f*9+v*3 + 1],
						normals[f*9+v*3 + 2]);*/

					handler.addVertex(pos * scale/*, normal*/);
					
				}
				

				const unsigned int vert_indices[3] = {num_verts_added, num_verts_added+1, num_verts_added+2};
				handler.addTriangle(vert_indices, vert_indices, (int)material_index);

				num_verts_added += 3;
			}
			else
			{
				//error, material we have not seen
				//so just ignore the tri.
			//	assert(0);
				conPrint("Triangle with no associated material.");
				//TEMP NO ERROR throw ModelFormatDecoderExcep("Triangle with no associated material.");
			}	
		}
	}

	lib3ds_file_free(file);
	handler.endOfModel();
}


#else //LIB3DS_SUPPORT

void Lib3dsFormatDecoder::streamModel(/*const void* data, int datalen, */const std::string& filename, ModelLoadingStreamHandler& handler, float scale)// throw (ModelFormatDecoderExcep)
{
	::fatalError("Lib3dsFormatDecoder::streamModel: LIB3DS_SUPPORT disabled.");
}

#endif //LIB3DS_SUPPORT
