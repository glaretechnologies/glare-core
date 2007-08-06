/*=====================================================================
material.cpp
------------
File created by ClassTemplate on Sun Sep 09 22:36:02 2001
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#include "material.h"

#include <assert.h>

Material::Material(const Colour3& diffuse_colour_, float specular_amount_, 
				   float specular_coeff_, float reflect_fraction_, 
				   const Colour3& emitted_colour_)
:	diffuse_colour(diffuse_colour_),
	specular_amount(specular_amount_),
	specular_coeff(specular_coeff_),
	reflect_fraction(reflect_fraction_),
	emitted_colour(emitted_colour_)
{
	assert(specular_amount >= 0);
	assert(specular_coeff > 0);
	assert(reflect_fraction >= 0 && reflect_fraction <= 1);

}