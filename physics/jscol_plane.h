#ifndef __JSCOL_PLANE_H__
#define __JSCOL_PLANE_H__

#include "jscol_shape.h"
#include "../maths/plane.h"
#include "../simple3d/aabox.h"


namespace js
{


class Plane : public Shape
{ 
public:
	Plane(const ::Plane& plane);
	virtual ~Plane(){}

	virtual float getRayDist(const Ray& ray);

	virtual bool lineStabsShape(float& t_out, Vec3& normal_os_out, const Vec3& raystart_os, const Vec3& rayend_os);

	
	//returns true if sphere hit object.
	virtual bool traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_os_out);//, bool& embedded_out,
										//Vec3& disembed_vec_out);

	virtual void appendCollPoints(std::vector<Vec3>& points_ws, const BoundingSphere& sphere_os,
											const CoordFrame& frame);


	//virtual const BoundingSphere& getBoundingSphereOS() = 0;
	virtual const AABox getBBoxOS();

private:
	//Vec3 point;
	//Vec3 normal;
	::Plane plane;
	AABox bbox_os;

};



}//end namespace js
#endif //__JSCOL_PLANE_H__