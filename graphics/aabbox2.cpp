/*=====================================================================
aabbox2.cpp
-----------
File created by ClassTemplate on Sun Dec 12 19:47:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "aabbox2.h"


#include <assert.h>


AABBox2::AABBox2(const Vec2d& min_, const Vec2d& max_)
:	min(min_),
	max(max_)
{
	assert(this->getDimsX() >= 0.0f);
	assert(this->getDimsY() >= 0.0f);
}


AABBox2::~AABBox2()
{
	
}

void AABBox2::enlargeToHoldPoint(const Vec2d& p)
{
	if(p.x > max.x)
		max.x = p.x;
	else if(p.x < min.x)
		min.x = p.x;

	if(p.y > max.y)
		max.y = p.y;
	else if(p.y < min.y)
		min.y = p.y;
}




