/*=====================================================================
InterpolatedTable.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 08 21:34:59 +1200 2010
=====================================================================*/
#include "InterpolatedTable.h"


#include "../indigo/SpectralVector.h"
#include "../indigo/PolarisationVec.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/platformutils.h"


InterpolatedTable::InterpolatedTable(
	const Array2d<Real>& data_, 
	Real start_x_, Real end_x_,
	Real start_y_, Real end_y_
	)
:	start_x(start_x_),
	end_x(end_x_),
	x_step((end_x_ - start_x_) / (data_.getWidth() - 1)),
	recip_x_step((data_.getWidth() - 1) / (end_x_ - start_x_)),
	start_y(start_y_),
	end_y(end_y_),
	y_step((end_y_ - start_y_) / (data_.getHeight() - 1)),
	recip_y_step((data_.getHeight() - 1) / (end_y_ - start_y_)),
	data(data_),
	width_minus_1((int)data_.getWidth() - 1),
	height_minus_1((int)data_.getHeight() - 1)
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
static inline Real biLerp(Real a, Real b, Real c, Real d, Real t_x, Real t_y)
{
	const Real one_t_x = 1 - t_x;
	const Real one_t_y = 1 - t_y;
	return 
		one_t_x * one_t_y * a + 
		t_x * one_t_y * b + 
		one_t_x * t_y * c + 
		t_x * t_y * d;
}


InterpolatedTable::Real InterpolatedTable::getValue(Real x_, Real y_) const
{
	// Do the max with zero with the floating point input, otherwise t will end up < 0 for input < 0.

	const Real x = ((myMax<Real>(x_, 0) - start_x) * recip_x_step);
	const int x_index =   myMin((int)x,      width_minus_1);
	const int x_index_1 = myMin(x_index + 1, width_minus_1);
	const Real t_x = x - x_index;

	const Real y = (myMax<Real>(y_, 0) - start_y) * recip_y_step;
	const int y_index   = myMin((int)y,      height_minus_1);
	const int y_index_1 = myMin(y_index + 1, height_minus_1);
	const Real t_y = y - y_index;

	assert(t_x >= 0.0f && t_y >= 0.0f);

	return biLerp(
		data.elem(x_index, y_index),
		data.elem(x_index_1, y_index),
		data.elem(x_index, y_index_1),
		data.elem(x_index_1, y_index_1),
		t_x,
		t_y
	);
}


const Vec2f InterpolatedTable::getValues(const Vec2f& x_vals, Real y_) const
{
	const Real y = (myMax<Real>(y_, 0) - start_y) * recip_y_step;
	const int y_index   = myMin((int)y,      height_minus_1);
	const int y_index_1 = myMin(y_index + 1, height_minus_1);
	const Real t_y = y - y_index;
	assert(t_y >= 0.0f);

	Vec2f res;
	{
		const Real x = ((myMax<Real>(x_vals.x, 0) - start_x) * recip_x_step);
		const int x_index =   myMin((int)x,      width_minus_1);
		const int x_index_1 = myMin(x_index + 1, width_minus_1);
		const Real t_x = x - x_index;
		assert(t_x >= 0.0f);
		res.x = biLerp(
			data.elem(x_index, y_index),
			data.elem(x_index_1, y_index),
			data.elem(x_index, y_index_1),
			data.elem(x_index_1, y_index_1),
			t_x,
			t_y
		);
	}
	{
		const Real x = ((myMax<Real>(x_vals.y, 0) - start_x) * recip_x_step);
		const int x_index =   myMin((int)x,      width_minus_1);
		const int x_index_1 = myMin(x_index + 1, width_minus_1);
		const Real t_x = x - x_index;
		assert(t_x >= 0.0f);
		res.y = biLerp(
			data.elem(x_index, y_index),
			data.elem(x_index_1, y_index),
			data.elem(x_index, y_index_1),
			data.elem(x_index_1, y_index_1),
			t_x,
			t_y
		);
	}
	return res;
}


void InterpolatedTable::getValues(const SpectralVector& wavelengths, Real y, PolarisationVec& values_out) const
{
	const int y_index = myClamp((int)((y - start_y) * recip_y_step), 0, (int)data.getHeight() - 1);
	const int y_index_1 = myMin(y_index + 1, (int)data.getHeight() - 1);

	// Calculate interpolation parameter along y axis.
	const Real t_y = (y - (start_y + (float)y_index * y_step)) * recip_y_step;

	assert(t_y >= -0.01f && t_y <= 1.01f);

	for(unsigned int i=0; i<wavelengths.size(); ++i)
	{
		const Real w = wavelengths[i];
		const int x_index = myClamp((int)((w - start_x) * recip_x_step), 0, (int)data.getWidth() - 1);
		const int x_index_1 = myMin(x_index + 1, (int)data.getWidth() - 1);

		// Calculate interpolation parameter along x axis.
		const Real t_x = (w - (start_x + (float)x_index * x_step)) * recip_x_step;

		assert(t_x >= 0 && t_x <= 1.01);

		/*printVar(i);
		printVar(data.elem(x_index, y_index));
		printVar(data.elem(x_index_1, y_index));
		printVar(data.elem(x_index, y_index_1));
		printVar(data.elem(x_index_1, y_index_1));
		printVar(t_x);
		printVar(t_y);*/

		values_out.e[i] = biLerp(
			data.elem(x_index, y_index),
			data.elem(x_index_1, y_index),
			data.elem(x_index, y_index_1),
			data.elem(x_index_1, y_index_1),
			t_x,
			t_y
		);

		//printVar(values_out.e[i]);
	}	
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/CycleTimer.h"


void InterpolatedTable::test()
{
	Array2d<Real> d(2, 2);
	d.elem(0, 0) = 0;  d.elem(1, 0) = 1;
	d.elem(0, 1) = 2;  d.elem(1, 1) = 3;

	InterpolatedTable table(
		d,
		0.0, // start x
		1.0, // end x
		0.f, // start y
		1.f // end y
	);

	// Test evals at corners
	testAssert(epsEqual(table.getValue(0.f, 0.f), 0.f));
	testAssert(epsEqual(table.getValue(1.f, 0.f), 1.f));
	testAssert(epsEqual(table.getValue(0.f, 1.f), 2.f));
	testAssert(epsEqual(table.getValue(1.f, 1.f), 3.f));

	// Test part way along top edge
	testAssert(epsEqual(table.getValue(0.2f, 0.f), 0.2f));

	// Test midpoint
	testAssert(epsEqual(table.getValue(0.5f, 0.5f), 1.5f));

	// Test bottom edge to right
	testAssert(epsEqual(table.getValue(0.75f, 1.0f), 2.75f));


	// Test out-of-bounds access
	testAssert(epsEqual(table.getValue(-0.1f, -0.1f), 0.f));
	testAssert(epsEqual(table.getValue(1.1f, 1.1f), 3.f));



	// Perf test
#if !defined(OSX) // Don't run these speed tests on OSX, as CycleTimer crashes on OSX.
	{
		// Run test
		CycleTimer timer;
		float sum = 0;
		const int N = 1000000;
		for(int n=0; n<N; ++n)
		{
			float x = n * 0.000001f;
			float y = n * 0.000045f;

			sum += table.getValue(x, y);
		}

		const double cycles = timer.getCyclesElapsed() / (double)(N);
		conPrint("cycles: " + toString(cycles));
		printVar(sum);
	}

	{
		// Run test
		CycleTimer timer;
		Vec2f sum(0.f);
		const int N = 1000000;
		for(int n=0; n<N; ++n)
		{
			float x = n * 0.000001f;
			float y = n * 0.000045f;

			sum += table.getValues(Vec2f(x, x), y);
		}

		const double cycles = timer.getCyclesElapsed() / (double)(N);
		conPrint("cycles: " + toString(cycles));
		conPrint("sum: " + sum.toString());
	}

#endif
}


#endif
