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

	inline const Vec4f getPointOnPlane() const; // Returns a point somewhere on the plane (actually the point closest to the origin)

	inline const Vec4f projOnPlane(const Vec4f& vec) const;

	inline const Vec4f compNormalToPlane(const Vec4f& vec) const;

	inline float signedDistToPoint(const Vec4f& p) const;

	inline const Vec4f closestPointOnPlane(const Vec4f& p) const;

	inline float getDist() const { return d; }
	inline float getD() const { return d; }

	inline const Vec4f& getNormal() const { return normal; }

	//returns fraction of ray travelled. Will be in range [0, 1] if ray hit
	inline float finiteRayIntersect(const Vec4f& raystart, const Vec4f& rayend) const;

	inline float rayIntersect(const Vec4f& raystart, const Vec4f& ray_unitdir) const;

	//NOTE: will return a position even if ray points AWAY from plane.
	inline const Vec4f getRayIntersectPos(const Vec4f& raystart, const Vec4f& ray_unitdir) const;

	//inline const Planef transformedToLocal(const CoordFrame<float>& coordframe) const;

	inline bool isSpherePartiallyOnFrontSide(const Vec4f& sphere_center, float radius) const;
	inline bool isSphereTotallyOnFrontSide(const Vec4f& sphere_center, float radius) const;

	inline bool isPlaneOnFrontSide(const Planef& p) const;

	inline const Vec4f reflectPointInPlane(const Vec4f& point) const;
	inline const Vec4f reflectVectorInPlane(const Vec4f& vec) const;

private:
	Vec4f normal;
	float d;
};


//typedef Planef Planef;


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
	return dot(point, normal) >= d;
}


bool Planef::pointTouchingBackHalfSpace(const Vec4f& point) const
{
	return dot(point, normal) <= d;
}

/*
bool Planef::isPointOnPlane(const Vec4f& point) const 
{
	if(fabs(signedDistToPoint(point)) < 0.00001f)
		return true;
	else
		return false;
}*/



const Vec4f Planef::getPointOnPlane() const
{
	return Vec4f(0,0,0,1) + normal * d;
}


const Vec4f Planef::projOnPlane(const Vec4f& vec) const
{
	return vec - compNormalToPlane(vec);
}


const Vec4f Planef::compNormalToPlane(const Vec4f& vec) const
{
	return normal * dot(vec, normal);
}


float Planef::signedDistToPoint(const Vec4f& p) const
{
	return dot(p, normal) - d;
}


const Vec4f Planef::closestPointOnPlane(const Vec4f& p) const
{
	return p - (normal * signedDistToPoint(p));
}


float Planef::finiteRayIntersect(const Vec4f& raystart, const Vec4f& rayend) const
{
	const float raystart_dot_n = dot(raystart, normal);

	const float rayend_dot_n = dot(rayend, normal);

	const float denom = rayend_dot_n - raystart_dot_n;

	if(denom == 0.0f)
		return -1.0f;

	return (d - raystart_dot_n) / denom;

}


float Planef::rayIntersect(const Vec4f& raystart, const Vec4f& ray_unitdir) const
{

	const float start_to_plane_dist = signedDistToPoint(raystart);

	return start_to_plane_dist / -dot(ray_unitdir, normal);

	//NOTE: deal with div by 0?

}



const Vec4f Planef::getRayIntersectPos(const Vec4f& raystart, const Vec4f& ray_unitdir) const
{
	const float dist_till_intersect = rayIntersect(raystart, ray_unitdir);

	return raystart + ray_unitdir * dist_till_intersect;
}


//const Planef Planef::transformedToLocal(const CoordFrame<float>& coordframe) const
//{
//	// NOTE: there's probably a much faster way of computing this.
//	return Planef(
//		coordframe.transformPointToLocal(this->getPointOnPlane()),
//		coordframe.transformVecToLocal(this->getNormal())
//		);
//}


/*
const Plane Plane::transformToLocal(const CoordFrame& coordframe) const
{
	const Vec3d pointonplane = closestPointOnPlane(Vec3d(0,0,0));

	const Vec3d& transformed_point = coordframe.transformPointToLocal(pointonplane);

	const Vec3d& transformed_normal = coordframe.transformVecToLocal(getNormal());

	return Plane(transformed_point, transformed_normal);

	//NOTE: check this is right
}*/


bool Planef::isSpherePartiallyOnFrontSide(const Vec4f& sphere_center, float radius) const
{
	return signedDistToPoint(sphere_center) <= -radius;
}


bool Planef::isSphereTotallyOnFrontSide(const Vec4f& sphere_center, float radius) const
{
	return signedDistToPoint(sphere_center) >= radius;
}


bool Planef::isPlaneOnFrontSide(const Planef& p) const
{
	return epsEqual(p.getNormal(), this->getNormal())
				&& p.getDist() > this->getDist();
}


inline const Vec4f Planef::reflectPointInPlane(const Vec4f& point) const
{
	return point - getNormal() * signedDistToPoint(point) * (float)2.0;
}


inline const Vec4f Planef::reflectVectorInPlane(const Vec4f& vec) const
{
	return vec - getNormal() * dot(getNormal(), vec) * (float)2.0;
}
