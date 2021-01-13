/*=====================================================================
DouglasPeuckerAlg.cpp
---------------------
File created by ClassTemplate on Thu May 21 11:05:42 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "DouglasPeuckerAlg.h"


#include "../utils/TestUtils.h"


DouglasPeuckerAlg::DouglasPeuckerAlg()
{
}


DouglasPeuckerAlg::~DouglasPeuckerAlg()
{
}


// http://mathworld.wolfram.com/Point-LineDistance2-Dimensional.html
template <class Real>
inline static Real distanceFromLineSegment(const Vec2<Real>& p, const Vec2<Real>& start, const Vec2<Real>& end)
{
	// Get vector perpendicular to end - start
	const Vec2<Real> v(end.y - start.y, start.x - end.x);

	assert(epsEqual(dot(v, end - start), (Real)0.0));

	return std::fabs(dot(normalise(v), start - p));
}


void DouglasPeuckerAlg::approximate(const std::vector<Vec2<Real> >& points, std::vector<Vec2<Real> >& points_out, std::vector<unsigned int>& simplified_indices_out, Real epsilon)
{
	assert(points.size() >= 2);
	assert(epsilon >= 0.0);
	
	points_out.resize(0);
	simplified_indices_out.resize(0);

	doApproximate(points, 0, (int)points.size() - 1, points_out, simplified_indices_out, epsilon);

	points_out.push_back(points[points.size() - 1]);

	simplified_indices_out.push_back((int)points.size() - 1);
}


void DouglasPeuckerAlg::doApproximate(const std::vector<Vec2<Real> >& points, int start_i, int end_i, std::vector<Vec2<Real> >& points_out, std::vector<unsigned int>& simplified_indices_out, Real epsilon)
{
	assert(points.size() >= 2);
	assert(end_i > start_i);
	assert(epsilon >= 0.0);

	// Find the point with the maximum distance from the current line segment
	Real max_dist = -1.0;
	int max_index = 0;

	for(int i=start_i+1; i<end_i; ++i)
	{
		const float dist = distanceFromLineSegment(points[i], points[start_i], points[end_i]);
		if(dist > max_dist)
		{
			max_dist = dist;
			max_index = i;
		}
	}

	if(max_dist >= epsilon)
	{
		// Subdivide curve
		doApproximate(points, start_i, max_index, points_out, simplified_indices_out, epsilon);
		doApproximate(points, max_index, end_i, points_out, simplified_indices_out, epsilon);
	}
	else
	{
		// The current segment is a sufficient approximation to the true curve
		// So add the start vertex.
		// The subsequent segment will add the end vertex
		points_out.push_back(points[start_i]);

		simplified_indices_out.push_back(start_i);
	}
}


#if (BUILD_TESTS)
void DouglasPeuckerAlg::test()
{
	{
	std::vector<Vec2<Real> > points;
	points.push_back(Vec2<Real>(0.f, 0.f));
	points.push_back(Vec2<Real>(1.f, 0.1f));
	points.push_back(Vec2<Real>(2.f, 0.2f));
	points.push_back(Vec2<Real>(3.f, 1.0f));
	points.push_back(Vec2<Real>(4.f, 0.5f));
	points.push_back(Vec2<Real>(5.f, 0.0f));

	std::vector<Vec2<Real> > res;
	std::vector<unsigned int> simplified_indices;
	approximate(points, res, simplified_indices, 0.01f);

	testAssert(res.size() == 4);
	testAssert(epsEqual(res[0], Vec2<Real>(0.f, 0.f)));
	testAssert(epsEqual(res[1], Vec2<Real>(2.f, 0.2f)));
	testAssert(epsEqual(res[2], Vec2<Real>(3.f, 1.f)));
	testAssert(epsEqual(res[3], Vec2<Real>(5.f, 0.f)));

	testAssert(simplified_indices.size() == 4);
	testAssert(simplified_indices[0] == 0);
	testAssert(simplified_indices[1] == 2);
	testAssert(simplified_indices[2] == 3);
	testAssert(simplified_indices[3] == 5);
	}

	// Try with a slope to the line
	{
	std::vector<Vec2<Real> > points;
	points.push_back(Vec2<Real>(0.f, 0.f));
	points.push_back(Vec2<Real>(1.f, 1.1f));
	points.push_back(Vec2<Real>(2.f, 2.2f));
	points.push_back(Vec2<Real>(3.f, 4.0f));
	points.push_back(Vec2<Real>(4.f, 4.5f));
	points.push_back(Vec2<Real>(5.f, 5.0f));

	std::vector<Vec2<Real> > res;
	std::vector<unsigned int> simplified_indices;
	approximate(points, res, simplified_indices, 0.01f);

	testAssert(res.size() == 4);
	testAssert(epsEqual(res[0], Vec2<Real>(0.f, 0.f)));
	testAssert(epsEqual(res[1], Vec2<Real>(2.f, 2.2f)));
	testAssert(epsEqual(res[2], Vec2<Real>(3.f, 4.f)));
	testAssert(epsEqual(res[3], Vec2<Real>(5.f, 5.f)));

	testAssert(simplified_indices.size() == 4);
	testAssert(simplified_indices[0] == 0);
	testAssert(simplified_indices[1] == 2);
	testAssert(simplified_indices[2] == 3);
	testAssert(simplified_indices[3] == 5);
	}

}
#endif
