/*=====================================================================
hitinfo.h
---------
File created by ClassTemplate on Mon May 30 19:24:40 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __HITINFO_H_666_
#define __HITINFO_H_666_


#include "../maths/vec2.h"
#include <limits>


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
	inline HitInfo()
	{
#ifdef DEBUG
		sub_elem_index = std::numeric_limits<unsigned int>::max();
		sub_elem_coords.set(-666.0f, -666.0f);
#endif
	}

	inline ~HitInfo(){}

	inline bool operator == (const HitInfo& rhs) const;


	//unsigned int hittriindex;
	//Vec2d hittricoords; // Hit position in triangle barycentric coordinates. 

	unsigned int sub_elem_index;
	Vec2d sub_elem_coords;
};


bool HitInfo::operator == (const HitInfo& rhs) const
{
	return sub_elem_index == rhs.sub_elem_index && sub_elem_coords == rhs.sub_elem_coords;
}


#endif //__HITINFO_H_666_
