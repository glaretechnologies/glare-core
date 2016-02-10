/*=====================================================================
GridNoise.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-06-22 22:23:03 +0100
=====================================================================*/
#pragma once


/*=====================================================================
GridNoise
-------------------

=====================================================================*/
namespace GridNoise
{
	void init();

	float eval(float x, float y, float z);
	float eval(float x, float y, float z, float w);
	float eval(int x, int y, int z);
	float eval(int x, int y, int z, int w);
}
