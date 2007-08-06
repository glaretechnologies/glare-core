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
#include "raytracer.h"

//#include "triangle.h"
#include "world.h"
#include "colour.h"

//#include "C:\programming\simple3d\simple3d.h"
//#include "../graphics2d/graphics2d.h"

#include "ray.h"
#include "globals.h"
#include "C:\programming\utils\timer.h"
#include "../maths/maths.h"

#include <windows.h>
#include <gl/gl.h>







const int UNSUBD_SCREENPOINT_RES = 40;
//const int SCREENPOINT_GRID_SIZE = SCREENPOINT_RES*SCREENPOINT_RES;
//const float SCREENPOINT_RES_FLOAT = (float)SCREENPOINT_RES;

const int PRECOMPUTE_SUBD_DEPTH = 3;


const int POINTS_PER_UNSUBD_POINT = 1 << PRECOMPUTE_SUBD_DEPTH;


const int SUBD_SCREENPOINT_RES = UNSUBD_SCREENPOINT_RES + 
						((UNSUBD_SCREENPOINT_RES-1) * (POINTS_PER_UNSUBD_POINT-1));

const int SUBD_SCREENPOINT_GRID_SIZE = SUBD_SCREENPOINT_RES * SUBD_SCREENPOINT_RES;

const int UNSUBD_Y_STEP = SUBD_SCREENPOINT_RES * POINTS_PER_UNSUBD_POINT;

inline int getIndexForScreenPoint(int x, int y)
{
	return x + y*SUBD_SCREENPOINT_RES;
}

inline int getIndexForUnsubdividedScreenPoint(int x, int y)
{
	return (x + y*SUBD_SCREENPOINT_RES) * POINTS_PER_UNSUBD_POINT;
}


RayTracer::RayTracer()
:	world(NULL),
	screenpoints(SUBD_SCREENPOINT_GRID_SIZE),
	cellmanager(UNSUBD_SCREENPOINT_RES-1, UNSUBD_SCREENPOINT_RES-1)
{
	MAX_SUBDIVISION_DEPTH = 3;

	fill_tris = true;


	world = new World(cellmanager);

//	ray.intensity_fraction = 1.0;



	//-----------------------------------------------------------------
	//set screenpoints screenspace position
	//-----------------------------------------------------------------
	for(int y=0; y<SUBD_SCREENPOINT_RES; y++)
		for(int x=0; x<SUBD_SCREENPOINT_RES; x++)
		{
			//	inline void reset(World& world, const Vec2& screenspace_pos_, const Ray& ray)	
			const Vec2 screenspacepos((float)x /  ((float)SUBD_SCREENPOINT_RES-1.0f), 
										(float)y / ((float)SUBD_SCREENPOINT_RES-1.0f));

			Vec3 ray_unitdir;
			getRayUnitDirForScreenspacePos(screenspacepos, ray_unitdir);

			//screenpoints[x + SCREENPOINT_RES*y].screenspace_pos = screenspacepos;
			screenpoints[ getIndexForScreenPoint(x, y) ].init(screenspacepos, ray_unitdir,
																		getIndexForScreenPoint(x, y));
		}




	//-----------------------------------------------------------------
	//set screenpoints screenspace position
	//-----------------------------------------------------------------
/*	for(int x=0; x<SCREENPOINT_RES; x++)
		for(int y=0; y<SCREENPOINT_RES; y++)
		{
			//	inline void reset(World& world, const Vec2& screenspace_pos_, const Ray& ray)	
			const Vec2 screenspacepos((float)x /  (SCREENPOINT_RES_FLOAT-1.0f), 
										(float)y / (SCREENPOINT_RES_FLOAT-1.0f));

			Vec3 ray_unitdir;
			getRayUnitDirForScreenspacePos(screenspacepos, ray_unitdir);

			//screenpoints[x + SCREENPOINT_RES*y].screenspace_pos = screenspacepos;
			screenpoints[x + SCREENPOINT_RES*y].init(screenspacepos, ray_unitdir);
		}*/

}

RayTracer::~RayTracer()
{
	delete world;
}





void RayTracer::drawFrame(float time, int windowwidth, int windowheight, 
								  const Vec3& camerapos, const Vec3& cam_upvec, 
		const Vec3& cam_right_vec, const Vec3& cam_forwards_vec,
								const CoordFrame& camera_frame, float yaw, float pitch)
{
	cam_right = cam_right_vec;
	cam_down = cam_upvec * -1;
	cam_top_left = cam_forwards_vec - (cam_down*0.5) - (cam_right*0.5);


	glClearColor (0.0, 0.0, 0.0, 0.0);

	if(!fill_tris)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	//-----------------------------------------------------------------
	//reset projection matrix
	//-----------------------------------------------------------------
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/*C SPECIFICATION
	  void glOrtho(	GLdouble left,
			GLdouble right,
			GLdouble bottom,
			GLdouble top,
			GLdouble zNear,
			GLdouble zFar )


     PARAMETERS
	  left,	right Specify the coordinates for the left and right
		      vertical clipping	planes.

	  bottom, top Specify the coordinates for the bottom and top
		      horizontal clipping planes.

	  zNear, zFar Specify the distances to the nearer and farther
		      depth clipping planes.  These values are
		      negative if the plane is to be behind the
		      viewer.
			 */

	//-----------------------------------------------------------------
	//set an orthographic projection
	//-----------------------------------------------------------------
	glOrtho(0, 1, 1, 0, 0.1, 100);


	//-----------------------------------------------------------------
	//init modelview matrix
	//-----------------------------------------------------------------
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();



	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_COLOR_MATERIAL);//this means u can change the colour of a quad with glColor
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);



	glColor4f(1,1,1,1);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);//just add to the fragment colour



	RayT::Colour white(1,1,1);
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, white.toFloatArray());

	//-----------------------------------------------------------------
	//set tex mode
	//-----------------------------------------------------------------
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


	//-----------------------------------------------------------------
	//set poly fill or line mode
	//-----------------------------------------------------------------
	if(fill_tris)
	{
		glPolygonMode(GL_FRONT, GL_FILL);
		glPolygonMode(GL_BACK, GL_FILL);
	}
	else
	{
		glPolygonMode(GL_FRONT, GL_LINE);
		glPolygonMode(GL_BACK, GL_LINE);
	}

	//glPolygonMode(GL_DROUN, GL_LINE);

	glNormal3f(0, 0, 1);



	numquadsdrawn = 0;




	//Timer timer;





	//-----------------------------------------------------------------
	//set ray info for this frame
	//-----------------------------------------------------------------
	ray.startpos.set(0,0,0);
//	ray.intensity_fraction = 1.0;
	ray.primary_ray = true;



	//CoordFrame object_transform(camera_frame);
	//object_transform.invert();

	//CoordFrame object_transform(camerapos);


	/*CoordFrame c1(Vec3(0,0,0));
		c1.getBasis().rotAroundAxis(Vec3(-1,0,0), -pitch);

	CoordFrame c2(Vec3(0,0,0));
		c2.getBasis().rotAroundAxis(Vec3(-1,0,0), -pitch);

	CoordFrame c3(camerapos * -1);

	CoordFrame object_transform = c3 * c1 * c2;*/
	//object_transform.getBasis().rotAroundAxis(Vec3(-1,0,0), -pitch);
	//object_transform.getBasis().rotAroundAxis(Vec3(0,0,1), -yaw);
	


	//TEMP NO CELL RESETTING
	//cellmanager.resetCells();

	world->prepareForFrame(time, camera_frame, Vec3(0,0,0), cam_right_vec);

	//world->resetPrimaryObjectsUse();





	//-----------------------------------------------------------------
	//
	//-----------------------------------------------------------------
	/*for(std::vector<ScreenPoint>::iterator i = screenpoints.begin(); i != screenpoints.end(); ++i)
	{
		(*i).tracedthisframe = false;
	}*/

	/*for(int i=0; i<screenpoints.size(); i++)
	{
		screenpoints[i].tracedthisframe = false;
	}*/



//	const int OFFSET_TO_MIDDLE = SUBD_SCREENPOINT_RES * POINTS_PER_UNSUBD_POINT
//												+ (POINTS_PER_UNSUBD_POINT / 2);


	{
	/*for(int y=0; y<UNSUBD_SCREENPOINT_RES; y++)
	{
		int index = getIndexForUnsubdividedScreenPoint(0, y);

		const int endindex = getIndexForUnsubdividedScreenPoint(UNSUBD_SCREENPOINT_RES, y);

		while(index != endindex)
		{
			screenpoints[ index ].reset(*world);


			index += POINTS_PER_UNSUBD_POINT;
		}


	
	}*/


	}



	last_tex_used_id = -1;

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	tristrip_started = false;
	drawing = false;
	texturing = false;

	const int SCREENPOINT_RES_MINUS_ONE = UNSUBD_SCREENPOINT_RES-1;
	const int SCREENPOINT_RES_MINUS_TWO = UNSUBD_SCREENPOINT_RES-2;


	for(int y=0; y<SCREENPOINT_RES_MINUS_ONE; y++)
	{
		int topleft_index = getIndexForUnsubdividedScreenPoint(0, y);
		const int end_topleft_index = getIndexForUnsubdividedScreenPoint(SCREENPOINT_RES_MINUS_ONE, y);

		//for(int x=0; x<SCREENPOINT_RES_MINUS_TWO; x++)
		//{
		//int x=0;
		while(topleft_index != end_topleft_index)
		{

			const int topright = topleft_index + POINTS_PER_UNSUBD_POINT;
			const int botleft = topleft_index + UNSUBD_Y_STEP;
			const int botright = botleft + POINTS_PER_UNSUBD_POINT;

			/*screenpoints[topleft_index].reset(*world);
			screenpoints[topright].reset(*world);
			screenpoints[botleft].reset(*world);
			screenpoints[botright].reset(*world);*/

			/*const int topleft = x + SCREENPOINT_RES*y;
			const int topright = topleft + 1;
			const int botleft = topleft + SCREENPOINT_RES;
			const int botright = botleft + 1;*/

			//world->primary_objects_use = world->objects;
			world->resetPrimaryObjectsUse();

			world->resetMinSubdivDepth();
		
			

			screenpoints[topleft_index].reset(*world);
			screenpoints[topright].reset(*world);
			screenpoints[botleft].reset(*world);
			screenpoints[botright].reset(*world);
			
			
			/*const int cellindex = y*SCREENPOINT_RES_MINUS_ONE + x;
			

			if(cellmanager.getCell(cellindex).getLinePlanes().size() != 0)
			{
				//-----------------------------------------------------------------
				//clip the cell up into triangles, then render tri style
				//-----------------------------------------------------------------

				//-----------------------------------------------------------------
				//terminate tri strip
				//-----------------------------------------------------------------
				testTerminateTriStrip(screenpoints[topleft_index], screenpoints[botleft]);

				glBegin(GL_TRIANGLES);

				divideAndRenderTri(screenpoints[topleft_index], screenpoints[botleft], screenpoints[topright], 
											cellmanager.getCell(cellindex).getLinePlanes(), 0);

				divideAndRenderTri(screenpoints[botright], screenpoints[topright], screenpoints[botleft], 
											cellmanager.getCell(cellindex).getLinePlanes(), 0);

				glEnd();

			//if(cellmanager.getCell(cellindex).isOneTouchingLine())
			//{
			//	cellmanager.getCell(cellindex).clipLineToCell();
//
//				this->renderLineCell(screenpoints[topleft_index], screenpoints[topright],
//								screenpoints[botleft], screenpoints[botright],
//								cellmanager.getCell(cellindex));
//			}
			}
			else*/
			{

				//-----------------------------------------------------------------
				//render quad style
				//-----------------------------------------------------------------
				this->renderSquare(screenpoints[topleft_index], screenpoints[topright],
									screenpoints[botleft], screenpoints[botright], 0/*, false*/);
			}

			//-----------------------------------------------------------------
			//terminate tri strip cause at end of row
			//-----------------------------------------------------------------
			//testTerminateTriStrip(screenpoints[topright], screenpoints[botright]);


			topleft_index += POINTS_PER_UNSUBD_POINT;
			//x++;
		}
		
	}

	assert(!drawing);

	glEnd();

	//-----------------------------------------------------------------
	//render light flares
	//-----------------------------------------------------------------
	world->drawFlares(cam_forwards_vec, cam_down, cam_right, camerapos);



	//double time_elapsed = timer.getSecondsElapsed();

	//double seconds_per_pixel = time_elapsed / ((double)windowwidth * (double)windowheight);

	//debugPrint("pixels per ms: %f\n", 1 / (seconds_per_pixel * 1000));

	debugPrint("num quads drawn: %i\n", numquadsdrawn);

	debugPrint("max subdivision depth: %i\n", MAX_SUBDIVISION_DEPTH);

	debugPrint("num primary rays cast: %i\n", world->num_primary_rays_cast);
	debugPrint("num secondary rays cast: %i\n", world->num_secondary_rays_cast);

}







inline bool isObjectInVector(const std::vector<Object*>& vec, Object* ob)
{
	for(int i=0; i<vec.size(); ++i)
	{
		if(ob == vec[i])
			return true;
	}

	return false;
}


const float Z = -1;



inline void renderVertWithTexLighting(const ScreenPoint& spoint)
{
	glTexCoord2fv(spoint.tex_coords.data());
	glColor3fv(spoint.texlighting.data());
	glVertex3f(spoint.screenspace_pos.x, spoint.screenspace_pos.y, Z);
}

inline void renderVertWithAddLighting(const ScreenPoint& spoint)
{
	glColor3fv(spoint.addlighting.data());
	glVertex3f(spoint.screenspace_pos.x, spoint.screenspace_pos.y, Z);
}

/*inline void renderNoTexBothLighting(const ScreenPoint& spoint)
{
	glColor3f(	spoint.addlighting.r + spoint.texlighting.r,
					spoint.addlighting.g + spoint.texlighting.g,
					spoint.addlighting.b + spoint.texlighting.b);
		
	glVertex3f(spoint.screenspace_pos.x, spoint.screenspace_pos.y, Z);
}*/

void RayTracer::renderSquare(	const ScreenPoint& topleft, const ScreenPoint& topright,
								const ScreenPoint& botleft, const ScreenPoint& botright,
								int currentdepth/*, bool last_square_in_row*/)
{


	if((currentdepth >= MAX_SUBDIVISION_DEPTH || (topleft.id == topright.id && 
													topleft.id == botleft.id && 
													topleft.id == botright.id))
									&& currentdepth >= world->getMinSubdivDepth())
	{
		//-----------------------------------------------------------------
		//all ids are the same or max depth reached, so draw quad
		//-----------------------------------------------------------------
		numquadsdrawn++;



		//TEMP 
		//glDisable(GL_TEXTURE_2D);

		if(topleft.final_object_hit)
		{
			
			//TEMP
			//glEnable(GL_TEXTURE_2D);

		

				/*if(tristrip_started)
				{
					glEnd();
					assert(drawing);
					drawing = false;
					tristrip_started = false;//NOTE: can this ever be reached?
				}*/
				


				if(topleft.final_object_hit->getMaterial().use_diffuse_texture)
				{
					
					//-----------------------------------------------------------------
					//
					//-----------------------------------------------------------------
					if(!texturing)
					{
						glEnd();	//GL_QUADS

						glEnable(GL_TEXTURE_2D);
						texturing = true;

						glBegin(GL_QUADS);
					}

					//-----------------------------------------------------------------
					//check for texture change
					//-----------------------------------------------------------------
					if(topleft.final_object_hit->getMaterial().getDiffuseTextureId() != last_tex_used_id)
					{
						glEnd();	//GL_QUADS

						glBindTexture(GL_TEXTURE_2D, topleft.final_object_hit->getMaterial().getDiffuseTextureId());
						last_tex_used_id = topleft.final_object_hit->getMaterial().getDiffuseTextureId();
					
						glBegin(GL_QUADS);
					}

				}
				else
				{
					if(texturing)
					{
						glEnd();	//GL_QUADS

						glDisable(GL_TEXTURE_2D);
						texturing = false;

						glBegin(GL_QUADS);
					}

						//last_tex_used_id = -1;
				}




				
			//}
		}
		else
		{
			//else if ray hit the void

			//-----------------------------------------------------------------
			//disable texturing if it is going
			//-----------------------------------------------------------------
			if(texturing)
			{
				glEnd();	//GL_QUADS

				glDisable(GL_TEXTURE_2D);
				texturing = false;

				glBegin(GL_QUADS);
			}
		}

	









		//-----------------------------------------------------------------
		//this square is consistent, so render it with a quad
		//-----------------------------------------------------------------
		//glBegin(GL_QUADS);
		//glBegin(GL_TRIANGLE_STRIP);

		

		/*if(currentdepth == 0)
		{
			if(!tristrip_started)
			{
				assert(!drawing);
				glBegin(GL_TRIANGLE_STRIP);
				drawing = true;
				tristrip_started = true;
			}

			//-----------------------------------------------------------------
			//draw 2 verts to continue the triangle strip
			//-----------------------------------------------------------------
			renderVertWithTexLighting(topleft);
			renderVertWithTexLighting(botleft);

		//	if(last_square_in_row)
		//	{
		//		assert(drawing);
		//		assert(tristrip_started);
//
//				renderScreenPointVert(topright);
//				renderScreenPointVert(botright);
//				
//				glEnd();//end tristrip for this row
///				tristrip_started = false;
	//			drawing = false;
	//		}

		}
		else
		{*/
			//assert(!drawing);
			//glBegin(GL_TRIANGLE_STRIP);


		if(texturing)
		{
			//-----------------------------------------------------------------
			//render textured quad
			//-----------------------------------------------------------------
			renderVertWithTexLighting(botleft);
			renderVertWithTexLighting(topleft);		
			renderVertWithTexLighting(topright);
			renderVertWithTexLighting(botright);
		

			//glEnd();


			if(topleft.addlighting.r + topleft.addlighting.g + topleft.addlighting.b >= 0.001)
			{	//if noticeable additive lighting

				glEnd();//GL_QUADS

				//-----------------------------------------------------------------
				//now render tri with the additive lighting over the top of it
				//-----------------------------------------------------------------
				//if(texturing)
					glDisable(GL_TEXTURE_2D);

				glEnable(GL_BLEND);

				glBegin(GL_QUADS);
				
					renderVertWithAddLighting(botleft);
					renderVertWithAddLighting(topleft);
					renderVertWithAddLighting(topright);
					renderVertWithAddLighting(botright);
					
				glEnd();

				glDisable(GL_BLEND);

				//if(last_tex_used_id != -1)//if currently drawing textured object

				//if(texturing)
					glEnable(GL_TEXTURE_2D);

				glBegin(GL_QUADS);
			}
		}
		else
		{
			//if not texturing..
			
			renderVertWithAddLighting(botleft);
			renderVertWithAddLighting(topleft);		
			renderVertWithAddLighting(topright);
			renderVertWithAddLighting(botright);
		}
			
			
	//	}

	}
	else
	{
		//-----------------------------------------------------------------
		//all ids are not the same, so subdivide
		//-----------------------------------------------------------------

		//if(currentdepth == 0)
		//{

			//-----------------------------------------------------------------
			//constrain list of objects to test primary rays against
			//-----------------------------------------------------------------
			/*world->primary_objects_use_size = 0;
			world->first_nonobstructor_index_use = 0;


			//id is not the same at all corners - so subdivide
			if(topleft.first_object_hit)
				addObjectToPrimUseVec(topleft.first_object_hit);

			testAddObToPrimUseVec(topright.first_object_hit);

			testAddObToPrimUseVec(botleft.first_object_hit);

			testAddObToPrimUseVec(botright.first_object_hit);*/
		//}


		/*if(tristrip_started)
		{
			assert(drawing);
			assert(currentdepth == 0);
			//-----------------------------------------------------------------
			//first terminate current horizontal tri strip by drawing 2 capping verts
			//-----------------------------------------------------------------
			renderVertWithTexLighting(topleft);
			renderVertWithTexLighting(botleft);

			glEnd();//end the triangle strip
			drawing = false;
			tristrip_started = false;
		}*/
	

	

		if(currentdepth + 1 <= PRECOMPUTE_SUBD_DEPTH)
		{
			//if screenpoints for next depth level are precomputed..

			const int X_HALFSTEP = 1 << (PRECOMPUTE_SUBD_DEPTH - currentdepth - 1);
			const int Y_HALFSTEP = X_HALFSTEP * SUBD_SCREENPOINT_RES;

			ScreenPoint& topmiddle = screenpoints[topleft.getIndex() + X_HALFSTEP];
			ScreenPoint& rightmiddle = screenpoints[topright.getIndex() + Y_HALFSTEP];
			ScreenPoint& bottommiddle = screenpoints[botleft.getIndex() + X_HALFSTEP];
			ScreenPoint& leftmiddle = screenpoints[topleft.getIndex() + Y_HALFSTEP];
			ScreenPoint& middle = screenpoints[topleft.getIndex() + X_HALFSTEP + Y_HALFSTEP];
			


			//-----------------------------------------------------------------
			//trace the rays
			//-----------------------------------------------------------------
			middle.reset(*world);
			topmiddle.reset(*world);
			rightmiddle.reset(*world);
			bottommiddle.reset(*world);
			leftmiddle.reset(*world);

			//-----------------------------------------------------------------
			//add objects hit to vector of objects to test against
			//-----------------------------------------------------------------
			if(currentdepth == 0)
			{
				//-----------------------------------------------------------------
				//constrain list of objects to test primary rays against
				//-----------------------------------------------------------------
				
					//try to see if they are just all the same
				if(topleft.first_object_hit && 
					(topleft.first_object_hit == topright.first_object_hit) &&
					(topleft.first_object_hit == botleft.first_object_hit) &&
					(topleft.first_object_hit == botright.first_object_hit))
				{
					world->primary_objects_use[0] = topleft.first_object_hit;

					world->primary_objects_use_size = 1;
					//world->first_nonobstructor_index_use = 0;
					world->are_prim_objects_use_reset = false;//set disturbed flag

				}
				else
				{
					world->primary_objects_use_size = 0;
					//world->first_nonobstructor_index_use = 0;
					world->are_prim_objects_use_reset = false;//set disturbed flag

					if(topleft.first_object_hit)
						addObjectToPrimUseVec(topleft.first_object_hit);
					testAddObToPrimUseVec(topright.first_object_hit);
					testAddObToPrimUseVec(botleft.first_object_hit);
					testAddObToPrimUseVec(botright.first_object_hit);

					testAddObToPrimUseVec(middle.first_object_hit);
					testAddObToPrimUseVec(topmiddle.first_object_hit);
					testAddObToPrimUseVec(rightmiddle.first_object_hit);
					testAddObToPrimUseVec(bottommiddle.first_object_hit);
					testAddObToPrimUseVec(leftmiddle.first_object_hit);
				}

			
			}

			




			//-----------------------------------------------------------------
			//process subudividees
			//-----------------------------------------------------------------
			const int newdepth = currentdepth + 1;

			renderSquare(topleft, topmiddle, leftmiddle, middle, newdepth/*, false*/);//top left
			renderSquare(topmiddle, topright, middle, rightmiddle, newdepth/*, false*/);//top right
			renderSquare(leftmiddle, middle, botleft, bottommiddle, newdepth/*, false*/);//bot left
			renderSquare(middle, rightmiddle, bottommiddle, botright, newdepth/*, false*/);//bot right
		}
		else
		{
			const float HALF_SCREENSPACE_STEP = 
				(topright.screenspace_pos.x - topleft.screenspace_pos.x) * 0.5f;
			
	
			Vec2 screenspacepos(topleft.screenspace_pos.x + HALF_SCREENSPACE_STEP, topleft.screenspace_pos.y);
			
			getRayUnitDirForScreenspacePos(screenspacepos, this->ray.unitdir);

			const ScreenPoint topmiddle(*world, screenspacepos/*top_mid_screenspacepos*/, ray);
			


			screenspacepos.set(topright.screenspace_pos.x, topright.screenspace_pos.y + HALF_SCREENSPACE_STEP);
	
			getRayUnitDirForScreenspacePos(screenspacepos, this->ray.unitdir);

			const ScreenPoint rightmiddle(*world, screenspacepos, ray);//do it



			screenspacepos.set(botleft.screenspace_pos.x + HALF_SCREENSPACE_STEP, botleft.screenspace_pos.y);

			getRayUnitDirForScreenspacePos(screenspacepos, this->ray.unitdir);

			const ScreenPoint bottommiddle(*world, screenspacepos/*bot_mid_screenspacepos*/, ray);//do it



			screenspacepos.set(topleft.screenspace_pos.x, topleft.screenspace_pos.y + HALF_SCREENSPACE_STEP);

			getRayUnitDirForScreenspacePos(screenspacepos, this->ray.unitdir);

			const ScreenPoint leftmiddle(*world, screenspacepos, ray);//do it


			screenspacepos.set(topleft.screenspace_pos.x + HALF_SCREENSPACE_STEP, topleft.screenspace_pos.y + HALF_SCREENSPACE_STEP);

			getRayUnitDirForScreenspacePos(screenspacepos, this->ray.unitdir);

			const ScreenPoint middle(*world, screenspacepos, ray);//do it





			//-----------------------------------------------------------------
			//add middle object hit to vector of objects to test against
			//-----------------------------------------------------------------
			/*if(currentdepth == 0)
				if(middle.first_object_hit && 
					!isObjectInPrimUseVec(middle.first_object_hit))
					addObjectToPrimUseVec(middle.first_object_hit);
			TEMP DODGY*/


			const int newdepth = currentdepth + 1;

			renderSquare(topleft, topmiddle, leftmiddle, middle, newdepth/*, false*/);//top left
			renderSquare(topmiddle, topright, middle, rightmiddle, newdepth/*, false*/);//top right
			renderSquare(leftmiddle, middle, botleft, bottommiddle, newdepth/*, false*/);//bot left
			renderSquare(middle, rightmiddle, bottommiddle, botright, newdepth/*, false*/);//bot right
		}
	}
}











void RayTracer::testTerminateTriStrip(const ScreenPoint& top, const ScreenPoint& bottom)
{
	if(tristrip_started)
	{
		assert(drawing);
//		assert(currentdepth == 0);
		//-----------------------------------------------------------------
		//first terminate current horizontal tri strip by drawing 2 capping verts
		//-----------------------------------------------------------------
		renderVertWithTexLighting(top);
		renderVertWithTexLighting(bottom);

		glEnd();//end the triangle strip
		drawing = false;
		tristrip_started = false;
	}
}





