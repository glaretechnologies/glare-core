/*=====================================================================
FormatDecoderPLY.cpp
--------------------
File created by ClassTemplate on Sat Dec 02 04:33:29 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "FormatDecoderPLY.h"


// Grab rply src from: http://www.cs.princeton.edu/~diego/professional/rply/


#include "../dll/include/IndigoMesh.h"
#include "../utils/Exception.h"
#include "../rply-1.1.1/rply.h"
#include <assert.h>
#include <vector>


using namespace Indigo;


//dirty nasty global variables
static Vec3f current_vert_pos;//NOTE TEMP HACK GLOBAL VAR
static const std::vector<Vec2f> texcoord_sets;
static unsigned int current_vert_indices[3];
static float ply_scale = 1.0f;//TEMP NASTY HACK


static int vertex_callback(p_ply_argument argument) 
{

	//------------------------------------------------------------------------
	//get pointer to current mesh, current component of vertex pos.
	//------------------------------------------------------------------------
	long current_cmpnt = 0;
	void* pointer_usrdata = NULL;
	ply_get_argument_user_data(argument, &pointer_usrdata, &current_cmpnt);

	//PlyMesh* current_mesh = static_cast<PlyMesh*>(pointer_usrdata);
	//assert(current_mesh);

	Indigo::Mesh* handler =  static_cast<Indigo::Mesh*>(pointer_usrdata);


	//get vert index
	long vertindex;
	ply_get_argument_element(argument, NULL, &vertindex);

	//current_mesh->verts[vertindex].pos[current_cmpnt] = ply_get_argument_value(argument);
	current_vert_pos[current_cmpnt] = (float)ply_get_argument_value(argument);
	if(current_cmpnt == 2)
	{
		handler->addVertex(current_vert_pos * ply_scale/*, Vec3f(0, 0, 1)*/); // , texcoord_sets);//NOTE: FUCKING NASTY HARD CODED NORMAL HERE
	}
		
    return 1;
}


static int face_callback(p_ply_argument argument) 
{
	//------------------------------------------------------------------------
	//get pointer to current mesh
	//------------------------------------------------------------------------
	void* pointer_usrdata = NULL;
	ply_get_argument_user_data(argument, &pointer_usrdata, NULL);

	//PlyMesh* current_mesh = static_cast<PlyMesh*>(pointer_usrdata);
	//assert(current_mesh);
	Indigo::Mesh* handler =  static_cast<Indigo::Mesh*>(pointer_usrdata);


	//get current tri index
	long tri_index;
	ply_get_argument_element(argument, NULL, &tri_index);


	long length, value_index;
    ply_get_argument_property(argument, NULL, &length, &value_index);

	if(value_index >= 0 && value_index <= 2)
	{
		//current_mesh->tris[tri_index].vert_indices[value_index] = (int)ply_get_argument_value(argument);

		current_vert_indices[value_index] = (unsigned int)ply_get_argument_value(argument);

		if(value_index == 2)
		{
			const unsigned int mat_index = 0;
			const unsigned int uv_indices[] = {0, 0, 0};
			handler->addTriangle(current_vert_indices, uv_indices, mat_index);
		}
	}

    return 1;
}


void FormatDecoderPLY::streamModel(const std::string& pathname, Indigo::Mesh& handler, float scale)
{
	ply_scale = scale;

	handler.setMaxNumTexcoordSets(0);
	handler.addMaterialUsed("default");
	//handler.addUVSetExposition("default", 0);


	p_ply ply = ply_open(pathname.c_str(), NULL, 0, NULL);

    if(!ply) 
		throw glare::Exception("could not open file '" + pathname + "' for reading.");
    
	if(!ply_read_header(ply))
		throw glare::Exception("could not read header.");
		
    //const long nvertices =
	ply_set_read_cb(ply, "vertex", "x", vertex_callback, &handler, 0);

	//mesh_out.verts.resize(nvertices);

    ply_set_read_cb(ply, "vertex", "y", vertex_callback, &handler, 1);
    ply_set_read_cb(ply, "vertex", "z", vertex_callback, &handler, 2);

    //const long ntriangles = 
	ply_set_read_cb(ply, "face", "vertex_indices", face_callback, &handler, 0);

	//mesh_out.tris.resize(ntriangles);
	
    if(!ply_read(ply)) 
		throw glare::Exception("read of body failed.");

    ply_close(ply);

	handler.endOfModel();
}
