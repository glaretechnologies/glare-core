/*=====================================================================
InterpolatedTable3D.h
---------------------
Copyright Glare Technologies Limited 2012 -
=====================================================================*/
#pragma once


#include "Array3D.h"


/*=====================================================================
InterpolatedTable3D
-------------------
Three dimensional data table.
Does linear interpolation.
=====================================================================*/
class InterpolatedTable3D
{
public:
	typedef float Real;

	InterpolatedTable3D(
		const Array3D<Real>& data, 
		Real start_x, Real end_x,
		Real start_y, Real end_y,
		Real start_z, Real end_z
	);
	~InterpolatedTable3D();


	// Get interpolated value
	Real getValue(Real x, Real y, Real z) const;


	const Array3D<Real>& getData() const { return data; }

	Real getStartX() const { return start_x; }
	Real getEndX() const { return end_x; }

	static void test();

private:
	Real start_x;
	Real end_x;
	Real x_step;
	Real recip_x_step;

	Real start_y;
	Real end_y;
	Real y_step;
	Real recip_y_step;

	Real start_z;
	Real end_z;
	Real z_step;
	Real recip_z_step;

	Array3D<Real> data;
};
