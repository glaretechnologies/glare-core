/*=====================================================================
InterpolatedTable.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 08 21:34:59 +1200 2010
=====================================================================*/
#pragma once


#include "array2d.h"
class SpectralVector;


/*=====================================================================
InterpolatedTable
-------------------
Will be laid out in memory like:

Wavelengths: 380 390 400 410 ... 
y:			x	x	x	x
0			x	x	x	x
1			x	x	x
2
3
=====================================================================*/
class InterpolatedTable
{
public:
	typedef float Real;

	InterpolatedTable(
		const Array2d<Real>& data, 
		Real start_wavelen_m, Real end_wavelen_m,
		Real start_y, Real end_y
	);
	~InterpolatedTable();

	void getValues(const SpectralVector& wavelengths, Real y, SpectralVector& values_out) const;



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
