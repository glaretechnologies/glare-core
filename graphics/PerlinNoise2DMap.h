/*=====================================================================
PerlinNoise2DMap.h
------------------
File created by ClassTemplate on Sun May 18 21:06:02 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PERLINNOISE2DMAP_H_666_
#define __PERLINNOISE2DMAP_H_666_


#include "Map2D.h"


/*=====================================================================
PerlinNoise2DMap
----------------

=====================================================================*/
class PerlinNoise2DMap : public Map2D
{
public:
	/*=====================================================================
	PerlinNoise2DMap
	----------------
	
	=====================================================================*/
	PerlinNoise2DMap(Coord u_scale, Coord v_scale);

	virtual ~PerlinNoise2DMap();


	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	virtual Value scalarSampleTiled(Coord x, Coord y) const;


	virtual unsigned int getWidth() const { return 1; }
	virtual unsigned int getHeight() const { return 1; }

private:
	Coord u_scale, v_scale;
};



#endif //__PERLINNOISE2DMAP_H_666_




