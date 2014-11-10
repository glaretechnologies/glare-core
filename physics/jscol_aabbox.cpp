/*=====================================================================
aabbox.cpp
----------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_aabbox.h"


#include "../maths/mathstypes.h"
#include "../indigo/TestUtils.h"
#include <limits>


namespace js
{


AABBox AABBox::emptyAABBox()
{
	return AABBox(
		Vec4f( std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(), 1.0f),
		Vec4f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f)
	);
}


bool AABBox::invariant() const
{
	return max_.x[0] >= min_.x[0] && max_.x[1] >= min_.x[1] && max_.x[2] >= min_.x[2];
}


static inline float myMin(float a, float b, float c)
{
	return ::myMin(a, ::myMin(b, c));
}


static inline float myMax(float a, float b, float c)
{
	return ::myMax(a, ::myMax(b, c));
}


} // End namespace namespace js


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/CycleTimer.h"


void js::AABBox::test()
{
	{
		AABBox box(Vec4f(0,0,0,1.0f), Vec4f(1,2,3,1.0f));

		testAssert(box == AABBox(Vec4f(0,0,0,1.0f), Vec4f(1,2,3,1.0f)));
		testAssert(::epsEqual(box.getSurfaceArea(), 22.f));
		testAssert(::epsEqual(box.axisLength(0), 1.f));
		testAssert(::epsEqual(box.axisLength(1), 2.f));
		testAssert(::epsEqual(box.axisLength(2), 3.f));
		testAssert(box.longestAxis() == 2);

		box.enlargeToHoldPoint(Vec4f(10, 20, 0, 1.0f));
		testAssert(box == AABBox(Vec4f(0,0,0, 1.0f), Vec4f(10,20,3, 1.0f)));

		box.enlargeToHoldPoint(Vec4f(-10, -20, 0, 1.0f));
		testAssert(box == AABBox(Vec4f(-10,-20,0, 1.0f), Vec4f(10,20,3, 1.0f)));

	}

	{
		const AABBox box(Vec4f(0,0,0,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(0.0f, 0.0f, 1.0f, 0.f));
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);
		const Vec4f raystart(0, 0, -1.0f, 1);
		float near, far;
		const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
		testAssert(hit != 0);
		testAssert(::epsEqual(near, 1.0f));
		testAssert(::epsEqual(far, 2.0f));
	}
	{
		const AABBox box(Vec4f(0,0,0,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(0.0f, 0.0f, 1.0f, 0.f));
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);
		const Vec4f raystart(2.0f, 0,-1.0f, 1.f);
		float near, far;
		const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
		testAssert(hit == 0);
	}

	{
		const AABBox box(Vec4f(0,0,0,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(0.0f, 0.0f, 1.0f, 0.f));
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);
		const Vec4f raystart(1.0f, 0, -1.0f, 1.f);
		float near, far;
		const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
		testAssert(hit != 0);
		testAssert(::epsEqual(near, 1.0f));
		testAssert(::epsEqual(far, 2.0f));
	}


	// Performance test
	conPrint("rayAABBTrace() [float]");
	{
		
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		
		
		const AABBox box(Vec4f(0.2f,0.2f,0.2f,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(0.0f, 0.0f, 1.0f, 0.f));
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);

		const int trials = 5;
		const int N = 1000000;
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.000001f;
				const Vec4f raystart(0.5f, 0.5f, -1.0f - x, 1);

				float near, far;
				const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
				if(hit != 0)
					sum += near;
			}
			elapsed = ::myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}
}


#endif // BUILD_TESTS
