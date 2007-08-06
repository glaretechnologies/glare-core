/*=====================================================================
Code By Nicholas Chapman.

  nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may *not* use this code for any commercial project.
=====================================================================*/
#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "../graphics/colour3.h"
#include "../maths/vec3.h"

//Point light

class Light
{
public:
	Light(const Vec3& pos_, const Colour3& c)
	:	pos(pos_),
		colour(c)
	{}

	~Light(){}

	inline const Vec3& getPos() const { return pos; }
	inline const Colour3& getColour() const { return colour; }

	Vec3 pos;
	Colour3 colour;
};


#endif //__LIGHT_H__
