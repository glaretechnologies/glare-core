/*=====================================================================
PerlinNoise2DMap.cpp
--------------------
File created by ClassTemplate on Sun May 18 21:06:02 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "PerlinNoise2DMap.h"


#include "PerlinNoise.h"

PerlinNoise2DMap::PerlinNoise2DMap(Coord u_scale_, Coord v_scale_)
:	u_scale(u_scale_),
	v_scale(v_scale_)
{
	
}


PerlinNoise2DMap::~PerlinNoise2DMap()
{
	
}


const Colour3<PerlinNoise2DMap::Value> PerlinNoise2DMap::vec3SampleTiled(Coord x, Coord y) const
{
	//return Colour3d(PerlinNoise::noise(x * u_scale, y * v_scale, 0.0));

	return Colour3<Value>(scalarSampleTiled(x, y));
}

PerlinNoise2DMap::Value PerlinNoise2DMap::scalarSampleTiled(Coord x, Coord y) const
{
	Value sum = 0.0;
	Value scale = 1.0;
	Value weight = 1.0;
	for(int i=0; i<10; ++i)
	{
		sum += weight * PerlinNoise::noise<Value>(x * scale * u_scale, y * scale * v_scale, 0.0);
		scale *= 1.99;
		weight *= 0.5;
	}

	return sum;
}



