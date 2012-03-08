/*=====================================================================
InterpolatedTable.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 08 21:34:59 +1200 2010
=====================================================================*/
#pragma once


#include "array2d.h"
class SpectralVector;
class PolarisationVec;


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
		const Array2d<Real>& data, 
		Real start_x, Real end_x,
		Real start_y, Real end_y
	);
	~InterpolatedTable();

	Real getValue(Real wavelength, Real y) const;
	void getValues(const SpectralVector& wavelengths, Real y, PolarisationVec& values_out) const;


	const Array2d<Real>& getData() const { return data; }

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
	Array2d<Real> data;
};
