/*=====================================================================
PointTreeTest.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Apr 07 14:22:30 +1200 2010
=====================================================================*/
#include "PointTreeTest.h"


#include "PointKDTree.h"
#include "../indigo/ThreadContext.h"
#include <iostream>
#include "../indigo/TestUtils.h"
#include "../maths/vec3.h"
#include "../utils/MTwister.h"


PointTreeTest::PointTreeTest()
{

}


PointTreeTest::~PointTreeTest()
{

}


void PointTreeTest::test()
{
	{
		std::vector<Vec3f> points;
		points.push_back(Vec3f(0,0,0));
		points.push_back(Vec3f(2,0,0));
		points.push_back(Vec3f(0,1,0));
		points.push_back(Vec3f(2,1,0));

		PointKDTree tree(points);

		tree.printTree(std::cout, 0, 0);

		ThreadContext context;
		{
		const uint32 res = tree.getNearestPoint(Vec3f(0.05, 0.05, 0.0), context);
		testAssert(res == 0);
		}

		{
		const uint32 res = tree.getNearestPoint(Vec3f(0.25, 0.25, 0.0), context);
		testAssert(res == 0);
		}
	}


	{
		std::vector<Vec3f> points(1000, Vec3f(0,0,0));

		PointKDTree tree(points);

		//tree.printTree(std::cout, 0, 0);
	}

	{
		std::vector<Vec3f> points;
		MTwister rng(1);
		for(int i=0; i<10000; ++i)
		{
			points.push_back(Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()));
		}

		ThreadContext context;
		PointKDTree tree(points);

		for(int i=0; i<10000; ++i)
		{
			const Vec3f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

			const uint32 res = tree.getNearestPoint(p, context);

			const uint32 debug_res = tree.getNearestPointDebug(points, p, context);

			testAssert(res == debug_res);
		}
	}

	{
		std::vector<Vec3f> points;
		MTwister rng(1);
		for(int i=0; i<10000; ++i)
		{
			points.push_back(Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) * 1000.0f);
		}

		ThreadContext context;
		PointKDTree tree(points);

		for(int i=0; i<10000; ++i)
		{
			const Vec3f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

			const uint32 res = tree.getNearestPoint(p, context);

			const uint32 debug_res = tree.getNearestPointDebug(points, p, context);

			testAssert(res == debug_res);
		}
	}


	exit(0);
}
