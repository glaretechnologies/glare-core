/*=====================================================================
InterpolatedTable.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 08 21:34:59 +1200 2010
=====================================================================*/
#include "InterpolatedTable.h"


#include "../indigo/SpectralVector.h"


InterpolatedTable::InterpolatedTable(
	const Array2d<Real>& data_, 
	Real start_wavelen_m, Real end_wavelen_m,
	Real start_y_, Real end_y_
	)
:	start_x(start_wavelen_m * (Real)1.0e9),
	end_x(end_wavelen_m * (Real)1.0e9),
	x_step((end_wavelen_m * (Real)1.0e9 - start_wavelen_m * (Real)1.0e9) / (Real)data_.getWidth()),
	recip_x_step((Real)data_.getWidth() / (end_wavelen_m * (Real)1.0e9 - start_wavelen_m * (Real)1.0e9)),
	start_y(start_y_),
	end_y(end_y_),
	y_step((end_y_ - start_y_) / (Real)data_.getHeight()),
	recip_y_step((Real)data_.getHeight() / (end_y_ - start_y_)),
	data(data_)
{

}


InterpolatedTable::~InterpolatedTable()
{

}


/*
a   b


c   d

f = 
(1-t_x)*(1-t_y)*a + 
t_x*(1-t_y)*b + 
(1-t_x)*t_y*c
t_x*t_y*d

*/

template <class Real> 
Real biLerp(Real a, Real b, Real c, Real d, Real t_x, Real t_y)
{
	const Real one_t_x = (Real)1 - t_x;
	const Real one_t_y = (Real)1 - t_y;
	return 
		one_t_x * one_t_y * a + 
		t_x * one_t_y * b + 
		one_t_x * t_y * c + 
		t_x + t_y * d;
}


void InterpolatedTable::getValues(const SpectralVector& wavelengths, Real y, SpectralVector& values_out) const
{
	const int y_index = myMax(0, (int)((y - start_y) * recip_y_step));
	const int y_index_1 = myMin(y_index + 1, (int)data.getHeight() - 1);

	// Calc interpolation parameter along y axis.
	const Real t_y = (y - (float)y_index * y_step) * recip_y_step;

	for(unsigned int i=0; i<wavelengths.size(); ++i)
	{
		const Real w = wavelengths[i];
		const int x_index = myMax(0, (int)((w - start_x) * recip_x_step));
		const int x_index_1 = myMin(x_index + 1, (int)data.getHeight() - 1);

		// Calc interpolation parameter along x axis.
		const Real t_x = (w - (float)x_index * x_step) * recip_x_step;

		values_out[i] = biLerp(
			data.elem(x_index, y_index),
			data.elem(x_index_1, y_index),
			data.elem(x_index, y_index_1),
			data.elem(x_index_1, y_index_1),
			t_x,
			t_y
		);
	}	
}
