/*=====================================================================
material.h
----------
File created by ClassTemplate on Sun Sep 09 22:36:02 2001
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __MATERIAL_H_666_
#define __MATERIAL_H_666_


#include "colour3.h"


/*=====================================================================
Material
--------

=====================================================================*/
class Material
{
public:

	Material(const Colour3& diffuse_colour, float specular_amount, float specular_coeff,
				float reflect_fraction, const Colour3& emitted_colour);

	~Material(){}

	Colour3 diffuse_colour;
	Colour3 emitted_colour;

	float specular_amount;//[0,infinity]. 0 = matt
	float specular_coeff;//(0, infinity). for phong lighting  a.k.a shineness. 

	float reflect_fraction;//[0,1]. 1 == total reflection, i.e. a perfect mirror

};


#endif //__MATERIAL_H_666_




