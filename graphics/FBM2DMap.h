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
	FBM2DMap(double u_scale, double v_scale);

	~FBM2DMap();

	virtual const Colour3d vec3SampleTiled(double x, double y) const;

	virtual double scalarSampleTiled(double x, double y) const;


	virtual unsigned int getWidth() const { return 1; }
	virtual unsigned int getHeight() const { return 1; }

private:
	double u_scale, v_scale;
};



#endif //__FBM2DMAP_H_666_




