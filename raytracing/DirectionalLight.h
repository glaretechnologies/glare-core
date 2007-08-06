/*=====================================================================
DirectionalLight.h
------------------
File created by ClassTemplate on Thu Jun 23 04:41:34 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DIRECTIONALLIGHT_H_666_
#define __DIRECTIONALLIGHT_H_666_


#include "../maths/vec3.h"
#include "../graphics/colour3.h"


/*=====================================================================
DirectionalLight
----------------

=====================================================================*/
class DirectionalLight
{
public:
	/*=====================================================================
	DirectionalLight
	----------------
	
	=====================================================================*/
	DirectionalLight(const Vec3& neg_lightdir, const Colour3& power);

	~DirectionalLight();


	Vec3 neg_lightdir;

	Colour3 power;

};



#endif //__DIRECTIONALLIGHT_H_666_




