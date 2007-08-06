/*=====================================================================
tritree.cpp
-----------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_tritree.h"


#include "jscol_treenode.h"

#ifdef OPENGL_DRAWABLE
	#include <windows.h>
	#include <GL/gl.h>
#endif


namespace js
{




TriTree::TriTree()
//:	rootnode(NULL)
{
//	rootnode = new TreeNode(0);
}


TriTree::~TriTree()
{
//	delete rootnode;
}

void TriTree::insertTri(const js::Triangle& tri)
{
	//rootnode->addTri(tri);
	tris.push_back(tri);
}

	//returns dist till hit tri, neg number if missed.
float TriTree::traceRay(const ::Ray& ray, js::Triangle*& hit_tri_out)
{
	//return rootnode->traceRay(ray, 0.0f, 1e9f, hit_tri_out);
	return rootnode->traceRayIterative(ray, hit_tri_out);
}

void TriTree::build()
{
	std::vector<js::Triangle*> tripointers(tris.size());
	for(int i=0; i<tris.size(); ++i)
		tripointers[i] = &tris[i];

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	rootnode->calcAABB(tripointers);


	rootnode->build(tripointers, 0);
}


void TriTree::draw() const
{
#ifdef OPENGL_DRAWABLE
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);			
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);

	rootnode->draw();
#endif
}


} //end namespace jscol






