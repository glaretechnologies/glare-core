/*=====================================================================
Vec4.cpp
--------
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Vec4f.h"


#include "../utils/stringutils.h"


const std::string Vec4f::toString() const
{
	return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
}


// Explicit template instantiation
//template const std::string Vec4<float>::toString() const;
