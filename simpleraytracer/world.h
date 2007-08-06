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
#ifndef __WORLD_H__
#define __WORLD_H__

#include <vector>
#include "../maths/vec3.h"

class Vec3;
class Object;
class Light;
class Ray;
class Colour;


/*===================================================================
World
-----
this class represents the scene (collection of objects/lights),
and supplies operations on the scene (raytrace, lighting etc..)
====================================================================*/
class World
{
public:
	World();
	~World();

	void getColourForRay(const Ray& ray, Colour& colour_out);
	
	bool doesFiniteRayHitAnything(const Ray& ray, float length);


	//-----------------------------------------------------------------
	//object/light insertion funcs
	//objects inserted in this way will be deleted upon distruction of the world object
	//-----------------------------------------------------------------
	void insertObject(Object* ob);

	void insertLight(Light* light);


	//Calculate the lighting at a certain point with a certain normal
	void calcLightingForSurfPoint(const Object& ob, const Vec3& unit_cam_ray, const Vec3& ob_intersect_pos,
		const Vec3& ob_normal, Colour& lighting_out);


private:

	std::vector<Object*> objects;
	std::vector<Light*> lights;

	Vec3 cam_pos;
	Vec3 cam_down;
	Vec3 cam_right;
	Vec3 cam_forwards;
};


#endif //__WORLD_H__