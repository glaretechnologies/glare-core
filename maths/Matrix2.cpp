/*=====================================================================
Matrix2.cpp
-----------
File created by ClassTemplate on Sun Mar 05 17:55:13 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "Matrix2.h"


#include "../utils/stringutils.h"


template <>
const std::string Matrix2<float>::toString() const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + floatToString(elem(i, 0)) + "," + floatToString(elem(i, 1)) + "|" + ((i < 1) ? "\n" : "");
	return s;
}

template <>
const std::string Matrix2<double>::toString() const
{
	std::string s;
	for(int i=0; i<2; ++i)
		s += "|" + doubleToString(elem(i, 0)) + "," + doubleToString(elem(i, 1)) + "|" + ((i < 1) ? "\n" : "");
	return s;
}







