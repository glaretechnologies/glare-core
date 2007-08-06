
#include "jscol_raytraceresult.h"



#include "C:\programming\maths\maths.h"

#include "jscol_simpletrimeshshape.h"

#include "studiomesh.h"
#include "jscol_worldcollisionsys.h"
#include "jscol_simplemodel.h"
#include <iostream>
#include <sstream>

#include "../utils/timer.h"
using namespace std;

//js::WorldCollisionSys* col_sys = 0;
js::SimpleTriMeshShape* hover_shape = 0;



void S3dFramework_Init(HINSTANCE  hinst, HWND windowhandle)
{

	//-----------------------------------------------------------------
	//create collision sys
	//-----------------------------------------------------------------
	//col_sys = new js::WorldCollisionSys();
	js::WorldCollisionSys::createInstance(new js::WorldCollisionSys());



	//-----------------------------------------------------------------
	//add a !!!!hovercraft!!! to the graphics engine
	//-----------------------------------------------------------------
	StudioMesh studio_mesh("hov0.3DS");

	//-----------------------------------------------------------------
	//build the triangle mesh
	//-----------------------------------------------------------------
	js::TriMesh hovermesh;
	studio_mesh.buildJSTriMesh(hovermesh);

	//-----------------------------------------------------------------
	//create the collision surface
	//-----------------------------------------------------------------
	hover_shape = new js::SimpleTriMeshShape(hovermesh);

	js::WorldCollisionSys::getInstance().insertStaticModel(
		new js::SimpleModel(hover_shape, CoordFrame(Vec3(0,0,0))));

}

void S3dFramework_Main(int ww, int wh)
{

	int NUM_ITERATIONS = 1000;

	Timer timer;

	for(int i=0; i<NUM_ITERATIONS; ++i)
	{
		Vec3 spherepos(0,0,-5);
		const js::BoundingSphere playersphere(spherepos, 1);


		const Vec3 dpos(0,0,10);

		//-----------------------------------------------------------------
		//trace sphere through world
		//-----------------------------------------------------------------
		js::RayTraceResult traceresults;
			//js::WorldCollisionSys::getInstance().traceRay(campos_out.toVec3(), newpos_vec3, 
			//									traceresults);

		js::WorldCollisionSys::getInstance().traceSphere(playersphere, dpos, traceresults);
																		//embedded, disembed_vec); 

	}

	const float avtime = timer.getSecondsElapsed() / (float)NUM_ITERATIONS;

	stringstream stream;
	stream << "num iterations: " << NUM_ITERATIONS << endl;
	stream << "av time/iteration: " << avtime*1000.0 << " msecs" << endl;

	::MessageBox(NULL, stream.str().c_str(), "results", MB_OK);

	::PostQuitMessage(0);
}


void S3dFramework_Shutdown()
{

}
