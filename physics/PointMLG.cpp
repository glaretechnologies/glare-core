/*=====================================================================
PointMLG.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-08-18 21:52:11 +0100
=====================================================================*/
#include "PointMLG.h"


#include "../indigo/TestUtils.h"
#include "../utils/MTwister.h"
#include "../utils/timer.h"
#include <set>


class TestPointData
{
public:


};


static bool operator < (const Point<TestPointData>& a, const Point<TestPointData>& b)
{
	return a.pos < b.pos;
}


static bool operator == (const Point<TestPointData>& a, const Point<TestPointData>& b)
{
	return a.pos == b.pos;
}


#if BUILD_TESTS


void doPointMLGTests()
{
	{
		std::vector<Point<TestPointData> > points;
		points.resize(2);
		points[0].pos = Vec3f(2.1f, 2.1f, 2.1f);
		points[1].pos = Vec3f(3.9f, 3.9f, 3.9f);


		PointMLG<TestPointData> point_mlg(
			Vec4f(2,2,2,1), // min
			Vec4f(4,4,4,1) // max
		);

		point_mlg.build(points);

		testAssert(point_mlg.grid->nodes.size() == 1);

		testAssert(point_mlg.grid->nodes[0].data.num_points == 2);
		testAssert(point_mlg.grid->nodes[0].data.points[0].pos == Vec3f(2.1f, 2.1f, 2.1f));
		testAssert(point_mlg.grid->nodes[0].data.points[1].pos == Vec3f(3.9f, 3.9f, 3.9f));
	}

	// Test with lots of random points
	{
		std::vector<Point<TestPointData> > points;
		int num = 128000;
		points.resize(num);
		MTwister rng(1);
		for(int i=0; i<num; ++i)
		{
			points[i].pos = Vec3f(2,2,2) + Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) * 2.0f;
		}
			


		PointMLG<TestPointData> point_mlg(
			Vec4f(2,2,2,1), // min
			Vec4f(4,4,4,1) // max
		);

		Timer timer;

		point_mlg.build(points);

		conPrint("build time: " + timer.elapsedStringNPlaces(4));
		conPrint("Num nodes: " + toString(point_mlg.grid->nodes.size()));
		conPrint("Max depth: " + toString(point_mlg.max_depth));


		
		int num_queries = 1000;
		for(int z=0; z<num_queries; ++z)
		{
			// Do reference point query
			float r = rng.unitRandom() * 0.1f;
			Vec4f query_p = Vec4f(2,2,2,1) + Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0) * 2.0f;

			std::set<Point<TestPointData> > ref_result;

			for(int i=0; i<(int)points.size(); ++i)
				if(points[i].pos.toVec4fPoint().getDist2(query_p) <= r*r)
					ref_result.insert(points[i]);


			// Do fast query
			std::vector<Point<TestPointData> > fast_result_v;
			point_mlg.getPointsInRadius(query_p, r, fast_result_v);

			// Form a set
			std::set<Point<TestPointData> > fast_result;
			for(int i=0; i<(int)fast_result_v.size(); ++i)
				fast_result.insert(fast_result_v[i]);

			// Check both the queries returned the same results.
			testAssert(ref_result == fast_result);
		}
	}
}


#endif
