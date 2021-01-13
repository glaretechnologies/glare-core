/*=====================================================================
UnitInterpolatedTable.cpp
-------------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "UnitInterpolatedTable.h"


UnitInterpolatedTable::UnitInterpolatedTable(const Array2D<Real>& data_)
:	recip_x_step((Real)(data_.getWidth() - 1)),
	recip_y_step((Real)(data_.getHeight() - 1)),
	data(data_),
	width_minus_1((int)data_.getWidth() - 1),
	height_minus_1((int)data_.getHeight() - 1),
	width((int)data_.getWidth()),
	height((int)data_.getHeight())
{}


UnitInterpolatedTable::~UnitInterpolatedTable()
{}


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


UnitInterpolatedTable::Real UnitInterpolatedTable::getValue(Real x_, Real y_) const
{
	// Do the max with zero with the floating point input, otherwise t will end up < 0 for input < 0.

	const Real x = myMax<Real>(x_, 0) * recip_x_step;
	const int x_index =   myMin((int)x,      width_minus_1);
	const int x_index_1 = myMin(x_index + 1, width_minus_1);
	const Real t_x = x - x_index;

	const Real y = myMax<Real>(y_, 0) * recip_y_step;
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


#include "../maths/Vec4f.h"
#include "../maths/Vec4i.h"


UnitInterpolatedTable::Real UnitInterpolatedTable::getValueSSE4(Real x_, Real y_) const
{
	//const Vec4f xy = mul(Vec4f(x_, y_, 0, 0), Vec4f(recip_x_step, recip_y_step, 0, 0));
	const Vec4f xy(x_ * recip_x_step, y_ * recip_y_step, 0, 0);

	// Convert to integer
	Vec4i xyi = toVec4i(xy);
	Vec4i xyi1 = xyi + Vec4i(1);

	// Clamp to [0, W)
	const Vec4i upper = Vec4i(width_minus_1, height_minus_1, 0, 0);
	xyi  = clamp(xyi,  Vec4i(0), upper);
	xyi1 = clamp(xyi1, Vec4i(0), upper);

	const Vec4f t = xy - toVec4f(xyi);

	const float t_x = t[0];
	const float t_y = t[1];

	const int x_index   = xyi[0];
	const int x_index_1 = xyi1[0];

	const int y_index   = xyi[1];
	const int y_index_1 = xyi1[1];

	assert(t_x >= 0.0f && t_y >= 0.0f);

	const float a = data.elem(x_index, y_index);
	const float b = data.elem(x_index_1, y_index);
	const float c = data.elem(x_index, y_index_1);
	const float d = data.elem(x_index_1, y_index_1);

	const float one_t_x = 1 - t_x;
	const float one_t_y = 1 - t_y;

	return 
		one_t_x * one_t_y * a + 
		t_x * one_t_y * b + 
		one_t_x * t_y * c + 
		t_x * t_y * d;
}


const Vec2f UnitInterpolatedTable::getValues(const Vec2f& x_vals, Real y_) const
{
	const Real y = myMax<Real>(y_, 0) * recip_y_step;
	const int y_index   = myMin((int)y,      height_minus_1);
	const int y_index_1 = myMin(y_index + 1, height_minus_1);
	const Real t_y = y - y_index;
	assert(t_y >= 0.0f);

	Vec2f res;
	{
		const Real x = (myMax<Real>(x_vals.x, 0) * recip_x_step);
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
		const Real x = (myMax<Real>(x_vals.y, 0) * recip_x_step);
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


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/CycleTimer.h"
#include "../utils/ConPrint.h"


void UnitInterpolatedTable::test()
{
	Array2D<Real> d(2, 2);
	d.elem(0, 0) = 0;  d.elem(1, 0) = 1;
	d.elem(0, 1) = 2;  d.elem(1, 1) = 3;

	UnitInterpolatedTable table(
		d
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


	const int N = 1000000;
	const int trials = 4;

	conPrint("table.getValue() [float]");
	{
		
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.0001f;
				sum += table.getValue(x, x + 0.001f);
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}
	
	conPrint("table.getValueSSE4() [float]");
	{
		
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.0001f;
				sum += table.getValueSSE4(x, x + 0.001f);
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}
}


#endif
