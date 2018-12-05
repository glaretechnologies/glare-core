/*=====================================================================
jscol_Triangle.cpp
------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "jscol_triangle.h"


// Closest point on line ab to p
inline static const Vec4f closestPointOnLine(const Vec4f& a, const Vec4f& a_to_b, const Vec4f& p)
{
	const Vec4f ap = p - a;
	const float d = dot(ap, a_to_b);
	if(d <= 0)
		return a;
	const float t = myMin(1.f, d / (a_to_b.length2()));
	return a + a_to_b * t;
}


// Returns closest point out of {a, b, c} to p
static inline const Vec4f closest(const Vec4f& a, const Vec4f& b, const Vec4f& c, const Vec4f& p)
{
	const float d1 = p.getDist2(a);
	const float d2 = p.getDist2(b);
	const float d3 = p.getDist2(c);

	if(d1 < d2)
	{
		if(d1 < d3)
			return a;
		else // else d3 <= d1 && d1 < d2
			return c;
	}
	else // else d1 >= d2 => d2 <= d1
	{
		if(d2 < d3)
			return b;
		else // else d2 >= d3   =>   d3 <= d2 && d2 <= d1
			return c;
	}
}


// Closest point on triangle to target.  Target must be in triangle plane and is assumed to be outside the triangle.
const Vec4f js::Triangle::closestPointOnTriangle(const Vec4f& target) const
{
	const Vec4f e0_closest = closestPointOnLine(v0, e1, target);

	const Vec4f v1 = v0 + e1;
	const Vec4f v1_to_v2 = e2 - e1;
	const Vec4f e1_closest = closestPointOnLine(v1, v1_to_v2, target);
	const Vec4f e2_closest = closestPointOnLine(v0, e2, target);

	return closest(e0_closest, e1_closest, e2_closest, target);
}


// Point must be on triangle plane
bool js::Triangle::pointInTri(const Vec4f& point) const
{
	// Check inside edge 0 (v0 to v1)
	const Vec4f edge0_normal = crossProduct(e1, normal);
	const float d0 = dot(v0, edge0_normal);
	if(dot(point, edge0_normal) > d0)
		return false;

	// Check inside edge 1 (v1 to v2)
	const Vec4f v1 = v0 + e1;
	const Vec4f v1_to_v2 = e2 - e1;
	const Vec4f edge1_normal = crossProduct(v1_to_v2, normal);
	const float d1 = dot(v1, edge1_normal);
	if(dot(point, edge1_normal) > d1)
		return false;

	// Check inside edge 2 (v2 to v0)
	const Vec4f edge2_normal = crossProduct(normal, e2);
	const float d2 = dot(v0, edge2_normal);
	if(dot(point, edge2_normal) > d2)
		return false;

	return true;
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"


void js::Triangle::test()
{
	//----------------------- Test closestPointOnLine() --------------------------------

	testAssert(epsEqual(closestPointOnLine(Vec4f(2, 2, 0, 1), Vec4f(5, 0, 0, 0), Vec4f(4, 5, 0, 1)), Vec4f(4, 2, 0, 1)));

	// Point off left side of line
	testAssert(epsEqual(closestPointOnLine(Vec4f(2, 2, 0, 1), Vec4f(5, 0, 0, 0), Vec4f(1, 5, 0, 1)), Vec4f(2, 2, 0, 1)));

	// Point off right side of line
	testAssert(epsEqual(closestPointOnLine(Vec4f(2, 2, 0, 1), Vec4f(5, 0, 0, 0), Vec4f(10, 5, 0, 1)), Vec4f(7, 2, 0, 1)));

	// Point above left side of line
	testAssert(epsEqual(closestPointOnLine(Vec4f(2, 2, 0, 1), Vec4f(5, 0, 0, 0), Vec4f(2, 10, 0, 1)), Vec4f(2, 2, 0, 1)));

	// Point above right side of line
	testAssert(epsEqual(closestPointOnLine(Vec4f(2, 2, 0, 1), Vec4f(5, 0, 0, 0), Vec4f(7, 10, 0, 1)), Vec4f(7, 2, 0, 1)));


	//----------------------- Test closestPointOnTriangle() --------------------------------
	{
		Vec4f v0(3, 2, 0, 1);
		Vec4f v1(13, 4, 0, 1);
		Vec4f v2(8, 9, 0, 1);
		js::Triangle tri(v0, v1 - v0, v2 - v0, Vec4f(0, 0, 1, 0));

		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(0, 0, 0, 1)), v0));
		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(15, 4, 0, 1)), v1));
		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(8, 10, 0, 1)), v2));

		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(9, -2, 0, 1)), Vec4f(8, 3, 0, 1))); // Point below edge v0 to v1

		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(13, 9, 0, 1)), Vec4f(10.5f, 6.5f, 0, 1))); // Point above and to the right of edge v1 to v2

		testAssert(epsEqual(tri.closestPointOnTriangle(Vec4f(4, 7, 0, 1)), Vec4f(5.70270252f, 5.78378391f, 0, 1))); // Point above and to the left of edge v0 to v2
	}


	//----------------------- Test pointInTri() --------------------------------
	{
		Vec4f v0(3, 2, 0, 1);
		Vec4f v1(13, 4, 0, 1);
		Vec4f v2(8, 9, 0, 1);
		js::Triangle tri(v0, v1 - v0, v2 - v0, Vec4f(0, 0, 1, 0));

		testAssert(tri.pointInTri(v0));
		testAssert(tri.pointInTri(v1 - Vec4f(1.0e-4f, 0, 0, 0)));
		testAssert(tri.pointInTri(v2 - Vec4f(0, 1.0e-4f, 0, 0)));
		testAssert(tri.pointInTri(Vec4f(7, 3, 0, 1)));
		testAssert(tri.pointInTri(Vec4f(7, 7, 0, 1)));
		testAssert(tri.pointInTri(Vec4f(12, 4, 0, 1)));
		testAssert(tri.pointInTri(Vec4f(10, 6, 0, 1)));
		testAssert(tri.pointInTri(Vec4f(8, 5, 0, 1)));

		// Test that points just outside of v0 are out
		testAssert(!tri.pointInTri(v0 - Vec4f(1.0e-4f, 0, 0, 0)));
		testAssert(!tri.pointInTri(v0 - Vec4f(0, 1.0e-4f, 0, 0)));

		// Test that points just outside of v1 are out
		testAssert(!tri.pointInTri(v1 + Vec4f(1.0e-4f, 0, 0, 0)));
		testAssert(!tri.pointInTri(v1 + Vec4f(0, 1.0e-4f, 0, 0)));
		testAssert(!tri.pointInTri(v1 - Vec4f(0, 1.0e-4f, 0, 0)));

		// Test that points just outside of v2 are out
		testAssert(!tri.pointInTri(v2 + Vec4f(1.0e-4f, 0, 0, 0)));
		testAssert(!tri.pointInTri(v2 + Vec4f(0, 1.0e-4f, 0, 0)));
		testAssert(!tri.pointInTri(v2 - Vec4f(1.0e-4f, 0, 0, 0)));

		testAssert(!tri.pointInTri(Vec4f(10, 3, 0, 1)));
		testAssert(!tri.pointInTri(Vec4f(10, -3, 0, 1)));

		testAssert(!tri.pointInTri(Vec4f(11, 7, 0, 1)));
		testAssert(!tri.pointInTri(Vec4f(110, 70, 0, 1)));
		
		testAssert(!tri.pointInTri(Vec4f(5, 6, 0, 1)));
		testAssert(!tri.pointInTri(Vec4f(50, 60, 0, 1)));
	}
}


#endif // BUILD_TESTS
