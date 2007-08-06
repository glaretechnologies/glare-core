#ifndef __STUDIOMESH_H__
#define __STUDIOMESH_H__

#include <vector>
#include "C:\programming\simple3d\simple3d.h"
#include "C:\programming\simple3d\triangle.h"
#include "jscol_trimesh.h"


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


class StudioMesh
{
public:
	StudioMesh(const char* filename);
	~StudioMesh();




	void buildJSTriMesh(js::TriMesh& trimesh_out);
	//void buildGraphicsTriList(std::vector<Triangle>& tris_out);

private:
	long EatChunk(char* buffer);

	CModel g_model;
};






#endif //__STUDIOMESH_H__