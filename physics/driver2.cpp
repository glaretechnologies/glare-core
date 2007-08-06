
#include "c:\programming\directxlibs\dxinput.h"

#include "jscol_raytraceresult.h"



#include "C:\programming\maths\maths.h"

#include "C:\programming\simple3d\s3dframework.h"
#include "C:\programming\simple3d\simple3d.h"
#include "C:\programming\simple3d\sphere.h"
#include "js_physics.h"
//#include "js_ball.h"
//#include "jscol_meshobject.h"
#include "jscol_simpletrimeshshape.h"
//#include "js_particle.h"

#include "studiomesh.h"
#include "C:\programming\simple3d\trianglemesh.h"
#include "C:\programming\simple3d\ray.h"
#include "C:\programming\simple3d\point.h"
#include "jscol_worldcollisionsys.h"
#include "jscol_simplemodel.h"
#include "body.h"
#include "particle.h"
#include "C:\programming\simple3d\grid.h"


Simple3d* simple3d = 0;
js::Physics* physics = 0; 
//js::Ball* ball = 0;
//js::MeshObject* hovercraft = 0;
//TriangleMesh* gr_hover = 0;
Sphere* gr_sphere = 0;
Ray* gr_ray = 0;
Grid* gr_grid = 0;
//js::Particle* particle = 0;

js::WorldCollisionSys* col_sys = 0;
js::SimpleTriMeshShape* hover_shape = 0;
//js::SimpleModel* hover_model1 = 0;
//js::SimpleModel* hover_model2 = 0;
//js::SimpleModel* hover_model3 = 0;

Body* hover1 = 0;
Body* hover2 = 0;
Body* hover3 = 0;
Particle* part1 = 0;

/*==========================================================================
class IN_EventReceiver
----------------
sunbclass this class, and overide the function eventReceiver if you want to 
receive events
==========================================================================*/
class MyEventReceiver : public IN_EventReceiver
{
public:
	bool eventReceiver( input_event* event )
	{
		if(!simple3d)
			return true;

		Camera& cam = simple3d->getCamera();

		const float MOVE_SPEED = 0.25f;
		const float TURN_SPEED = 0.003f;

		//-----------------------------------------------------------------
		//process key presses
		//-----------------------------------------------------------------
		if(event->event_type == EV_KEYPRESS || event->event_type == EV_KEYHELDDOWN)
		{
		
			if(event->key == 'a')
				cam.strafeRight(MOVE_SPEED);

			else if(event->key == 'd')
				cam.strafeRight(-MOVE_SPEED);
			//hack: changed the strafes here :)

			else if(event->key == 'w')
				//cam.strafeUp(5);
				cam.moveUp(MOVE_SPEED);

			else if(event->key == 's')
				//cam.strafeUp(-5);
				cam.moveUp(-MOVE_SPEED);
		}
		//-----------------------------------------------------------------
		//process mouse movements
		//-----------------------------------------------------------------
		else if(event->event_type == EV_MOUSEMOVED)
		{
			cam.turnClockwise(TURN_SPEED * event->d_mouse_x);
			
			cam.lookUp(TURN_SPEED * event->d_mouse_y);

			cam.rollClockwise(TURN_SPEED*0.3 * event->d_mouse_z);

			//camangles[ROLL] += 0.03 * event->d_mouse_z;
		}
		//-----------------------------------------------------------------
		//process mouse clicks
		//-----------------------------------------------------------------
		else if(event->event_type == EV_MOUSEPRESS || event->event_type == EV_MOUSEBUTTONHELDDOWN)
		{
			if(event->key == 0)
				cam.moveForwards(MOVE_SPEED);
			else if(event->key == 1)
				cam.moveForwards(-MOVE_SPEED);
			//s3d->drawString(200, 200, "mousepress key: %i\n", (int)event->key);
		}



		return true;
	}
};
MyEventReceiver event_r;

DXInput* input = NULL;






void S3dFramework_Init(HINSTANCE  hinst, HWND windowhandle)
{
	//-----------------------------------------------------------------
	//set up coord system for yaw pitch roll stuff
	//-----------------------------------------------------------------
	Vec3::setWsUp(		Vec3(0,1,0));//-y
	Vec3::setWsForwards(	Vec3(0,0,-1));//-z
	Vec3::setWsRight(		Vec3(-1,0,0));//-x
	//NOTE: fixme, make up etc. prop of camera
	
	simple3d = new Simple3d();
	simple3d->setCameraPos(Vec3(0,0,10));


	const float HASHVOXEL_WIDTH = 1.0f;
	//-----------------------------------------------------------------
	//create collision sys
	//-----------------------------------------------------------------
	col_sys = new js::WorldCollisionSys();


	physics = new js::Physics(col_sys);

	//-----------------------------------------------------------------
	//create grid graphics object
	//-----------------------------------------------------------------
	gr_grid = new Grid(HASHVOXEL_WIDTH);
	simple3d->insertObject(gr_grid);


	//-----------------------------------------------------------------
	//add a !!!!hovercraft!!! to the graphics engine
	//-----------------------------------------------------------------
	StudioMesh studio_mesh("hov0.3DS");

	std::vector<Triangle> meshtris;
	studio_mesh.buildGraphicsTriList(meshtris);

	//for(std::vector<Triangle>::iterator i = meshtris.begin(); i != meshtris.end(); i++)
	//{
	//	Triangle* tri = new Triangle(*i);
	//
	//	simple3d->insertObject(tri);
	//}

//	gr_hover = new TriangleMesh(CoordFrame(Vec3(0,0,0)), meshtris);

//	simple3d->insertObject( gr_hover );


	//-----------------------------------------------------------------
	//add a hovercraft to the physics engine
	//-----------------------------------------------------------------

	//-----------------------------------------------------------------
	//build the triangle mesh
	//-----------------------------------------------------------------
	js::TriMesh hovermesh;
	studio_mesh.buildJSTriMesh(hovermesh);

	//-----------------------------------------------------------------
	//create the collision surface
	//-----------------------------------------------------------------
	hover_shape = new js::SimpleTriMeshShape(hovermesh);




	CoordFrame hover1frame(Vec3(5, 5, 5));
	hover1 = new Body(hover_shape, new TriangleMesh(hover1frame, meshtris), hover1frame); 


	CoordFrame hover2frame(Vec3(0, 0, 0));
	hover2 = new Body(hover_shape, new TriangleMesh(hover2frame, meshtris), hover2frame);

//	Particle(const Vec3& startpos, float mass, const Vec3& startvel, float drag_coeff)

	part1 = new Particle(Vec3(3, 15, 3), 0.001, Vec3(0,1, 0), 1.0);



	//-----------------------------------------------------------------
	//insert a ray into the graphics engine
	//-----------------------------------------------------------------
	gr_ray = new Ray(Vec3(0,0,0), Vec3(10, 10, 10));
	simple3d->insertObject(gr_ray);




	//-----------------------------------------------------------------
	//set up input
	//-----------------------------------------------------------------
	input = new DXInput(hinst, windowhandle);

	input->registerEventReceiver( &event_r );

}

void S3dFramework_Main(int ww, int wh)
{
	//-----------------------------------------------------------------
	//run input
	//-----------------------------------------------------------------
	input->tick(0);

	//-----------------------------------------------------------------
	//run the physics engine
	//-----------------------------------------------------------------
	physics->advanceTime(0.01f);





	//-----------------------------------------------------------------
	//trace a ray forwards from the a bit below the camera
	//-----------------------------------------------------------------
	Vec3 forwards = simple3d->getCamera().getFacingVec();
	Vec3 raystart = simple3d->getCamera().getPos() - Vec3(0, 2, 0);

	Vec3 rayend = raystart + forwards * 100;

	js::RayTraceResult results;
	col_sys->traceRay(raystart, rayend, results);

	float fraction;

	if(results.rayhitsomething)
	{
		fraction = results.fraction;
		//-----------------------------------------------------------------
		//draw normal of the hit object
		//-----------------------------------------------------------------
		const Vec3 col_point = raystart + (rayend - raystart) * fraction;
		simple3d->insertTempObject( new Ray(col_point, col_point + results.ob_normal) );
	}
	else
	{
		fraction = 1.0;
	}

	rayend = raystart + forwards * (100 * fraction);

	gr_ray->set(raystart, rayend);


	simple3d->insertTempObject(new Point(rayend));







	//-----------------------------------------------------------------
	//draw the particle
	//-----------------------------------------------------------------
//	simple3d->insertTempObject(new Point(particle->getPos()));

	part1->think();

	//-----------------------------------------------------------------
	//draw the scene
	//-----------------------------------------------------------------
	simple3d->drawScene(ww, wh);




}

void S3dFramework_Shutdown()
{
	delete input;

//	delete hover_model1;
//	delete hover_model2;
//	delete hover_model3;
	delete hover1;
	delete hover2;
	delete hover3;
	delete part1;

//	delete ball;
//	delete gr_sphere;

	delete gr_grid;
	delete physics;
	delete hover_shape;
//	delete gr_hover;
	delete gr_ray;
	delete col_sys;
	delete simple3d;

}
