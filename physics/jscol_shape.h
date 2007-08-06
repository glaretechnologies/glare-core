#ifndef __JSCOL_SHAPE_H__
#define __JSCOL_SHAPE_H__

#include "../maths/maths.h"
#include <vector>
class AABox;
class Ray;

namespace js
{

class BoundingSphere;

/*============================================================================
Shape
-----
abstract base class
reprazents some collision geometry
============================================================================*/
class Shape
{
public:
	Shape(){}
	virtual ~Shape(){}

	virtual float getRayDist(const ::Ray& ray) = 0;

	//TEMP
	//virtual float traceRay(const ::Ray& ray, unsigned int& hit_tri_out, float& u_out,
	//						float& v_out, Vec3& normal_out) = 0;

	//virtual bool lineStabsShape(float& t_out, Vec3& normal_os_out, const Vec3& raystart_os,
	//											const Vec3& rayend_os) = 0;


	//returns true if sphere hit object.
	virtual bool traceSphere(const BoundingSphere& sphere, const Vec3& translation,	
										float& dist_out, Vec3& normal_os_out) = 0;//, bool& embedded_out,
										//Vec3& disembed_vec_out) = 0;


	virtual void appendCollPoints(std::vector<Vec3>& points_ws, const BoundingSphere& sphere_os,
											const CoordFrame& parentframe) = 0;


	//virtual const BoundingSphere& getBoundingSphereOS() = 0;
	virtual const AABox getBBoxOS() = 0;

};







}//end namespace js
#endif //__JSCOL_SHAPE_H__
