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
#ifndef __RAYTRACER_H__
#define __RAYTRACER_H__

#include <list>

class Object;
class Graphics2d;
//#include "../simplewin2d/simplewin2d.h"
class Simple3d;
class Light;
class Vec3;
#include "world.h"
#include "screenpoint.h"
#include <vector>
#include "cellmanager.h"
class Cell;
#include "../maths/plane2.h"
//#include "../simple3d/visibleobject.h"

/*===================================================================
RayTracer
---------
This is the active class that does the image creation.
====================================================================*/
class RayTracer
{
public:
	RayTracer();
	~RayTracer();



	inline void insertObject(Object* o){ world->insertObject(o); }
	inline void removeObject(Object* o){ world->removeObject(o); }

	inline void deleteAllObjects(){ world->deleteAllObjects(); }
	inline void removeAllObjects(){ world->removeAllObjects(); }

	inline void insertLight(RayT::Light* l){ world->insertALight(l); }
	inline void deleteAllLights(){ world->deleteAllLights(); }
	inline void removeAllLights(){ world->removeAllLights(); }
	inline void removeLastLight(){ world->removeLastLight(); }
	inline const std::vector<RayT::Light*>& getLights() const { return world->getLights(); }



	void drawFrame(float time, int windowwidth, int windowheight, 
								  const Vec3& camerapos, const Vec3& cam_upvec, 
								const Vec3& cam_rightvec, const Vec3& cam_forwardsvec,
								const CoordFrame& camera_frame, float yaw, float pitch);

	World& getWorld(){ return *world; }




	//-----------------------------------------------------------------
	//render a screenspace square
	//-----------------------------------------------------------------
	void renderSquare(	const ScreenPoint& topleft, const ScreenPoint& topright,
						const ScreenPoint& botleft, const ScreenPoint& botright,
						int currentdepth/*, bool last_square_in_row*/);

	/*
	//let bc be the hypoteneuse
	void renderTri(int currentdepth, const ScreenPoint& a, const ScreenPoint& b,
						const ScreenPoint& c);

	bool renderLineCell( const ScreenPoint& topleft, const ScreenPoint& topright,
						const ScreenPoint& botleft, const ScreenPoint& botright,
						Cell& cell);

	//screenpoints anticlockwise from start pos
	bool renderCornerType(const ScreenPoint& s1, const ScreenPoint& s2, 
								const ScreenPoint& s3, const ScreenPoint& cornerspoint, Cell& cell, 
								const Vec2& startnudge, const Vec2& endnudge);


	//screenpoints anticlockwise from start pos
	bool renderType2or4(const ScreenPoint& s1, const ScreenPoint& s2, 
								const ScreenPoint& s3, const ScreenPoint& s4, Cell& cell, const Vec2& nudgevec);


	void divideAndRenderTri(const ScreenPoint& a, const ScreenPoint& b,
						const ScreenPoint& c, const std::vector<Plane2>& lines, int curlineindex);

	//where line intersects ac and bc (ie where c is the isolated vertex)
	void divAndRenderTri(const ScreenPoint& a, const ScreenPoint& b, const ScreenPoint& c,
								float ac_fraction, float bc_fraction, const std::vector<Plane2>& lines, 
								int curlineindex);*/


	void incSubdivisionDepth(){ MAX_SUBDIVISION_DEPTH++; }

	void decSubdivisionDepth()
	{ 
		if(MAX_SUBDIVISION_DEPTH > 0)
			MAX_SUBDIVISION_DEPTH--;
	}

	void setSubdivisionDepth(int d){ MAX_SUBDIVISION_DEPTH = d; }
	void setFillTris(bool fill){ fill_tris = fill; }
	void toggleFillTris(){ fill_tris = !fill_tris; }
	void toggleCellOutlines(){ world->toggleCellOutlines(); }
private:

	//terminate tri strip if in the middle of one
	void testTerminateTriStrip(const ScreenPoint& top, const ScreenPoint& bottom);

	//void initSquare(const Vec2& topleft, const Vec2& topright, 
	//					const Vec2& botleft, const Vec3& botright

	inline bool isObjectInPrimUseVec(Object* object);
	inline void addObjectToPrimUseVec(Object* object);
	inline void testAddObToPrimUseVec(Object* object);

	CellManager cellmanager;
	World* world;
	Vec3 cam_right;
	Vec3 cam_down;
	Vec3 cam_top_left;

	std::vector<ScreenPoint> screenpoints;


	Ray ray;
	inline void getRayUnitDirForScreenspacePos(const Vec2& screenspace_pos, Vec3& ray_unitdir_out);

	int numquadsdrawn;
	int last_tex_used_id;


	int MAX_SUBDIVISION_DEPTH;
	bool fill_tris;

	bool tristrip_started;
	bool drawing;
	bool texturing;


};

void RayTracer::testAddObToPrimUseVec(Object* object)
{
	if(object && !isObjectInPrimUseVec(object))
			addObjectToPrimUseVec(object);
}


bool RayTracer::isObjectInPrimUseVec(Object* object)
{
	for(int i=0; i<world->primary_objects_use_size; ++i)
	{
		if(world->primary_objects_use[i] == object)
			return true;
	}
	return false;
}

void RayTracer::addObjectToPrimUseVec(Object* object)
{
	//NOTE: optimise me etc..

		//obstructors need to come b4 non obstructors

	/* TEMP NOT NEEDED CAUSE OBSTRUCTORS CAN BLOCK if(object->isObstructor())
	{
		//insert in vector at first_nonobstructor_index

		//-----------------------------------------------------------------
		//first move all non-obstructors back
		//-----------------------------------------------------------------
		for(int i=world->primary_objects_use_size-1; i >= world->first_nonobstructor_index_use; --i)
		{
			//move object to right
			world->primary_objects_use[i+1] = world->primary_objects_use[i];
		}

		world->primary_objects_use[ world->first_nonobstructor_index_use++ ] = object;

		world->primary_objects_use_size++;
	}
	else
	{*/
		//object is non-obstructor

		//-----------------------------------------------------------------
		//insert at back of queue
		//-----------------------------------------------------------------
//		world->primary_objects_use[world->primary_objects_use_size++] = object;
	//}

	//-----------------------------------------------------------------
	//insert sorted by getClosestDistFromCam(), smallest first
	//-----------------------------------------------------------------
	for(int i=0; i<world->primary_objects_use_size; ++i)
	{
		//NOTE: investigate this more closely (>=)
		if(object->getClosestDistFromCam() <= world->primary_objects_use[i]->getClosestDistFromCam())
		{
			//-----------------------------------------------------------------
			//insert 
			//-----------------------------------------------------------------
			
			//-----------------------------------------------------------------
			//push rest of list to the right
			//-----------------------------------------------------------------
			for(int z=world->primary_objects_use_size-1; z >= i; --z)
			{
				//move object to right
				world->primary_objects_use[z+1] = world->primary_objects_use[z];
			}

			world->primary_objects_use[i] = object;

			world->primary_objects_use_size++;
			return;
		}
	}

	//else bigger than them all so insert at end
	world->primary_objects_use[world->primary_objects_use_size++] = object;
}












void RayTracer::getRayUnitDirForScreenspacePos(const Vec2& screenspace_pos, Vec3& ray_unitdir_out)
{
	//ray.unitdir = cam_top_left + (screenspace_pos.x * ASPECT_RATIO) * cam_right
	//						+ screenspace_pos.y * cam_down;

//	ray_unitdir_out.set(NEG_ASPECT_RATIO_2, 1, 0.5);/*cam_top_left*/;
//	ray_unitdir_out.addMult(/*cam_right*/CAMRIGHT, screenspace_pos.x * ASPECT_RATIO);
//	ray_unitdir_out.addMult(/*cam_down*/CAMDOWN, screenspace_pos.y);

	ray_unitdir_out.set((NEG_ASPECT_RATIO_2_FOV_MULT + screenspace_pos.x * ASPECT_RATIO_FOV_MULT),
							1,
							(0.5 - screenspace_pos.y) * FOV_MULT/*TEMP*/);

	ray_unitdir_out.normalise();
	//ray_unitdir_out.fastNormalise();
}

#endif //__RAYTRACER_H__