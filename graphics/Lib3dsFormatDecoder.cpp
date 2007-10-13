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

class MyBuf
{
public:
	const void* data;
	int datalen;
	int pos;
	bool error;
};



Lib3dsBool
my_fileio_error_func(void *self)
{
	//FILE *f = (FILE*)self;
	//return(ferror(f)!=0);
	MyBuf* buf = (MyBuf*)self;
	return buf->error;
}


long
my_fileio_seek_func(void *self, long offset, Lib3dsIoSeek origin)
{
	MyBuf* buf = (MyBuf*)self;
	switch(origin)
	{
	case LIB3DS_SEEK_SET:
		buf->pos = offset;
		break;
    case LIB3DS_SEEK_CUR:
		buf->pos += offset;
		break;
    case LIB3DS_SEEK_END:
		buf->pos = buf->datalen + offset;
		break;
    default:
		assert(0);
		return(0);
	}

	return 0;
}

  /*FILE *f = (FILE*)self;
  int o;
  switch (origin) {
    case LIB3DS_SEEK_SET:
      o = SEEK_SET;
      break;
    case LIB3DS_SEEK_CUR:
      o = SEEK_CUR;
      break;
    case LIB3DS_SEEK_END:
      o = SEEK_END;
      break;
    default:
      ASSERT(0);
      return(0);
  }
  return (fseek(f, offset, o));*/


long
my_fileio_tell_func(void *self)
{
	MyBuf* buf = (MyBuf*)self;
	return buf->pos;

  //FILE *f = (FILE*)self;
  //return(ftell(f));
}


static int
my_fileio_read_func(void *self, Lib3dsByte *buffer, int size)
{
	MyBuf* buf = (MyBuf*)self;

	if(buf->pos + size > buf->datalen)
	{
		buf->error = true;
		return 0;
	}


	//TODO: error checking
	memcpy(buffer, (const unsigned char*)buf->data + buf->pos, size);
	buf->pos += size;

	//FILE *f = (FILE*)self;
	//return(fread(buffer, 1, size, f));

	return size; 
}


int
my_fileio_write_func(void *self, const Lib3dsByte *buffer, int size)
{
	MyBuf* buf = (MyBuf*)self;

	//don't do writing.
	//memcpy(buf->data, buffer, size);


  //FILE *f = (FILE*)self;
  //return(fwrite(buffer, 1, size, f));

	return 0;
}

#endif

#if 0
void Lib3dsFormatDecoder::buildModel(const void* data, int datalen, const std::string& filename, 
		CS::Model& model_out) throw (ModelFormatDecoderExcep)
{

	MyBuf buf;
	buf.data = data;
	buf.datalen = datalen;
	buf.pos = 0;
	buf.error = false;

	Lib3dsIo *io = lib3ds_io_new(
		&buf, 
		my_fileio_error_func,
		my_fileio_seek_func,
		my_fileio_tell_func,
		my_fileio_read_func,
		my_fileio_write_func
	);

	if(!io) 
	{
		throw ModelFormatDecoderExcep("Failed to create a Lib3dsIo struct.");
	}

	Lib3dsFile* file = lib3ds_file_new();
	if(!file) 
		throw ModelFormatDecoderExcep("Failed to create a Lib3dsFile struct.");

	if(!lib3ds_file_read(file, io))
	{
		free(file);
		
		throw ModelFormatDecoderExcep("3ds read failed.");
	}

	assert(file);


	//------------------------------------------------------------------------
	//build the CS model from the Lib3dsFile structure.
	//------------------------------------------------------------------------

	//a map from material names to object parts.
	std::map<std::string, int> mat_to_part_map;

	//------------------------------------------------------------------------
	//load the materials
	//------------------------------------------------------------------------
	int i=0;
	Lib3dsMaterial* material = file->materials;
	if(!material)
	{
		//throw ModelFormatDecoderExcep("No materials in file.");
		
		//Make a dummy mat
		model_out.model_parts.push_back(CS::ModelPart());
		CS::Material& mat = model_out.model_parts.back().material;
		mat.setName("dummy");
		mat_to_part_map["dummy"] = 0;
		mat.diffuse_colour = Colour3(0.8f);
		mat.specular_colour = Colour3(0.0f);
		mat.opacity = 1.0f;
		mat.textured = false;
	}

	while(material)
	{
		const std::string srcmatname = material->name;

		//create a new model part
		model_out.model_parts.push_back(CS::ModelPart());
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

		mat_to_part_map[srcmatname] = i++;
	
		material = material->next;
	}

	Lib3dsMesh* mesh = file->meshes;
	while(mesh)
	{
		//------------------------------------------------------------------------
		//calculate normals for mesh
		//------------------------------------------------------------------------
		//void lib3ds_mesh_calculate_normals(Lib3dsMesh *mesh, Lib3dsVector *normalL)
		std::vector<float> normals(mesh->faces * 9);
		lib3ds_mesh_calculate_normals(mesh, (Lib3dsVector*)&normals[0]);


		for(unsigned int f=0; f<mesh->faces; ++f)
		{
			std::string facemat = mesh->faceL[f].material;//the material for this triangle
			if(facemat == "")
				facemat = "dummy";//assign to dummy material if not specified"

			//find the part for this material
			std::map<std::string, int>::iterator result = mat_to_part_map.find(facemat);
			if(result != mat_to_part_map.end())
			{
				CS::ModelPart& part = model_out.model_parts[(*result).second];

				//add the tri to the parts
				part.getTris().resize(part.getTris().size() + 1);

				
				//set the tri smoothing group
///				part.getTris().back().smoothing_group = mesh->faceL[f].smoothing;

				//copy over the normals
//NEW: NO NORMAL COPYING				
//				for(int i=0; i<3; ++i)
//					part.getTris().back().vertnormals[i].set(normals[f*9+i*3], normals[f*9+i*3 + 1], normals[f*9+i*3 + 2]);


				//TODO: collapse similar vertices

				const int v0index = (int)part.getVerts().size();

				//TEMP: add required vertices to part
				for(int v=0; v<3; ++v)
				{
					const unsigned int srcvertindex = mesh->faceL[f].points[v];

					part.getVerts().push_back(CS::Vertex());

					part.getVerts().back().pos.set(
							mesh->pointL[srcvertindex].pos[0],
							mesh->pointL[srcvertindex].pos[1],
							mesh->pointL[srcvertindex].pos[2]);

					if(mesh->texels > srcvertindex)
					{
						part.getVerts().back().texcoords.set(
							(mesh->texelL[srcvertindex])[0],
							(mesh->texelL[srcvertindex])[1]);
					}

					part.getVerts().back().normal.set(
						normals[f*9+v*3],
						normals[f*9+v*3 + 1],
						normals[f*9+v*3 + 2]);

				}

				//set the vertex indices of the tri
				part.getTris().back().v[0] = v0index;
				part.getTris().back().v[1] = v0index + 1;
				part.getTris().back().v[2] = v0index + 2;

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



		//model_out.model_parts.push_back(CS::ModelPart());
		//CS::ModelPart& part = model_out.model_parts.back();


		//------------------------------------------------------------------------
		//read vertices
		//------------------------------------------------------------------------
		/*part.getVerts().resize(mesh->points);
		
		for(unsigned int i=0; i<mesh->points; ++i)
		{
			part.getVerts()[i].pos.set(
				mesh->pointL[i].pos[0],
				mesh->pointL[i].pos[1],
				mesh->pointL[i].pos[2]);

			if(mesh->texels > i)//if there are at least i tex coords
			{
				part.getVerts()[i].texcoords.set(
					(mesh->texelL[i])[0],
					(mesh->texelL[i])[1]);
			}


		}

		//------------------------------------------------------------------------
		//read triangles
		//------------------------------------------------------------------------
		//TEMP:
		//std::string first_tri_mat_name;
		//if(mesh->faces > 0)
		//	first_tri_mat_name = mesh->faceL[0].material;


		part.getTris().resize(mesh->faces);
		for(unsigned int f=0; f<mesh->faces; ++f)
		{
			const std::string facemat = mesh->faceL[f].material;
			assert(facemat == first_tri_mat_name);

			part.getTris()[f].v[0] = mesh->faceL[f].points[0];
			part.getTris()[f].v[1] = mesh->faceL[f].points[1];
			part.getTris()[f].v[2] = mesh->faceL[f].points[2];

			part.getTris()[f].smoothing_group = mesh->faceL[f].smoothing;
		}

		part.autogen_normals = true;

		//------------------------------------------------------------------------
		//set the material for this part
		//------------------------------------------------------------------------
		CS::Material& mat = part.material;

		if(mesh->faces > 0)
		{
			const std::string matname = mesh->faceL[0].material;
			mat.setName(matname);

			//------------------------------------------------------------------------
			//find the right 3ds material
			//------------------------------------------------------------------------
			Lib3dsMaterial* material = file->materials;
			while(material)
			{
				if(std::string(material->name) == matname)
				{
					//found the right material

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

			

				}

				material = material->next;
			}
		}*/


		
		mesh = mesh->next;
	}

	lib3ds_file_free(file);
	lib3ds_io_free(io);
}

#endif





#ifdef LIB3DS_SUPPORT

void Lib3dsFormatDecoder::streamModel(/*const void* data, int datalen, */const std::string& filename, ModelLoadingStreamHandler& handler, float scale)// throw (ModelFormatDecoderExcep)
{
	handler.addUVSetExposition("albedo", 0);
	handler.addUVSetExposition("default", 0);

	Lib3dsFile* file = NULL;
	Lib3dsIo *io = NULL;

	{
	std::vector<unsigned char> datavec;
	readEntireFile(filename, datavec);

	MyBuf buf;
	buf.data = &datavec[0];//data;
	buf.datalen = (int)datavec.size();//datalen;
	buf.pos = 0;
	buf.error = false;

	io = lib3ds_io_new(
		&buf, 
		my_fileio_error_func,
		my_fileio_seek_func,
		my_fileio_tell_func,
		my_fileio_read_func,
		my_fileio_write_func
	);

	if(!io) 
	{
		throw ModelFormatDecoderExcep("Failed to create a Lib3dsIo struct.");
	}

	file = lib3ds_file_new();
	if(!file) 
		throw ModelFormatDecoderExcep("Failed to create a Lib3dsFile struct.");

	if(!lib3ds_file_read(file, io))
	{
		free(file);
		throw ModelFormatDecoderExcep("3ds read failed.");
	}
	}

	assert(file);

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

	std::vector<Vec2f> texcoords(0);
	std::vector<float> normals;//(mesh->faces * 9);
	unsigned int num_verts_added = 0;
	for(Lib3dsMesh* mesh = file->meshes; mesh; mesh = mesh->next)
	{
		//------------------------------------------------------------------------
		//calculate normals for mesh
		//------------------------------------------------------------------------
		//void lib3ds_mesh_calculate_normals(Lib3dsMesh *mesh, Lib3dsVector *normalL)
		normals.resize(mesh->faces * 9);
		if(mesh->faces > 0)//otherwise will crash when faces == 0
			lib3ds_mesh_calculate_normals(mesh, (Lib3dsVector*)&normals[0]);

		for(unsigned int f=0; f<mesh->faces; ++f)//for each tri
		{
			std::string facemat = mesh->faceL[f].material;//the material for this triangle
			if(facemat == "")
				facemat = "dummy";//assign to dummy material if not specified


			if(mesh->texels > 0)
			{
				handler.setMaxNumTexcoordSets(1);
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
						texcoords.resize(1);

						texcoords[0].set(
							(mesh->texelL[srcvertindex])[0],
							(mesh->texelL[srcvertindex])[1]);
					}

					const Vec3f normal(
						normals[f*9+v*3],
						normals[f*9+v*3 + 1],
						normals[f*9+v*3 + 2]);

					handler.addVertex(pos * scale, normal, texcoords);
				}
				

				const unsigned int vert_indices[3] = {num_verts_added, num_verts_added+1, num_verts_added+2};
				handler.addTriangle(vert_indices, (int)material_index);

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
	lib3ds_io_free(io);
}


#else //LIB3DS_SUPPORT

void Lib3dsFormatDecoder::streamModel(/*const void* data, int datalen, */const std::string& filename, ModelLoadingStreamHandler& handler, float scale)// throw (ModelFormatDecoderExcep)
{
	::fatalError("Lib3dsFormatDecoder::streamModel: LIB3DS_SUPPORT disabled.");
}

#endif //LIB3DS_SUPPORT
