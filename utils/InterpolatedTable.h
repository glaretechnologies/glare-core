/*=====================================================================
InterpolatedTable.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 08 21:34:59 +1200 2010
=====================================================================*/
#pragma once


#include "Array2D.h"
#include "../indigo/PolarisationVec.h"
#include "../maths/vec2.h"


/*=====================================================================
InterpolatedTable
-------------------
Will be laid out in memory like:

Wavelengths: 380 390 400 410 ... 
y:          
0            x	 x	 x	 x
1            x	 x	 x   x
2            x	 x	 x   x
3            x	 x	 x   x
=====================================================================*/
class InterpolatedTable
{
public:
	typedef float Real;

	InterpolatedTable(
		const Array2D<Real>& data, 
		Real start_x, Real end_x,
		Real start_y, Real end_y
	);
	~InterpolatedTable();

	Real getValue(Real wavelength, Real y) const;
	const Vec2f getValues(const Vec2f& x_vals, Real y) const;
	void getValues(const SpectralVector& wavelengths, Real y, PolarisationVec& values_out) const;


	const Array2D<Real>& getData() const { return data; }

	Real getStartX() const { return start_x; }
	Real getEndX() const { return end_x; }
	Real getInvXStep() const { return recip_x_step; }

	Real getStartY() const { return start_y; }
	Real getEndY() const { return end_y; }
	Real getInvYStep() const { return recip_y_step; }

	static void test();

private:
	Array2D<Real> data;
	Real start_x;
	Real start_y;
	Real recip_x_step;
	Real recip_y_step;
	int width_minus_1;
	int height_minus_1;

	Real x_step;
	Real y_step;
	Real end_x;
	Real end_y;
};
