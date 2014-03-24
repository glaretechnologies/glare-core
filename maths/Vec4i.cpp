/*=====================================================================
Vec4i.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-10 21:02:10 +0000
=====================================================================*/
#include "Vec4i.h"


#include "../utils/StringUtils.h"


const std::string Vec4i::toString() const
{
	return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
}
