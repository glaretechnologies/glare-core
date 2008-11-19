/*=====================================================================
FBM2DMap.cpp
------------
File created by ClassTemplate on Tue May 27 19:02:52 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "FBM2DMap.h"


#include "PerlinNoise.h"


FBM2DMap::FBM2DMap(Coord u_scale_, Coord v_scale_)
:	u_scale(u_scale_),
	v_scale(v_scale_)
{
	
}


FBM2DMap::~FBM2DMap()
{
	
}


const Colour3<FBM2DMap::Value> FBM2DMap::vec3SampleTiled(Coord x, Coord y) const
{
	return Colour3<Value>(scalarSampleTiled(x, y));
}

FBM2DMap::Value FBM2DMap::scalarSampleTiled(Coord x, Coord y) const
{
	/*double sum = 0.0;
	double scale = 1.0;
	double weight = 1.0;
	for(int i=0; i<10; ++i)
	{
		sum += weight * PerlinNoise::noise(x * scale * u_scale, y * scale * v_scale, 0.0);
		scale *= 1.99;
		weight *= 0.5;
	}

	return sum;*/

	return PerlinNoise::FBM(
		x * u_scale, 
		y * v_scale, 
		0.0, // z
		10 // num octaves
		);
}



