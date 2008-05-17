/*=====================================================================
Intersectable.cpp
-----------------
File created by ClassTemplate on Sat Apr 15 19:01:14 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_Intersectable.h"




namespace js
{




Intersectable::Intersectable()
:	object_index(-1)
{
}
/*

Intersectable::~Intersectable()
{
	
}*/

void Intersectable::setObjectIndex(int i)
{
	assert(object_index == -1);
	assert(i >= 0);

	object_index = i; 
}




} //end namespace js






