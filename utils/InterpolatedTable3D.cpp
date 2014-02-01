/*=====================================================================
InterpolatedTable3D.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
=====================================================================*/
#include "InterpolatedTable3D.h"


#include "../indigo/globals.h"
#include "../utils/stringutils.h"


InterpolatedTable3D::InterpolatedTable3D(
	const Array3D<Real>& data_, 
	Real start_x_, Real end_x_,
	Real start_y_, Real end_y_,
	Real start_z_, Real end_z_
	)
:	start_x(start_x_),
	end_x(end_x_),
	x_step((end_x_ - start_x_) / (data_.dX() - 1)),
	recip_x_step((data_.dX() - 1) / (end_x_ - start_x_)),

	start_y(start_y_),
	end_y(end_y_),
	y_step((end_y_ - start_y_) / (data_.dY() - 1)),
	recip_y_step((data_.dY() - 1) / (end_y_ - start_y_)),

	start_z(start_z_),
	end_z(end_z_),
	z_step((end_z_ - start_z_) / (data_.dZ() - 1)),
	recip_z_step((data_.dZ() - 1) / (end_z_ - start_z_)),

	num_x_minus_1((int)data_.dX() - 1),
	num_y_minus_1((int)data_.dY() - 1),
	num_z_minus_1((int)data_.dZ() - 1),

	data(data_)
{
}


InterpolatedTable3D::~InterpolatedTable3D()
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


InterpolatedTable3D::Real InterpolatedTable3D::getValue(Real x_, Real y_, Real z_) const
{
	//NOTE: could optimise this by doing the evaluation of the indices and t values with SSE.

	// Do the max with zero with the floating point input, otherwise t will end up < 0 for input < 0.

	const Real x = ((myMax<Real>(x_, 0) - start_x) * recip_x_step);
	const int x_index =   myMin((int)x,      num_x_minus_1);
	const int x_index_1 = myMin(x_index + 1, num_x_minus_1);
	const Real t_x = x - x_index;

	const Real y = (myMax<Real>(y_, 0) - start_y) * recip_y_step;
	const int y_index   = myMin((int)y,      num_y_minus_1);
	const int y_index_1 = myMin(y_index + 1, num_y_minus_1);
	const Real t_y = y - y_index;

	const Real z = (myMax<Real>(z_, 0) - start_z) * recip_z_step;
	const int z_index   = myMin((int)z,      num_z_minus_1);
	const int z_index_1 = myMin(z_index + 1, num_z_minus_1);
	const Real t_z = z - z_index;

	const Real val_z = biLerp(
		data.e(x_index,   y_index,   z_index),
		data.e(x_index_1, y_index,   z_index),
		data.e(x_index,   y_index_1, z_index),
		data.e(x_index_1, y_index_1, z_index),
		t_x,
		t_y
	);

	const Real val_z_1 = biLerp(
		data.e(x_index,   y_index,   z_index_1),
		data.e(x_index_1, y_index,   z_index_1),
		data.e(x_index,   y_index_1, z_index_1),
		data.e(x_index_1, y_index_1, z_index_1),
		t_x,
		t_y
	);

	// Interpolate between the two bi-lerped values.
	return val_z * (1 - t_z) + val_z_1 * t_z;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/CycleTimer.h"


void InterpolatedTable3D::test()
{
	conPrint("InterpolatedTable3D::test()");

	Array3D<Real> d(2, 2, 2);
	d.e(0, 0, 0) = 0;  d.e(1, 0, 0) = 1;
	d.e(0, 1, 0) = 2;  d.e(1, 1, 0) = 3;

	d.e(0, 0, 1) = 4;  d.e(1, 0, 1) = 1;
	d.e(0, 1, 1) = 5;  d.e(1, 1, 1) = 6;

	InterpolatedTable3D table(
		d,
		0.0, // start x
		1.0, // end x
		0.f, // start y
		1.f, // end y
		0.f, // start z
		1.0f // end z
	);

	//// Test at z=0 ////
	// Test evals at corners
	testAssert(epsEqual(table.getValue(0.f, 0.f, 0.f), 0.f));
	testAssert(epsEqual(table.getValue(1.f, 0.f, 0.f), 1.f));
	testAssert(epsEqual(table.getValue(0.f, 1.f, 0.f), 2.f));
	testAssert(epsEqual(table.getValue(1.f, 1.f, 0.f), 3.f));

	// Test part way along top edge
	testAssert(epsEqual(table.getValue(0.2f, 0.f, 0.f), 0.2f));

	// Test midpoint
	testAssert(epsEqual(table.getValue(0.5f, 0.5f, 0.f), 1.5f));

	// Test bottom edge to right
	testAssert(epsEqual(table.getValue(0.75f, 1.0f, 0.f), 2.75f));


	//// Test at z=1 ////

	// Test evals at corners
	testAssert(epsEqual(table.getValue(0.f, 0.f, 1.f), 4.f));
	testAssert(epsEqual(table.getValue(1.f, 0.f, 1.f), 1.f));
	testAssert(epsEqual(table.getValue(0.f, 1.f, 1.f), 5.f));
	testAssert(epsEqual(table.getValue(1.f, 1.f, 1.f), 6.f));

	// Test part way along top edge
	testAssert(epsEqual(table.getValue(0.25f, 0.f, 1.f), 4 - 3.f/4.f));

	// Test midpoint
	testAssert(epsEqual(table.getValue(0.5f, 0.5f, 1.f), 4.0f));

	// Test bottom edge to right
	testAssert(epsEqual(table.getValue(0.75f, 1.0f, 1.f), 5.75f));

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
			float z = n * 0.00005465f;

			sum += table.getValue(x, y, z);
		}

		const double cycles = timer.getCyclesElapsed() / (double)(N);
		conPrint("cycles: " + toString(cycles));
		printVar(sum);
	}
#endif
}


#endif
