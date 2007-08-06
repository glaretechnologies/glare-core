/*=====================================================================
treenode.cpp
------------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_treenode.h"


//#define OPENGL_DRAWABLE


#include "../maths/mathstypes.h"
#include "../simpleraytracer/ray.h"
#include "jscol_aabbox.h"

#ifdef OPENGL_DRAWABLE
	#include <windows.h>
	#include <GL/gl.h>
#endif


namespace js
{








#ifdef OPENGL_DRAWABLE
void TreeNode::draw() const
{

	if(!positive_child)
	{
		//this is a leaf node
		return;
	}

	glBegin(GL_QUADS);

	Vec3 normal(0.0f, 0.0f, 0.0f);
	normal[dividing_axis] = 1.0f;

	glNormal3fv(normal.data());

	const float ALPHA = 0.2f;

	if(dividing_axis == 0)
	{
		glColor4f(1.0f, 0.0f, 0.0f, ALPHA);
		glVertex3f(dividing_val, min.y, max.z);
		glVertex3f(dividing_val, max.y, max.z);
		glVertex3f(dividing_val, max.y, min.z);
		glVertex3f(dividing_val, min.y, min.z);
	}
	else if(dividing_axis == 1)
	{
		glColor4f(0.0f, 1.0f, 0.0f, ALPHA);
		glVertex3f(min.x, dividing_val, max.z);
		glVertex3f(max.x, dividing_val, max.z);
		glVertex3f(max.x, dividing_val, min.z);
		glVertex3f(min.x, dividing_val, min.z);
	}
	else if(dividing_axis == 2)
	{
		glColor4f(0.0f, 0.0f, 1.0f, ALPHA);
		glVertex3f(min.x, max.y, dividing_val);
		glVertex3f(max.x, max.y, dividing_val);
		glVertex3f(max.x, min.y, dividing_val);
		glVertex3f(min.x, min.y, dividing_val);
	}
	else
	{
		assert(0);
	}


	
	glEnd();



	if(positive_child)
		positive_child->draw();

	if(negative_child)
		negative_child->draw();


}
#endif

} //end namespace js






