/*=====================================================================
Plane.h
-------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "Vec4f.h"


class Planef
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	inline Planef();
	inline Planef(const Vec4f& origin, const Vec4f& normal);
	inline Planef(const Vec4f& normal, float dist_from_origin);


	inline bool pointTouchingFrontHalfSpace(const Vec4f& point) const;

	inline bool pointTouchingBackHalfSpace(const Vec4f& point) const;

	inline const Vec4f getPointOnPlane() const; // Returns a point somewhere on the plane - in particular the point closest to the origin.

	inline float signedDistToPoint(const Vec4f& p) const;

	inline const Vec4f closestPointOnPlane(const Vec4f& p) const;

	inline float getDist() const { return d; }
	inline float getD() const { return d; }

	inline const Vec4f& getNormal() const { return normal; }

	inline float rayIntersect(const Vec4f& raystart, const Vec4f& ray_unitdir) const;

	//NOTE: will return a position even if ray points AWAY from plane.
	inline const Vec4f getRayIntersectPos(const Vec4f& raystart, const Vec4f& ray_unitdir) const;

private:
	Vec4f normal;
	float d;
};


Planef::Planef()
{}


Planef::Planef(const Vec4f& origin, const Vec4f& normal_)
:	normal(normal_),
	d(dot(origin, normal))
{
	assert(origin[3]  == 1.f);
	assert(normal_[3] == 0.f);
	assert(normal.isUnitLength());
}


Planef::Planef(const Vec4f& normal_, float d_)
:	normal(normal_),
	d(d_)
{
	assert(normal_[3] == 0.f);
	assert(normal.isUnitLength());
}


bool Planef::pointTouchingFrontHalfSpace(const Vec4f& point) const
{
	assert(point[3] == 1.f);
	return dot(point, normal) >= d;
}


bool Planef::pointTouchingBackHalfSpace(const Vec4f& point) const
{
	assert(point[3] == 1.f);
	return dot(point, normal) <= d;
}


const Vec4f Planef::getPointOnPlane() const
{
	return Vec4f(0,0,0,1) + normal * d;
}


float Planef::signedDistToPoint(const Vec4f& p) const
{
	assert(p[3] == 1.f);
	return dot(p, normal) - d;
}


const Vec4f Planef::closestPointOnPlane(const Vec4f& p) const
{
	assert(p[3] == 1.f);
	return p - (normal * signedDistToPoint(p));
}


float Planef::rayIntersect(const Vec4f& raystart, const Vec4f& ray_unitdir) const
{
	assert(raystart[3] == 1.f);
	assert(ray_unitdir[3] == 0.f);

	const float start_to_plane_dist = signedDistToPoint(raystart);

	return start_to_plane_dist / -dot(ray_unitdir, normal);

	//NOTE: deal with div by 0?
}


const Vec4f Planef::getRayIntersectPos(const Vec4f& raystart, const Vec4f& ray_unitdir) const
{
	const float dist_till_intersect = rayIntersect(raystart, ray_unitdir);

	return raystart + ray_unitdir * dist_till_intersect;
}
