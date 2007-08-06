#include "jscol_simplemodel.h"

#include "jscol_shape.h"
#include "../simpleraytracer/ray.h"
//#include "../cyberspace/globals.h"


js::SimpleModel::SimpleModel(Shape* shape_, const CoordFrame& frame_)
{
	shape = shape_;
	frame = frame_;

	calcBBox();
}


js::SimpleModel::~SimpleModel()
{
	delete shape;
}


float js::SimpleModel::getRayDist(const ::Ray& ray)
{
	//-----------------------------------------------------------------
	//translate ray into object space
	//-----------------------------------------------------------------
	::Ray ray_os(frame.transformPointToLocal(ray.startpos),
					frame.transformVecToLocal(ray.unitdir));

	//-----------------------------------------------------------------
	//test against bounding box
	//-----------------------------------------------------------------
	if(!bbox_os.pointInBox(ray_os.startpos))
	{
		const float HACK_LENGTH = 1e7f;

		if(!bbox_os.hitByRay(ray_os.startpos, 
			ray_os.startpos + ray_os.unitdir * HACK_LENGTH, 
			ray_os.unitdir))
			return -1.0f;
	}

	return shape->getRayDist(ray_os);

}


float js::SimpleModel::traceRay(const ::Ray& ray, unsigned int& hit_tri_out, float& u_out,
							float& v_out, Vec3& normal_out)
{
	//-----------------------------------------------------------------
	//translate ray into object space
	//-----------------------------------------------------------------
	::Ray ray_os(frame.transformPointToLocal(ray.startpos),
					frame.transformVecToLocal(ray.unitdir));

	//-----------------------------------------------------------------
	//test against bounding box
	//-----------------------------------------------------------------
	if(!bbox_os.pointInBox(ray_os.startpos))
	{
		const float HACK_LENGTH = 1e7f;

		if(!bbox_os.hitByRay(ray_os.startpos, 
			ray_os.startpos + ray_os.unitdir * HACK_LENGTH, 
			ray_os.unitdir))
			return -1.0f;
	}

	assert(0);
//	return shape->traceRay(ray_os, hit_tri_out, u_out, v_out/*, normal_out*/);
	return 0.0f;
}


/*bool js::SimpleModel::lineStabsModel(float& t_out, Vec3& normal_ws_out, 
								const Vec3& raystart_ws, const Vec3& rayend_ws, const Vec3& rayunitdir, 
															float raylength)
{

	//-----------------------------------------------------------------
	//test against world space bounding sphere first
	//-----------------------------------------------------------------
	//if(!getBoundingSphereWS().lineStabsShape(raystart_ws, rayunitdir, raylength))
	//	return false;


	//-----------------------------------------------------------------
	//translate ray into object space
	//-----------------------------------------------------------------
	const Vec3 raystart_os = frame.transformPointToLocal(raystart_ws);
	const Vec3 rayend_os = frame.transformPointToLocal(rayend_ws);

	//-----------------------------------------------------------------
	//test against bounding box
	//-----------------------------------------------------------------
	if(!bbox_os.pointInBox(raystart_os))
	{

		if(!bbox_os.hitByRay(raystart_os, rayend_os, normalise(rayend_os - raystart_os)))
			return false;
	}

	//::debugPrint("hit bbox.");


	//-----------------------------------------------------------------
	//test the ray against the shape of this model
	//-----------------------------------------------------------------
	Vec3 normal_os;
	const bool hit = shape->lineStabsShape(t_out, normal_os, raystart_os, rayend_os);

	//-----------------------------------------------------------------
	//transform hit surface normal from object to world space
	//-----------------------------------------------------------------
	if(hit)
	{
		normal_ws_out = frame.transformVecToParent(normal_os);
	}

	return hit;

}*/


//returns true if sphere hit object.
bool js::SimpleModel::traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_ws_out)//, bool& embedded_out,
										//Vec3& disembedded_pos_out)
{
	//-----------------------------------------------------------------
	//calc sphere path in object space
	//-----------------------------------------------------------------
	const Vec3 startpos_os = frame.transformPointToLocal(sphere.getCenter());
	const Vec3 endpos_os = frame.transformPointToLocal(sphere.getCenter() + translation);


	//-----------------------------------------------------------------
	//calc a bounding box for the sphere path (in object space).  NOTE: slow
	//-----------------------------------------------------------------
	AABox spherepath_bbox;

	//-----------------------------------------------------------------
	//make it hold sphere at start pos
	//-----------------------------------------------------------------
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(0,0,sphere.getRadius()));
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(0,0,-sphere.getRadius()));
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(0,sphere.getRadius(), 0));
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(0,-sphere.getRadius(), 0));
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(sphere.getRadius(), 0, 0));
	spherepath_bbox.enlargeToHold(startpos_os + Vec3(-sphere.getRadius(), 0, 0));

	//-----------------------------------------------------------------
	//make it hold sphere at end pos
	//-----------------------------------------------------------------
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(0,0,sphere.getRadius()));
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(0,0,-sphere.getRadius()));
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(0,sphere.getRadius(), 0));
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(0,-sphere.getRadius(), 0));
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(sphere.getRadius(), 0, 0));
	spherepath_bbox.enlargeToHold(endpos_os + Vec3(-sphere.getRadius(), 0, 0));


	//-----------------------------------------------------------------
	//see if sphere-path bbox and object bbox overlap
	//-----------------------------------------------------------------
	if(!bbox_os.doesOverlap(spherepath_bbox))
		return false;


	//::debugPrint("sphere path hit bounding box!");


	//-----------------------------------------------------------------
	//now test against the shape.  (everything is now in object space)
	//-----------------------------------------------------------------
	Vec3 normal_os;
	//Vec3 disembed_vec;
	const bool hit = shape->traceSphere(BoundingSphere(startpos_os, sphere.getRadius()), frame.transformVecToLocal(translation), 
											dist_out, normal_os);//, embedded_out, disembedded_pos_out);

//	if(embedded_out)
//		disembedded_pos_out = frame.transformVecToParent(disembedded_pos_out);


	//-----------------------------------------------------------------
	//transform normal from object to world space
	//-----------------------------------------------------------------
	if(hit)
	{
		normal_ws_out = frame.transformVecToParent(normal_os);
	}

	return hit;
}



void js::SimpleModel::appendCollPoints(std::vector<Vec3>& points, const BoundingSphere& sphere)
{
	//-----------------------------------------------------------------
	//convert sphere to object space
	//-----------------------------------------------------------------
	const Vec3 startpos_os = frame.transformPointToLocal(sphere.getCenter());

	//-----------------------------------------------------------------
	//calc a bounding box for the sphere
	//-----------------------------------------------------------------
	AABox sphere_bbox;

	//-----------------------------------------------------------------
	//make it hold sphere at start pos
	//-----------------------------------------------------------------
	sphere_bbox.enlargeToHold(startpos_os + Vec3(0,0,sphere.getRadius()));
	sphere_bbox.enlargeToHold(startpos_os + Vec3(0,0,-sphere.getRadius()));
	sphere_bbox.enlargeToHold(startpos_os + Vec3(0,sphere.getRadius(), 0));
	sphere_bbox.enlargeToHold(startpos_os + Vec3(0,-sphere.getRadius(), 0));
	sphere_bbox.enlargeToHold(startpos_os + Vec3(sphere.getRadius(), 0, 0));
	sphere_bbox.enlargeToHold(startpos_os + Vec3(-sphere.getRadius(), 0, 0));

	//-----------------------------------------------------------------
	//see if sphere bbox and object bbox overlap
	//-----------------------------------------------------------------
	if(!bbox_os.doesOverlap(sphere_bbox))
		return;

	shape->appendCollPoints(points, BoundingSphere(startpos_os, sphere.getRadius()), frame);
}


/*const js::BoundingSphere& js::SimpleModel::getBoundingSphereWS()
{

	return boundingsphere;
}*/


bool js::SimpleModel::checkCollisionWith(const Model& dyn_model, const CoordFrame& ini_transformation, 
									const Vec3& final_translation, MMColResults& results_out)
{


	return true;
}


void js::SimpleModel::calcBBox()
{
	bbox_os = shape->getBBoxOS();

	//-----------------------------------------------------------------
	//get the shapes bounding sphere
	//-----------------------------------------------------------------
	//boundingsphere = shape->getBoundingSphereOS();
	
	//-----------------------------------------------------------------
	//transform it's center into ws
	//-----------------------------------------------------------------
	//boundingsphere.setCenter(boundingsphere.getCenter() + frame.getOrigin());
	//NOTE: dodgy
	//boundingsphere.setCenter(frame.transformPointToParent(boundingsphere.getCenter()));
}




void js::SimpleModel::setCoordFrame(const CoordFrame& newframe)
{ 
	frame = newframe;

	//-----------------------------------------------------------------
	//reca
	//-----------------------------------------------------------------
	//calcBBox();
}
