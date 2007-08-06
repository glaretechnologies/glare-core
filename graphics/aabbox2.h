/*=====================================================================
aabbox2.h
---------
File created by ClassTemplate on Sun Dec 12 19:47:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __AABBOX2_H_666_
#define __AABBOX2_H_666_


#include "../maths/vec2.h"



/*=====================================================================
AABBox2
-------
2-dimensional axis-aligned bounding box
=====================================================================*/
class AABBox2
{
public:
	/*=====================================================================
	AABBox2
	-------
	
	=====================================================================*/
	AABBox2(const Vec2d& min, const Vec2d& max);

	~AABBox2();


	const Vec2d getDims() const { return max - min; }

	inline float getArea() const;

	inline float getDimsX() const { return max.x - min.x; }
	inline float getDimsY() const { return max.y - min.y; }
	
	void enlargeToHoldPoint(const Vec2d& p);


	Vec2d min;
	Vec2d max;


};

float AABBox2::getArea() const
{
	return (max.x - min.x) * (max.y - min.y);
}

#endif //__AABBOX2_H_666_




