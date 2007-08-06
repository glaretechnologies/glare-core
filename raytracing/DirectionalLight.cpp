/*=====================================================================
DirectionalLight.cpp
--------------------
File created by ClassTemplate on Thu Jun 23 04:41:34 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "DirectionalLight.h"


#include <assert.h>

DirectionalLight::DirectionalLight(const Vec3& neg_lightdir_, const Colour3& power_)
:	neg_lightdir(neg_lightdir_),
	power(power_)
{
	assert(::epsEqual(neg_lightdir.length(), 1.0f));
}


DirectionalLight::~DirectionalLight()
{
	
}






