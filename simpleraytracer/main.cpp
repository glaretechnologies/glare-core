/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/

#include "../simplewin2d/simwin_framework.h"
#include "../simplewin2d/simplewin2d.h"
#include "../utils/timer.h"
#include "world.h"
#include "ray.h"
#include "colour.h"
#include "material.h"
#include "rayplane.h"
#include "raysphere.h"
#include "object.h"
#include "light.h"
#include <fstream>


//------------------------------------------------------------------------
//forwards declarations
//------------------------------------------------------------------------
const Vec3 getUnitDirForImageCoords(int x, int y);

//------------------------------------------------------------------------
//some global objects
//------------------------------------------------------------------------
World* world = NULL;

Light* light1 = NULL;

Object* sphere1 = NULL;

Timer timer;

//dimensions of the window
const int width = 400;
const int height = 400;

//position of the camera
const Vec3 campos(-3,0,1);

int framenum = 0;

void getWindowDims(int& width_, int& height_)
{
	width_ = width;
	height_ = height;
}

void doInit(HINSTANCE hinstance, HWND windowhandle, bitmapWindow& graphics)
{
	//------------------------------------------------------------------------
	//create the world
	//------------------------------------------------------------------------
	world = new World();


	//------------------------------------------------------------------------
	//insert some objects into the world
	//------------------------------------------------------------------------
	{
	Material planemat(Colour(1,1,1), 0.5, 20, 0.6); 
		
	RayPlane* planegeom = new RayPlane(Plane(Vec3(0,0,1), 0));

	Object* groundplane = new Object(planemat, planegeom);

	world->insertObject(groundplane);
	}

	
	{
	Material mat(Colour(1,1,1), 0.5, 20, 0); 
		
	RayPlane* geom = new RayPlane(Plane(Vec3(0,1,0), -6));

	Object* theplane = new Object(mat, geom);

	world->insertObject(theplane);
	}

	{

	Material mat(Colour(0,1,0), 0.4, 20, 0.7); 
		
	RaySphere* geom = new RaySphere(Vec3(4,0,2), 0.7);

	sphere1 = new Object(mat, geom);

	world->insertObject(sphere1);
	}

	{

	Material mat(Colour(1,0,0), 0.4, 10, 0.5); 
		
	RaySphere* geom = new RaySphere(Vec3(4,0,2), 0.7);

	Object* thesphere = new Object(mat, geom);

	world->insertObject(thesphere);
	}



	//world->insertLight(new Light(Vec3(0,0,10), Colour(1, 0, 0) * 100));

	light1 = new Light(Vec3(15,0,5), Colour(1, 1, 1) * 100);
	
	world->insertLight(light1);

	timer.reset();
}


void doMain(bitmapWindow& graphics)
{
	//------------------------------------------------------------------------
	//animate the movements of some of the entities
	//------------------------------------------------------------------------
	const float time = timer.getSecondsElapsed();
	

	if(light1)
		light1->pos = Vec3(0 + cos(time) * 5, sin(time) * 5, 5);
	

	if(sphere1)
		static_cast<RaySphere&>(sphere1->getGeometry()).centerpos = 
					Vec3(3 + cos(time * 0.2) * 2, sin(time * 0.2) * 2, 1);


	//------------------------------------------------------------------------
	//draw the scene
	//------------------------------------------------------------------------
	for(int x=0; x<width; ++x)
		for(int y=0; y<height; ++y)
		{
			//------------------------------------------------------------------------
			//calculate the ray from the camera to trace thru the world
			//------------------------------------------------------------------------
			const Ray ray(campos, getUnitDirForImageCoords(x, y));

			Colour colour;
			world->getColourForRay(ray, colour);

			//------------------------------------------------------------------------
			//do some gamma correction
			//------------------------------------------------------------------------
			/*const float exponent = 0.6;
			colour.r = pow(colour.r, exponent);
			colour.g = pow(colour.g, exponent);
			colour.b = pow(colour.b, exponent);

			colour *= 0.5;*/

			//------------------------------------------------------------------------
			//make sure no components exceed 1
			//------------------------------------------------------------------------
			colour.positiveClipComponents();

			//------------------------------------------------------------------------
			//draw the pixel
			//------------------------------------------------------------------------
			graphics.drawPixel(x, y, Vec3(colour.b, colour.g, colour.r));
		}

	framenum++;
}

void doShutdown()
{
	//------------------------------------------------------------------------
	//write frame rate to a file
	//------------------------------------------------------------------------
	if(framenum != 0)
	{
		const float total_time = (float)timer.getSecondsElapsed();
		const float frame_rate = (float)framenum / total_time;
		std::ofstream fps_file("fps.txt");
		if(fps_file)
		{
			fps_file << "num frames: " << framenum << std::endl;
			fps_file << "time elapsed: " << total_time << std::endl;
			fps_file << "average frames per second: " << frame_rate << std::endl;
		}
	}

	delete world;
	world = NULL;
}


//the below is for a camera pointing in the x direction, with y off to the left and z up.
const Vec3 getUnitDirForImageCoords(int x, int y)
{
	const float xfrac = (float)x / (float)width;
	const float yfrac = (float)y / (float)height;

	return normalise(Vec3(1.0f, -(xfrac - 0.5f), -(yfrac - 0.5f)));
}