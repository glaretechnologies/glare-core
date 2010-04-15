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


PointTreeTest::PointTreeTest()
{

}


PointTreeTest::~PointTreeTest()
{

}


void PointTreeTest::test()
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

	exit(0);
}
