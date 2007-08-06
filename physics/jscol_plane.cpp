#include "jscol_plane.h"


#include "jscol_boundingsphere.h"
//#include "jscol_ray.h"
#include "../simpleraytracer/ray.h"



js::Plane::Plane(const ::Plane& plane_)
:	plane(plane_)
{	
	const float bigval = 1e9f;
	const float halfwidth = 0.1f;

	if(plane.getNormal() == Vec3(0,0,1))
	{
		bbox_os.enlargeToHold(Vec3(-bigval,-bigval,-halfwidth));
		bbox_os.enlargeToHold(Vec3(-bigval,bigval,-halfwidth));
		bbox_os.enlargeToHold(Vec3(bigval,-bigval,-halfwidth));
		bbox_os.enlargeToHold(Vec3(bigval,bigval,-halfwidth));

		bbox_os.enlargeToHold(Vec3(-bigval,-bigval,halfwidth));
		bbox_os.enlargeToHold(Vec3(-bigval,bigval,halfwidth));
		bbox_os.enlargeToHold(Vec3(bigval,-bigval,halfwidth));
		bbox_os.enlargeToHold(Vec3(bigval,bigval,halfwidth));
	}
	else
		bbox_os.setToLargeBox();
}

float js::Plane::getRayDist(const ::Ray& ray)
{
	return plane.rayIntersect(ray.startpos, ray.unitdir);
}

bool js::Plane::lineStabsShape(float& t_out, Vec3& normal_os_out, const Vec3& raystart_os, 
										 const Vec3& rayend_os)
{
	/*const Vec3 starttoplane = point - raystart_os;
	const Vec3 planetoend = rayend_os - point;

	float starttoplane_on_normal = dotProduct(starttoplane, normal);

	float planetoend_on_normal =  dotProduct(planetoend, normal);



	if(starttoplane_on_normal + planetoend_on_normal == 0)
		return false;
	//NOTE: make this good*/

	//	//returns fraction of ray travelled. Will be in range [0, 1] if ray hit
	//inline float finiteRayIntersect(const Vec3& raystart, const Vec3& rayend) const;
	const float tracefraction = plane.finiteRayIntersect(raystart_os, rayend_os);


	//fraction [0,1] ray got along path
	//float tracefraction = starttoplane_on_normal / (starttoplane_on_normal + planetoend_on_normal);

	//-----------------------------------------------------------------
	//return false if ray does not intersect triangle plane
	//-----------------------------------------------------------------
	if(tracefraction < 0 || tracefraction >= 1)
		return false;

	//-----------------------------------------------------------------
	//else fill out return info and return true
	//-----------------------------------------------------------------
	t_out = tracefraction;
	normal_os_out = plane.getNormal();
	return true;
}



	
//returns true if sphere hit object.
bool js::Plane::traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_os_out)//, bool& embedded_out,
										//Vec3& disembed_vec_out)
{
	//embedded_out = false;

	const float plane_to_center_dist = plane.signedDistToPoint(sphere.getCenter());
	if(fabs(plane_to_center_dist) < sphere.getRadius())
	{
		//embedded_out = true;
		//disembed_vec_out = plane.getNormal() * (sphere.getRadius() - plane_to_center_dist);
		//return false;

		dist_out = 0;
		normal_os_out = plane.getNormal();
		return true;
	}


	if(plane_to_center_dist < 0)
	{
		//sphere is on back side of plane, so can't collide with it.
		return false;
	}

	//amount translation is heading in inverse plane normal direction
	const float normal_translation_len = -1.0f * translation.dot(plane.getNormal());

	//-----------------------------------------------------------------
	//if sphere is not embbeded in plane and is travelling away from plane..
	//-----------------------------------------------------------------
	if(normal_translation_len <= 0)//-sphere.getRadius())
		return false;

	if(normal_translation_len + sphere.getRadius() > 
		plane_to_center_dist)
	{
		//NOTE: SLOW: taking dot again
		dist_out = (plane_to_center_dist - sphere.getRadius()) / 
							-normalise(translation).dot(plane.getNormal());

		normal_os_out = plane.getNormal();
		return true;
	}
	else
		return false;

/*

	Vec3 unitdir = translation;
	const float translen = unitdir.normalise_ret_length();

	//-----------------------------------------------------------------
	//get dist along translation to plane
	//-----------------------------------------------------------------
	const float dist = plane.rayIntersect(sphere.getCenter(), normalise(translation));

	if(dist < -sphere.getRadius())
		return false;

	if(dist <= translen
	*/

}


void js::Plane::appendCollPoints(std::vector<Vec3>& points_ws, const BoundingSphere& sphere_os,
											const CoordFrame& frame)
{
	const float dist = plane.signedDistToPoint(sphere_os.getCenter());

	if(fabs(dist) > sphere_os.getRadius())
		return;

	points_ws.push_back(plane.closestPointOnPlane(frame.transformPointToParent(sphere_os.getCenter())));
}


//virtual const BoundingSphere& getBoundingSphereOS() = 0;
const AABox js::Plane::getBBoxOS()
{
	return bbox_os;
}