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
	PerlinNoise2DMap(double u_scale, double v_scale);

	virtual ~PerlinNoise2DMap();


	virtual const Colour3d vec3SampleTiled(double x, double y) const;

	virtual double scalarSampleTiled(double x, double y) const;


	virtual unsigned int getWidth() const { return 1; }
	virtual unsigned int getHeight() const { return 1; }

private:
	double u_scale, v_scale;
};



#endif //__PERLINNOISE2DMAP_H_666_




