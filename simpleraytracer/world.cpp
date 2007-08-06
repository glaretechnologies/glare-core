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
#include "world.h"


#include "object.h"
#include "colour.h"
#include "ray.h"
#include "light.h"
#include "geometry.h"
#include <assert.h>


const Colour background_colour(0.2f, 0.2f, 0.2f);
const Colour ambient_lighting(0.3f, 0.3f, 0.3f);

World::World()
{
}

World::~World()
{
	for(int i=0; i<objects.size(); ++i)
	{
		delete objects[i];
	}

	for(int z=0; z<lights.size(); ++z)
	{
		delete lights[z];
	}
}


void World::insertObject(Object* ob)
{
	objects.push_back(ob);
}



void World::insertLight(Light* light)
{
	lights.push_back(light);
}



bool World::doesFiniteRayHitAnything(const Ray& ray, float length)
{
	for(int i=0; i<objects.size(); ++i)
	{	
		const float distance = objects[i]->getGeometry().getDistanceUntilHit(ray);
	
		if(distance >= 0 && distance < length)
		{
			//ray hit the object within the specified length!
			return true;
		}
	}

	return false;
}







//gets the colour of a certain ray.
void World::getColourForRay(const Ray& ray, Colour& colour_out)
{

	Object* closest_object = NULL;//the closest object hit by the ray
	float smallest_dist = 1e9f; //the smallest dist the trace got.


	for(int i=0; i<objects.size(); i++)
	{
		const float tracedist = objects[i]->getGeometry().getDistanceUntilHit(ray);
	
		//-----------------------------------------------------------------
		//if this is the closest object hit by ray so far..
		//-----------------------------------------------------------------
		if(tracedist >= 0.0f && tracedist < smallest_dist)
		{
			//-----------------------------------------------------------------
			//remember it
			//-----------------------------------------------------------------
			smallest_dist = tracedist;
			closest_object = objects[i];
		}
	}


	if(closest_object != NULL)
	{

		//-----------------------------------------------------------------
		//calc intersection point.
		//also nudge of the surface a little
		//-----------------------------------------------------------------
		Vec3 intersect_point = ray.startpos;
		intersect_point.addMult(ray.unitdir, smallest_dist * 0.999f);

		closest_object->getColourForRay(intersect_point, ray.unitdir, *this, colour_out);

		return;
	}

	//-----------------------------------------------------------------
	//if the ray didn't hit anything, just return the background colour.
	//-----------------------------------------------------------------
	colour_out = background_colour;
}







void World::calcLightingForSurfPoint(const Object& ob, const Vec3& unit_cam_ray, const Vec3& ob_intersect_pos,
		const Vec3& ob_normal, Colour& colour_out)
{
	colour_out.set(0.0f,0.0f,0.0f);

	//-----------------------------------------------------------------
	//calc direction of reflected camera ray.
	//this is used for the reflected colour and also for specular lighting
	//-----------------------------------------------------------------
	const float neg_raydir_on_normal = -dot(unit_cam_ray, ob_normal);

	Vec3 unit_reflectray_dir = unit_cam_ray;
	unit_reflectray_dir.addMult(ob_normal, neg_raydir_on_normal + neg_raydir_on_normal);
	
	//assert(unit_reflectray_dir.dot(ob_normal) >= 0);
	


	//-----------------------------------------------------------------
	//calc reflected colour
	//-----------------------------------------------------------------
	if(ob.getMaterial().reflect_fraction != 0.0f)
	{
		const Ray reflected_ray(ob_intersect_pos, unit_reflectray_dir);

		//-----------------------------------------------------------------
		//recurse - do the trace and get the colour
		//-----------------------------------------------------------------
		Colour reflected_colour;
		getColourForRay(reflected_ray, reflected_colour);

		colour_out.addMult(reflected_colour, ob.getMaterial().reflect_fraction);
	}


	if(ob.getMaterial().reflect_fraction == 1.0f)
		return;//don't do diffuse/specular lighting, reflected lighting is all we need


	//-----------------------------------------------------------------
	//do non-reflected lighting
	//-----------------------------------------------------------------
	Colour nonreflected_lighting(0.0f,0.0f,0.0f);
	//this will be scaled by the nonreflect fraction at end of func

	//-----------------------------------------------------------------
	//add ambient lighting term
	//-----------------------------------------------------------------
	nonreflected_lighting += ob.getMaterial().diffuse_colour * ambient_lighting;
								


	for(int i=0; i != lights.size(); ++i)
	{
		//for each light...

		Colour light_illum(0.0f,0.0f,0.0f);//illumination from this light

		//-----------------------------------------------------------------
		//get a ref to the light
		//-----------------------------------------------------------------
		const Light& light = *lights[i];

		//-----------------------------------------------------------------
		//get vector from light to point on object surface
		//-----------------------------------------------------------------
		Vec3 unit_light_to_intersect = ob_intersect_pos - light.getPos(); 
			//this will be normalised later.


		//-----------------------------------------------------------------
		//check to see surface is orientated towards light
		//-----------------------------------------------------------------
		float dot = -dotProduct(unit_light_to_intersect, ob_normal);
		
		if(dot < 0.0f)
		{
			//surface was facing away from light, so can't be illuminated by it.
			continue;
		}

		
		//-----------------------------------------------------------------
		//normalise 'unit_light_to_intersect' vector and get dist squared
		//-----------------------------------------------------------------
		float inverse_light_dist;
		const float light_dist = unit_light_to_intersect.normalise_ret_length(inverse_light_dist);
		
		const float inverse_light_dist_sqrd = inverse_light_dist * inverse_light_dist;


		dot *= inverse_light_dist;//dot is now the *actual* dot.
		

		//-----------------------------------------------------------------
		//test to see if this light illuminates this point, or if it is obstructed
		//-----------------------------------------------------------------
		if(this->doesFiniteRayHitAnything(
			Ray(ob_intersect_pos, unit_light_to_intersect * -1.0f), light_dist))
		{	
			continue;//light was blocked, so go on to next light
		}

		//else this light does illuminate this point.

		//------------------------------------------------------------------------
		//add the diffuse lighting contribution
		//------------------------------------------------------------------------
		const float	diffuse_intensity = dot * inverse_light_dist_sqrd;

		light_illum.addMult(ob.getMaterial().diffuse_colour, diffuse_intensity);


		//-----------------------------------------------------------------
		//now do the specular lighting
		//-----------------------------------------------------------------
		if(ob.getMaterial().specular_amount != 0.0f)
		{
			float spec_dot = -dotProduct(unit_reflectray_dir, unit_light_to_intersect);

			if(spec_dot > 0.0f)
			{
				const float specular_intensity = spec_dot * inverse_light_dist_sqrd * 
					pow(spec_dot, ob.getMaterial().specular_coeff) * 
					ob.getMaterial().specular_amount;
			
				//assume the 'specular colour of the material is white'.
				//eg assume a red sphere under a white light will specularly reflect 
				//white light.

				light_illum.add(Colour(specular_intensity, specular_intensity, specular_intensity));
			}

		}//end 'if use specular lighting'

		//------------------------------------------------------------------------
		//add the contribution from this light, modulating the whole thing by the light's colour
		//------------------------------------------------------------------------
		nonreflected_lighting += light_illum * light.getColour();
	
	}//end light loop
	
	//-----------------------------------------------------------------
	//scale by nonreflected fraction, and add to the final returned colour.
	//-----------------------------------------------------------------
	colour_out.addMult(nonreflected_lighting, 1.0f - ob.getMaterial().reflect_fraction);

}

