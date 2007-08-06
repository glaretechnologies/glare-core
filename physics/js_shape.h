#ifndef __JS_SHAPE_H__
#define __JS_SHAPE_H__

#include "C:\programming\maths\maths.h"



namespace js
{

/*============================================================================
Shape
-----
reprazents a collision topology
============================================================================*/
class Shape
{
public:
	Shape(){}
	virtual ~Shape(){}

	virtual bool lineStabsShape(float& t_out, Vec3& normal_out, const Vec3& raystart_os, const Vec3& rayend_os) = 0;

};







}//end namespace js
#endif //__JS_SHAPE_H__