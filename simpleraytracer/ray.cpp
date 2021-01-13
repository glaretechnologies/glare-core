/*=====================================================================
ray.cpp
-------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "ray.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"




void Ray::test()
{
	const float maxval = std::numeric_limits<float>::max();
	const float denorm_min = std::numeric_limits<float>::denorm_min();

	{
		const Ray ray(/*startpos=*/Vec4f(0, 0, 0, 1), /*unitdir=*/Vec4f(0.5f, -1.0f, 10.f, 0), 0.1f, 100.f);
		testAssert(epsEqual(ray.getRecipRayDirF(), Vec4f(2.f, -1.f, 0.1f, maxval)));
	}

	// Test avoidance of infinities in recip_unitdir_f.
	// Any dir components with a sufficiently small value that the reciprocal becomes +-INF should be relaced with maxval.
	{
		const Ray ray(/*startpos=*/Vec4f(0, 0, 0, 1), /*unitdir=*/Vec4f(0, 2.0f, 5.0f, 0), 0.1f, 100.f);
		testAssert(epsEqual(ray.getRecipRayDirF(), Vec4f(maxval, 0.5f, 0.2f, maxval)));
	}

	{
		const Ray ray(/*startpos=*/Vec4f(0, 0, 0, 1), /*unitdir=*/Vec4f(2.0, 0.0f, 5.0f, 0), 0.1f, 100.f);
		testAssert(epsEqual(ray.getRecipRayDirF(), Vec4f(0.5f, maxval, 0.2f, maxval)));
	}

	// Test with denorm_min component
	{
		const Ray ray(/*startpos=*/Vec4f(0, 0, 0, 1), /*unitdir=*/Vec4f(denorm_min, -denorm_min, 5.0f, 0), 0.1f, 100.f);
		testAssert(epsEqual(ray.getRecipRayDirF(), Vec4f(maxval, maxval, 0.2f, maxval)));
	}
}


#endif
