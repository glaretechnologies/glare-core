
#include "c:\programming\directxlibs\dxinput.h"





#include "C:\programming\simple3d\s3dframework.h"
#include "C:\programming\simple3d\simple3d.h"
#include "C:\programming\simple3d\sphere.h"
#include "js_physics.h"
#include "js_ball.h"
#include "js_meshobject.h"
#include "js_trimeshshape.h"
#include "js_particle.h"

#include "studiomesh.h"
#include "C:\programming\simple3d\trianglemesh.h"
#include "C:\programming\simple3d\ray.h"
#include "C:\programming\simple3d\point.h"


Simple3d* simple3d = 0;
js::Physics* physics = 0; 
js::Ball* ball = 0;
js::MeshObject* hovercraft = 0;
TriangleMesh* gr_hover = 0;
Sphere* gr_sphere = 0;
Ray* gr_ray = 0;
js::Particle* particle = 0;


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

	physics = new js::Physics();

	//-----------------------------------------------------------------
	//add a sphere to the physics engine
	//-----------------------------------------------------------------
	ball = new js::Ball(1.0f, Vec3(0,0,0), 10);
	physics->insertObject(ball);

	//-----------------------------------------------------------------
	//add a sphere to the graphics engine
	//-----------------------------------------------------------------
	gr_sphere = new Sphere(Vec3(0,0,0), 1.0f);
	simple3d->insertObject(gr_sphere);



	//-----------------------------------------------------------------
	//add a !!!!hovercraft!!! to the graphics engine
	//-----------------------------------------------------------------
	StudioMesh studio_mesh("hov0.3DS");

	std::vector<Triangle> meshtris;
	studio_mesh.buildGraphicsTriList(meshtris);

	/*for(std::vector<Triangle>::iterator i = meshtris.begin(); i != meshtris.end(); i++)
	{
		Triangle* tri = new Triangle(*i);

		simple3d->insertObject(tri);
	}*/

	gr_hover = new TriangleMesh(CoordFrame(Vec3(0,0,0)), meshtris);

	simple3d->insertObject( gr_hover );


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
	js::SimpleTriMeshShape* hover_shape = new js::SimpleTriMeshShape(hovermesh);

	//-----------------------------------------------------------------
	//create the object
	//-----------------------------------------------------------------
	hovercraft = new js::MeshObject(CoordFrame(Vec3(0,0,0)), hover_shape, true, 100.0);
	physics->insertObject(hovercraft);


	//-----------------------------------------------------------------
	//insert a ray into the graphics engine
	//-----------------------------------------------------------------
	gr_ray = new Ray(Vec3(0,0,0), Vec3(10, 10, 10));
	simple3d->insertObject(gr_ray);



	//-----------------------------------------------------------------
	//add an unmoveable sphere to the physics engine
	//-----------------------------------------------------------------
	//physics->insertObject(new js::Ball(5.0f, Vec3(0,-20,0), 100, false));

	//simple3d->insertObject(new Sphere(Vec3(0,-20,0), 5.0f));


	//-----------------------------------------------------------------
	//insert particle
	//-----------------------------------------------------------------
	//	Particle(const Vec3& startpos, float mass, const Vec3& startvel, float drag_coeff);

	particle = new js::Particle(Vec3(2,2,2), 0.001, Vec3(0,0,0), 0.3);
	physics->insertObject(particle);

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
	physics->tick(0.003f);

	//-----------------------------------------------------------------
	//get sphere pos
	//-----------------------------------------------------------------
	Vec3 spherepos = ball->getPos();

	//-----------------------------------------------------------------
	//position the sphere correspondingly in the graphics engine
	//-----------------------------------------------------------------
	simple3d->removeObject(gr_sphere);
	gr_sphere->pos = spherepos;
	simple3d->insertObject(gr_sphere);


	//-----------------------------------------------------------------
	//reposition the hovercraft
	//-----------------------------------------------------------------
	gr_hover->setPos(hovercraft->getPos());
	//gr_hover->getMatrix().getBasis().rotAroundAxis(Vec3(0,1,0), 0.01);


	//-----------------------------------------------------------------
	//trace a ray forwards from the a bit below the camera
	//-----------------------------------------------------------------
	Vec3 forwards = simple3d->getCamera().getFacingVec();
	Vec3 raystart = simple3d->getCamera().getPos() - Vec3(0, 2, 0);

	Vec3 rayend = raystart + forwards * 100;

	float fraction;
	Vec3 normal;
	bool hit = hovercraft->lineStabsObject(fraction, normal, raystart, rayend);

	if(!hit)
		fraction = 1.0;

	rayend = raystart + forwards * (100 * fraction);

	gr_ray->set(raystart, rayend);


	simple3d->insertTempObject(new Point(rayend));


	//-----------------------------------------------------------------
	//draw the particle
	//-----------------------------------------------------------------
	simple3d->insertTempObject(new Point(particle->getPos()));



	//-----------------------------------------------------------------
	//draw the scene
	//-----------------------------------------------------------------
	simple3d->drawScene(ww, wh);




}

void S3dFramework_Shutdown()
{
	delete input;

	delete ball;
	delete gr_sphere;

	delete physics;
	delete simple3d;

}
