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

	typedef float SubElemCoordsRealType;
	typedef Vec2<SubElemCoordsRealType> SubElemCoordsType;

	inline HitInfo(unsigned int sub_elem_index, const SubElemCoordsType& sub_elem_coords);

	inline bool operator == (const HitInfo& rhs) const;


	//unsigned int hittriindex;
	//Vec2d hittricoords; // Hit position in triangle barycentric coordinates. 

	SubElemCoordsType sub_elem_coords;
	unsigned int sub_elem_index;
};


HitInfo::HitInfo(unsigned int sub_elem_index_, const SubElemCoordsType& sub_elem_coords_)
:	sub_elem_index(sub_elem_index_),
	sub_elem_coords(sub_elem_coords_)
{}


bool HitInfo::operator == (const HitInfo& rhs) const
{
	return sub_elem_index == rhs.sub_elem_index && sub_elem_coords == rhs.sub_elem_coords;
}


#endif //__HITINFO_H_666_
