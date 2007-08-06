/*=====================================================================
hitinfo.h
---------
File created by ClassTemplate on Mon May 30 19:24:40 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __HITINFO_H_666_
#define __HITINFO_H_666_



#include "../maths/vec2.h"

/*=====================================================================
HitInfo
-------

=====================================================================*/
class HitInfo
{
public:
	/*=====================================================================
	HitInfo
	-------
	
	=====================================================================*/
	HitInfo();

	~HitInfo();

	inline bool operator == (const HitInfo& rhs) const;


	unsigned int hittriindex;
	Vec2d hittricoords;//hit position in triangle barycentric coordinates. 


};

bool HitInfo::operator == (const HitInfo& rhs) const
{
	return hittriindex == rhs.hittriindex && hittricoords == rhs.hittricoords;
}


#endif //__HITINFO_H_666_




