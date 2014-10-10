/*=====================================================================
Quat.cpp
--------
File created by ClassTemplate on Thu Mar 12 16:27:07 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Quat.h"


#include "../utils/StringUtils.h"


template <>
const std::string Quat<float>::toString() const
{
	return "(" + ::toString(v.x) + "," + ::toString(v.y) + "," + ::toString(v.z) + "," + ::toString(w) + ")";
}


template <>
const std::string Quat<double>::toString() const
{
	return "(" + ::toString(v.x) + "," + ::toString(v.y) + "," + ::toString(v.z) + "," + ::toString(w) + ")";
}
