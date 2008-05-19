/*=====================================================================
Map2D.h
-------
File created by ClassTemplate on Sun May 18 20:59:34 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MAP2D_H_666_
#define __MAP2D_H_666_


#include "Colour3.h"
#include "../utils/refcounted.h"


/*=====================================================================
Map2D
-----

=====================================================================*/
class Map2D : public RefCounted
{
public:
	/*=====================================================================
	Map2d
	-----
	
	=====================================================================*/
	Map2D();

	virtual ~Map2D();


	// X and Y are normalised image coordinates.
	virtual const Colour3d vec3SampleTiled(double x, double y) const = 0;

	virtual double scalarSampleTiled(double x, double y) const = 0;


	virtual unsigned int getWidth() const = 0;
	virtual unsigned int getHeight() const = 0;
};



#endif //__MAP2D_H_666_




