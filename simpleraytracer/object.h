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
#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "material.h"
class Geometry;
class Vec3;
class World;

/*===================================================================
Object
------
basically a (material, geometry) pair
====================================================================*/
class Object
{
public:

	//deletes 'geometry' on destruction 
	Object(const Material& material, Geometry* geometry);
	~Object();


	const Material& getMaterial() const { return material; }
	//Material& getMaterial(){ return material; }


	Geometry& getGeometry(){ return *geometry; }



		//get the colour of the ray that is known to hit this object
	void getColourForRay(const Vec3& intersect_pos, const Vec3& unit_raydir, 
		World& world, Colour& addlighting_out) const;


private:
	Material material;//material of the object
	Geometry* geometry;//shape of the object

};



#endif //__OBJECT_H__
