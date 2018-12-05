/*=====================================================================
jscol_Triangle.h
----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"


namespace js
{


class Triangle
{
public:
	inline Triangle(){}
	inline Triangle(const Vec4f& v0, const Vec4f& e1, const Vec4f& e2, const Vec4f& normal);

	// Is the point in the triangle?  Point must be on triangle plane.
	bool pointInTri(const Vec4f& point) const;

	// Closest point on triangle to target.  target must be in triangle plane and is assumed to be outside the triangle.
	const Vec4f closestPointOnTriangle(const Vec4f& target) const;

	static void test();

private:
	Vec4f v0;
	Vec4f e1; // = v1 - v0
	Vec4f e2; // = v2 - v0
	Vec4f normal;
};


Triangle::Triangle(const Vec4f& v0_, const Vec4f& e1_, const Vec4f& e2_, const Vec4f& normal_)
{
	assert(v0_[3] == 1.f);
	assert(e1_[3] == 0.f);
	assert(e2_[3] == 0.f);
	assert(normal_.isUnitLength() && normal_[3] == 0.f);
	v0 = v0_;
	e1 = e1_;
	e2 = e2_;
	normal = normal_;
}


} // end namespace js
