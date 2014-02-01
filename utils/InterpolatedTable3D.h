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
	Real getInvXStep() const { return recip_x_step; }

	Real getStartY() const { return start_y; }
	Real getEndY() const { return end_y; }
	Real getInvYStep() const { return recip_y_step; }

	Real getStartZ() const { return start_z; }
	Real getEndZ() const { return end_z; }
	Real getInvZStep() const { return recip_z_step; }

	static void test();

private:
	Array3D<Real> data;
	Real start_x;
	Real start_y;
	Real start_z;
	Real recip_x_step;
	Real recip_y_step;
	Real recip_z_step;
	int num_x_minus_1;
	int num_y_minus_1;
	int num_z_minus_1;

	Real end_x;
	Real end_y;
	Real end_z;
	Real x_step;
	Real y_step;
	Real z_step;
};
