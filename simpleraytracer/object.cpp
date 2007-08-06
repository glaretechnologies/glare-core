/*===================================================================

  
  digital liberation front 2002
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman[/ Ono-Sendai]
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#include "object.h"


#include "geometry.h"
#include "../maths/vec3.h"
#include "world.h"


Object::Object(const Material& material_, Geometry* geometry_)
:	material(material_),
	geometry(geometry_)
{

}
	
Object::~Object()
{
	delete geometry;
}



//get the colour of the ray that is known to hit this object
void Object::getColourForRay(const Vec3& intersect_pos, const Vec3& unit_raydir, 
		World& world, Colour& colour_out) const
{
	colour_out.set(0.0f,0.0f,0.0f);

	//------------------------------------------------------------------------
	//get normal at hit position
	//------------------------------------------------------------------------
	const Vec3 normal = geometry->getNormalForPos(intersect_pos);

	//------------------------------------------------------------------------
	//add lighting at this point
	//------------------------------------------------------------------------
//	world.calcLightingForSurfPoint(*this, 
	world.calcLightingForSurfPoint(*this, unit_raydir, intersect_pos, normal, colour_out);
}

void Object::setMaterial(int index, Material* mat)//auto-resizes material array
{
	if(materials.size() < index + 1)
	    materials.resize(index + 1);
	materials[index] = mat;
}



