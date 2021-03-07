/*=====================================================================
jscol_aabbox.cpp
----------------
Copyright Glare Technologies Limited 2019 -
File created by ClassTemplate on Thu Nov 18 03:48:29 2004
=====================================================================*/
#include "jscol_aabbox.h"


namespace js
{


bool AABBox::invariant() const
{
	return max_.x[0] >= min_.x[0] && max_.x[1] >= min_.x[1] && max_.x[2] >= min_.x[2];
}


const std::string AABBox::toString() const
{
	return "min=" + min_.toString() + ", max=" + max_.toString();
}


const std::string AABBox::toStringNSigFigs(int n) const
{
	return "min=" + min_.toStringNSigFigs(n) + ", max=" + max_.toStringNSigFigs(n);
}


} // End namespace namespace js


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/CycleTimer.h"
#include "../utils/Timer.h"


inline static js::AABBox refTransformedAABB(const js::AABBox& aabb, const Matrix4f& M)
{
	js::AABBox res = js::AABBox::emptyAABBox();
	res.enlargeToHoldPoint(M * Vec4f(aabb.min_.x[0], aabb.min_.x[1], aabb.min_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.min_.x[0], aabb.min_.x[1], aabb.max_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.min_.x[0], aabb.max_.x[1], aabb.min_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.min_.x[0], aabb.max_.x[1], aabb.max_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.max_.x[0], aabb.min_.x[1], aabb.min_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.max_.x[0], aabb.min_.x[1], aabb.max_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.max_.x[0], aabb.max_.x[1], aabb.min_.x[2], 1.0f));
	res.enlargeToHoldPoint(M * Vec4f(aabb.max_.x[0], aabb.max_.x[1], aabb.max_.x[2], 1.0f));
	return res;
}


void js::AABBox::test()
{
	//------------------- Test intersectsAABB -------------------
	{
		//AABBox box1(Vec4f(1,2,3,1.0f), Vec4f(4,5,6,1.0f));
		AABBox box1(Vec4f(0.01f,0.01f,0.01f,1), Vec4f(0.99f,0.99f,0.99f,1));

		testAssert(!box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(0,   0,   0,   1)))); // Separate on all axes
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(0.1f,0.1f,0,   1)))); // Separate on z axis
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(0.1f,0,   0.1f,1)))); // Separate on y axis
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(0,   0.1f,0.1f,1)))); // Separate on x axis
		testAssert( box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(0.1f,0.1f,0.1f,1)))); // intersecting

		testAssert(!box1.intersectsAABB(AABBox(Vec4f(1,   1,   1,   1), Vec4f(2,2,2,1))));
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(0.9f,0.9f,1,   1), Vec4f(2,2,2,1))));
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(0.9f,1,   0.9f,1), Vec4f(2,2,2,1))));
		testAssert(!box1.intersectsAABB(AABBox(Vec4f(1,   0.9f,0.9f,1), Vec4f(2,2,2,1))));
		testAssert( box1.intersectsAABB(AABBox(Vec4f(0.9f,0.9f,0.9f,1), Vec4f(2,2,2,1))));

		// Test one box completely in the other
		testAssert(box1.intersectsAABB(AABBox(Vec4f(0.5f,0.5f,0.5f,1), Vec4f(0.6f,0.6f,0.6f,1))));
		testAssert(box1.intersectsAABB(AABBox(Vec4f(-1,-1,-1,1), Vec4f(2,2,2,1))));
	}

	{
		AABBox box(Vec4f(0,0,0,1.0f), Vec4f(1,2,3,1.0f));

		testAssert(box == AABBox(Vec4f(0,0,0,1.0f), Vec4f(1,2,3,1.0f)));
		testAssert(::epsEqual(box.getSurfaceArea(), 22.f));
		testAssert(::epsEqual(box.getHalfSurfaceArea(), 11.f));
		testAssert(::epsEqual(box.axisLength(0), 1.f));
		testAssert(::epsEqual(box.axisLength(1), 2.f));
		testAssert(::epsEqual(box.axisLength(2), 3.f));
		testAssert(box.longestAxis() == 2);

		box.enlargeToHoldPoint(Vec4f(10, 20, 0, 1.0f));
		testAssert(box == AABBox(Vec4f(0,0,0, 1.0f), Vec4f(10,20,3, 1.0f)));

		box.enlargeToHoldPoint(Vec4f(-10, -20, 0, 1.0f));
		testAssert(box == AABBox(Vec4f(-10,-20,0, 1.0f), Vec4f(10,20,3, 1.0f)));

		//------------------- Test getSurfaceArea() -------------------
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 20, 10, 1)).getSurfaceArea(), 200.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 10, 20, 1)).getSurfaceArea(), 200.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(10, 20, 20, 1)).getSurfaceArea(), 200.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(10, 10, 10, 1)).getSurfaceArea(), 0.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 20, 20, 1)).getSurfaceArea(), 600.f));

		//------------------- Test getHalfSurfaceArea() -------------------
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 20, 10, 1)).getHalfSurfaceArea(), 100.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 10, 20, 1)).getHalfSurfaceArea(), 100.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(10, 20, 20, 1)).getHalfSurfaceArea(), 100.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(10, 10, 10, 1)).getHalfSurfaceArea(), 0.f));
		testAssert(::epsEqual(AABBox(Vec4f(10, 10, 10, 1), Vec4f(20, 20, 20, 1)).getHalfSurfaceArea(), 300.f));
	}

	//------------------- Test rayAABBTrace -------------------
	const float nearly_zero = 1.0e-30f;
	{
		const AABBox box(Vec4f(0,0,0,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(nearly_zero, nearly_zero, 1.0f, 0.f));
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
		const Vec4f dir = normalise(Vec4f(nearly_zero, nearly_zero, 1.0f, 0.f));
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);
		const Vec4f raystart(2.0f, 0,-1.0f, 1.f);
		float near, far;
		const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
		testAssert(hit == 0);
	}

	{
		const AABBox box(Vec4f(0,0,0,1), Vec4f(1,1,1,1));
		const Vec4f dir = Vec4f(-nearly_zero, nearly_zero, 1.0f, 0.f); // +z dir
		const Vec4f recip_dir(1 / dir[0], 1 / dir[1], 1 / dir[2], 0);
		const Vec4f raystart(1.0f, 0, -1.0f, 1.f);
		float near, far;
		const int hit = box.rayAABBTrace(raystart, recip_dir, near, far);
		testAssert(hit != 0);
		testAssert(::epsEqual(near, 1.0f));
		testAssert(::epsEqual(far, 2.0f));
	}

	//----------------- Test contains(const Vec4f& p) -------------------
	{
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(0.9f, 0.9f, 0.9f, 1.f)));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(0.9f, 1.9f, 1.9f, 1.f)));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(1.9f, 1.9f, 2.9f, 1.f)));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(2.9f, 2.9f, 2.9f, 1.f)));
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(1.9f, 1.9f, 1.9f, 1.f)));
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(1,1,1, 1.f)));
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).contains(Vec4f(2,2,2, 1.f)));
	}

	//----------------- Test containsAABBox() -------------------
	{
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(0.5f, 0.5f, 0.5f, 1), Vec4f(2, 2, 2, 1))));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(0.5f, 1, 1, 1), Vec4f(2, 2, 2, 1))));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2.5, 2, 2, 1))));
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2.5, 2.5, 2.5, 1))));
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(1.5, 1.5, 1.5, 1), Vec4f(1.5, 1.5, 1.5, 1))));
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).containsAABBox(js::AABBox(Vec4f(1.0, 1.0, 1.0, 1), Vec4f(2, 2, 2, 1))));
	}

	//----------------- Test isEmpty() -------------------
	{
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(2, 2, 2, 1)).isEmpty());
		testAssert(!js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(1, 1, 1, 1)).isEmpty());
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(0, 0, 0, 1)).isEmpty() != 0);
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(1, 0, 0, 1)).isEmpty() != 0);
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(0, 1, 0, 1)).isEmpty() != 0);
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(0, 0, 1, 1)).isEmpty() != 0);
		testAssert(js::AABBox(Vec4f(1, 1, 1, 1), Vec4f(1, 0, 1, 1)).isEmpty() != 0);
	}

	//----------------- Test transformedAABB() and transformedAABBFast() -------------------
	{
		PCG32 rng(1);
		for(int i=0; i<1000; ++i)
		{
			// Make a random matrix
			SSE_ALIGN float e[16];
			for(int z=0; z<16; ++z)
				e[z] = (-1.f + 2.f * rng.unitRandom());

			e[3] = e[7] = e[11] = 0;
			e[15] = 1;
			const Matrix4f M(e);

			const js::AABBox aabb(
				Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f),
				Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f)
			);

			const js::AABBox ref_M_aabb = refTransformedAABB(aabb, M);

			const js::AABBox M_aabb = aabb.transformedAABB(M);
			testAssert(M_aabb == ref_M_aabb);
			
			const js::AABBox fast_M_aabb = aabb.transformedAABBFast(M);
			testAssert(epsEqual(fast_M_aabb.min_, ref_M_aabb.min_));
			testAssert(epsEqual(fast_M_aabb.max_, ref_M_aabb.max_));
		}
	}

	//----------------- Test getAABBCornerVerts -------------------
	{
		const js::AABBox aabb(Vec4f(10, 11, 12, 1), Vec4f(20, 21, 22, 1));

		Vec4f v[8];
		getAABBCornerVerts(aabb, v);
		testAssert(v[0] == Vec4f(10, 11, 12, 1));
		testAssert(v[1] == Vec4f(20, 11, 12, 1));
		testAssert(v[2] == Vec4f(10, 21, 12, 1));
		testAssert(v[3] == Vec4f(20, 21, 12, 1));
		testAssert(v[4] == Vec4f(10, 11, 22, 1));
		testAssert(v[5] == Vec4f(20, 11, 22, 1));
		testAssert(v[6] == Vec4f(10, 21, 22, 1));
		testAssert(v[7] == Vec4f(20, 21, 22, 1));
	}


	// perf-test transformedAABB() and transformedAABBFast()
	{
		PCG32 rng(1);

		// Make a random matrix
		SSE_ALIGN float e[16];
		for(int z=0; z<16; ++z)
			e[z] = (-1.f + 2.f * rng.unitRandom());

		e[3] = e[7] = e[11] = 0;
		e[15] = 1;
		const Matrix4f M(e);

		const js::AABBox aabb(
			Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f),
			Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f)
		);
	
		const int N = 100000;

		{
			Timer timer;
			Vec4f sum(0);
			for(int i=0; i<N; ++i)
			{
				const js::AABBox M_aabb = refTransformedAABB(aabb, M);
				sum += M_aabb.min_ + M_aabb.max_;
			}

			const double elapsed = timer.elapsed();
			conPrint("refTransformedAABB() elapsed: " + ::toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(sum.toString());
		}
		{
			Timer timer;
			Vec4f sum(0);
			for(int i=0; i<N; ++i)
			{
				const js::AABBox M_aabb = aabb.transformedAABB(M);
				sum += M_aabb.min_ + M_aabb.max_;
			}

			const double elapsed = timer.elapsed();
			conPrint("transformedAABB() elapsed: " + ::toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(sum.toString());
		}
		{
			Timer timer;
			Vec4f sum(0);
			for(int i=0; i<N; ++i)
			{
				const js::AABBox M_aabb = aabb.transformedAABBFast(M);
				sum += M_aabb.min_ + M_aabb.max_;
			}

			const double elapsed = timer.elapsed();
			conPrint("transformedAABBFast() elapsed: " + ::toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(sum.toString());
		}
	}

	// Performance test
	conPrint("rayAABBTrace() [float]");
	{
		
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		
		
		const AABBox box(Vec4f(0.2f,0.2f,0.2f,1), Vec4f(1,1,1,1));
		const Vec4f dir = normalise(Vec4f(0.1f, 0.1f, 1.0f, 0.f));
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
		conPrint("\tcycles: " + ::toString(cycles));
		conPrint("\tsum: " + ::toString(sum));
	}
}


#endif // BUILD_TESTS
