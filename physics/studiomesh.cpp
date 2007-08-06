#include "studiomesh.h"

#include "jscol_trimesh.h"

#include <iostream.h>
#include <istream>
#include <fstream.h>

StudioMesh::StudioMesh(const char* filename)
{

	g_model.numverts = 0;
	g_model.verts = NULL;
	g_model.numpolys = 0;
	g_model.polys = NULL;


	//-----------------------------------------------------------------
	//
	//-----------------------------------------------------------------
    ifstream    file;
    bool        doneloading = false;
    int         filelength;
    char*       memfile;        // file loaded into memory

    // load entire file into memory (easier to deal with)
    file.open(filename, ios::binary);
    file.seekg(0, ios::end);
    filelength = file.tellg();
    memfile = new char[filelength];
    file.seekg(0, ios::beg);
    file.read(memfile, filelength);
    // parse it
    EatChunk(memfile);
    delete memfile;

}


// EatChunk
//
//  This function recursively handles chunks
// in a .3ds file.  When the function exits,
// the return value (i) should be the length
// of the current chunk.  Right now we only
// create one model (the first one listed in
// the file) and read in verts and polys only.
// To grab other info from the file, create
// a new case label in the switch statement
// for the chunk type you want to react to.
// 
long StudioMesh::EatChunk(char* buffer)
{
    short   chunkid;
    long    chunklength;
    int     i = 0;  // index into current chunk
    int     j;

    chunkid = *(short*)(buffer);
    chunklength = *(long*)(buffer+2);
    i = 6;
    switch (chunkid)
    {
    case 0x4D4D:    // main file
        while ((*(short*)(buffer+i) != 0x3D3D) &&
            (*(short*)(buffer+i) != 0xB000))
            i += 2;
        break;
    case 0x3D3D:    // editor data
        break;
    case 0x4000:    // object description
        while (*(buffer+(i++)) != 0);   // get past string description
        break;
    case 0x4100:    // triangular polygon list
        break;
    case 0x4110:    // vertex list
        if (g_model.numverts == NULL)
        {
            g_model.numverts = *(short*)(buffer+i);
            i+=2;
            g_model.verts = new CVertex[g_model.numverts];
            for (j=0;j<g_model.numverts;j++)
            {
                g_model.verts[j].x = *(float*)(buffer+i);
                i+=4;
                g_model.verts[j].y = *(float*)(buffer+i);
                i+=4;
                g_model.verts[j].z = *(float*)(buffer+i);
                i+=4;
            }
        }
        else
            i = chunklength;
        break;
    case 0x4120:
        if (g_model.numpolys == NULL)
        {
            g_model.numpolys = *(short*)(buffer+i);
            i+=2;
            g_model.polys = new CPolygon[g_model.numpolys];
            for (j=0;j<g_model.numpolys;j++)
            {
                g_model.polys[j].verts[0] = *(short*)(buffer+i);
                i+=2;
                g_model.polys[j].verts[1] = *(short*)(buffer+i);
                i+=2;
                g_model.polys[j].verts[2] = *(short*)(buffer+i);
                i+=2;
                i+=2;   // skip face info
            }
        }
        else
            i = chunklength;
        break;
    default:
        i = chunklength;    // skips over rest of chunk (ignores it)
        break;
    }

    // eat child chunks
    while (i < chunklength)
        i += EatChunk(buffer+i);
    return chunklength;
}
	

StudioMesh::~StudioMesh()
{
	delete[] g_model.verts;
	delete[] g_model.polys;
}

/*
struct CVertex
{
    float x,y,z;
};

struct CPolygon
{
    int verts[3];
};

struct CModel
{
    CVertex* verts;
    int numverts;
    CPolygon* polys;
    int numpolys;
};
*/

const float TEMP_MULTIPLIER = 0.004f;


void StudioMesh::buildJSTriMesh(js::TriMesh& trimesh_out)
{
	for(int i=0; i<g_model.numpolys; i++)
	{
		CPolygon& poly = g_model.polys[i];

		trimesh_out.insertTri( js::Triangle( 
			
			Vec3( g_model.verts[ poly.verts[0] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[0] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[0] ].z * TEMP_MULTIPLIER ),
			Vec3( g_model.verts[ poly.verts[1] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[1] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[1] ].z * TEMP_MULTIPLIER ),
			Vec3( g_model.verts[ poly.verts[2] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[2] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[2] ].z * TEMP_MULTIPLIER )

			));
	}

}

/* TEMP void StudioMesh::buildGraphicsTriList(std::vector<Triangle>& tris_out)
{	
	for(int i=0; i<g_model.numpolys; i++)
	{
		CPolygon& poly = g_model.polys[i];

		tris_out.push_back( Triangle( 
			
			Vec3( g_model.verts[ poly.verts[0] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[0] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[0] ].z * TEMP_MULTIPLIER ),
			Vec3( g_model.verts[ poly.verts[1] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[1] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[1] ].z * TEMP_MULTIPLIER ),
			Vec3( g_model.verts[ poly.verts[2] ].x * TEMP_MULTIPLIER ,g_model.verts[ poly.verts[2] ].y * TEMP_MULTIPLIER, g_model.verts[ poly.verts[2] ].z * TEMP_MULTIPLIER )

			));
	}

}*/