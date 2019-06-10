/*=====================================================================
InterpolatedTable1D.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Oct 07 22:28:38 +1300 2010
=====================================================================*/
#include "InterpolatedTable1D.h"


#if BUILD_TESTS


#include "../maths/PCG32.h"
#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"


void testInterpolatedTable1D()
{
	// Test where x spans unit interval
	{
		const float data_array[] = { 1.f, 2.f, 3.f, 4.f };
		const std::vector<float> data(data_array, data_array + 4);

		InterpolatedTable1D<float, float> t(data, 0.f, 1.f);

		testAssert(epsEqual(t.getValue(0.f), 1.f));
		testAssert(epsEqual(t.getValue(1/3.f), 2.f));
		testAssert(epsEqual(t.getValue(2/3.f), 3.f));
		testAssert(epsEqual(t.getValue(1.f), 4.f));


		// Make sure out-of-domain values don't cause a crash
		testAssert(t.getValue(-0.001f) != std::numeric_limits<float>::infinity()); // Test just out of domain
		testAssert(t.getValue(-1000.f) != std::numeric_limits<float>::infinity()); // Test far out of domain

		// Make sure out-of-domain values don't cause a crash, and that they take the endpoint values.
		testAssert(t.getValue(1.001f) == 4.f); // Test just out of domain
		testAssert(t.getValue(1000.f) == 4.f); // Test far out of domain

		// Test we don't crash on NaN input
		testAssert(t.getValue(std::numeric_limits<float>::quiet_NaN()) != std::numeric_limits<float>::infinity());
	}


	// Test where x interval is not the unit interval
	{
		const float data_array[] = { 1.f, 2.f, 3.f, 4.f };
		const std::vector<float> data(data_array, data_array + 4);

		InterpolatedTable1D<float, float> t(data, 10.f, 11.f);

		testAssert(epsEqual(t.getValue(10.f), 1.f));
		testAssert(epsEqual(t.getValue(10.f + 1/3.f), 2.f));
		testAssert(epsEqual(t.getValue(10.f + 2/3.f), 3.f));
		testAssert(epsEqual(t.getValue(11.f), 4.f));


		// Make sure out-of-domain values don't cause a crash, and that they take the endpoint values.
		testAssert(t.getValue(9.999f) != std::numeric_limits<float>::infinity()); // Test just out of domain
		testAssert(t.getValue(7.0f) != std::numeric_limits<float>::infinity()); // Test far out of domain
		testAssert(t.getValue(-1000.0f) != std::numeric_limits<float>::infinity()); // Test far out of domain

		// Make sure out-of-domain values don't cause a crash, and that they take the endpoint values.
		testAssert(t.getValue(11.001f) == 4.f); // Test just out of domain
		testAssert(t.getValue(15.001f) == 4.f); // Test just out of domain
		testAssert(t.getValue(1000.f) == 4.f); // Test far out of domain

		// Test we don't crash on NaN input
		testAssert(t.getValue(std::numeric_limits<float>::quiet_NaN()) != std::numeric_limits<float>::infinity());
	}
}


#endif
