/*=====================================================================
Map2D.h
-------
File created by ClassTemplate on Sun May 18 20:59:34 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MAP2D_H_666_
#define __MAP2D_H_666_


#include "colour3.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class Image;
namespace Indigo { class TaskManager; }


/*=====================================================================
Map2D
-----
Base class for two-dimensional textures.
Since both core threads and UI threads might be accessing a map concurrently, 
make ThreadSafeRefCounted.
=====================================================================*/
class Map2D : public ThreadSafeRefCounted
{
public:
	/*=====================================================================
	Map2d
	-----
	
	=====================================================================*/
	Map2D();
	virtual ~Map2D();


	typedef float Value;
	typedef float Coord;

	virtual const Colour3<Value> pixelColour(size_t x, size_t y) const = 0;

	// X and Y are normalised image coordinates.
	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const = 0;

	// X and Y are normalised image coordinates.
	virtual Value scalarSampleTiled(Coord x, Coord y) const = 0;

	// s and t are normalised image coordinates.
	// Returns texture value (v) at (s, t)
	// Also returns dv/ds and dv/dt.
	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const = 0;


	virtual unsigned int getMapWidth() const = 0;
	virtual unsigned int getMapHeight() const = 0;

	virtual bool takesOnlyUnitIntervalValues() const = 0;

	virtual bool hasAlphaChannel() const { return false; }
	virtual Reference<Map2D> extractAlphaChannel() const { return Reference<Map2D>(); }

	virtual Reference<Image> convertToImage() const = 0;

	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const = 0;

	virtual Reference<Map2D> resizeToImage(const int width, bool& is_linear) const = 0;

	virtual unsigned int getBytesPerPixel() const = 0;

private:
	INDIGO_DISABLE_COPY(Map2D)
};


typedef Reference<Map2D> Map2DRef;


#endif //__MAP2D_H_666_




