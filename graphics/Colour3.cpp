/*=====================================================================
Colour3.cpp
-----------
Copyright Glare Technologies Limited
=====================================================================*/
#include "colour3.h"


#include "../utils/StringUtils.h"


template <>
const std::string Colour3<float>::toString() const
{
	return "(" + ::toString(r) + "," + ::toString(g) + "," + ::toString(b) + ")";
}


template <>
const std::string Colour3<double>::toString() const
{
	return "(" + ::toString(r) + "," + ::toString(g) + "," + ::toString(b) + ")";
}
