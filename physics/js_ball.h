#ifndef __BALL_H__
#define __BALL_H__


#include "js_object.h"

namespace js
{

class Ball : public Object
{
public:
	Ball(float radius_, const Vec3& centre, float mass, bool moveable = true)
		:	Object(centre, moveable, mass)
	{
		radius = radius_;
	}

	virtual ~Ball(){}

	virtual bool lineStabsObject(float& t_out, Vec3& normal_out, const Vec3& raystart, const Vec3& rayend);
	


private:
	float radius;
};



}//end namespace js
#endif //__BALL_H__