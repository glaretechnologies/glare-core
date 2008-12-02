/*=====================================================================
FBM2DMap.h
----------
File created by ClassTemplate on Tue May 27 19:02:52 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FBM2DMAP_H_666_
#define __FBM2DMAP_H_666_


#include "Map2D.h"


/*=====================================================================
FBM2DMap
--------

=====================================================================*/
class FBM2DMap : public Map2D
{
public:
	/*=====================================================================
	FBM2DMap
	--------
	
	=====================================================================*/
	FBM2DMap(Coord u_scale, Coord v_scale);

	~FBM2DMap();

	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	virtual Value scalarSampleTiled(Coord x, Coord y) const;


	virtual unsigned int getWidth() const { return 1; }
	virtual unsigned int getHeight() const { return 1; }

	virtual bool takesOnlyUnitIntervalValues() const { return false; }

private:
	Coord u_scale, v_scale;
};



#endif //__FBM2DMAP_H_666_




