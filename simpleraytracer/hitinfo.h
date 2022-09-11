/*=====================================================================
hitinfo.h
---------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../maths/vec2.h"


/*=====================================================================
HitInfo
-------

=====================================================================*/
class HitInfo
{
public:
	typedef float SubElemCoordsRealType;
	typedef Vec2<SubElemCoordsRealType> SubElemCoordsType;

	inline HitInfo() {}
	inline HitInfo(unsigned int sub_elem_index, const SubElemCoordsType& sub_elem_coords);

	inline bool operator == (const HitInfo& rhs) const;


	SubElemCoordsType sub_elem_coords;
	unsigned int sub_elem_index; // For example, index of a triangle in a RayMesh.
	//bool hit_opaque_ob;
};


HitInfo::HitInfo(unsigned int sub_elem_index_, const SubElemCoordsType& sub_elem_coords_)
:	sub_elem_index(sub_elem_index_),
	sub_elem_coords(sub_elem_coords_)
{}


bool HitInfo::operator == (const HitInfo& rhs) const
{
	return sub_elem_index == rhs.sub_elem_index && sub_elem_coords == rhs.sub_elem_coords;
}
