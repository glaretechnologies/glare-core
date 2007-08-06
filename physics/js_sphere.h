#ifndef __BALL_H__
#define __BALL_H__


#include "object.h"

namespace js
{

class Ball : public Object
{
	Ball(float radius, const Vec3& centre);
	virtual ~Ball();

	virtual bool lineStabsObject(float& t_out, const Vec3& raystart, const Vec3& rayend);
	

private:
	float radius;
};



}//end namespace js
#endif //__BALL_H__