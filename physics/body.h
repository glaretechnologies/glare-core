#ifndef __BODY_H__
#define __BODY_H__


#include "stubglobals.h"

class Body
{
public:
	Body(js::SimpleTriMeshShape* physics_shape, TriangleMesh* gr_object_, const CoordFrame& frame)
	{
		gr_object = gr_object_;
		//-----------------------------------------------------------------
		//insert model into col det system
		//-----------------------------------------------------------------
		model = new js::SimpleModel(physics_shape, frame);
		col_sys->insertStaticModel(model);

		//-----------------------------------------------------------------
		//insert gr_object into graphics system
		//-----------------------------------------------------------------
		simple3d->insertObject(gr_object);
	}

	~Body()
	{
		col_sys->removeStaticModel(model);

		delete model;

		simple3d->removeObject(gr_object);

		delete gr_object;
	}


	void think()
	{
		gr_object->getMatrix() = model->getCoordFrame();
	}



private:
	js::Model* model;
	TriangleMesh* gr_object;

};





#endif //__BODY_H__