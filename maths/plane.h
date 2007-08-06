/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman[/ Ono-Sendai]
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __PLANE_H__
#define __PLANE_H__


#include "vec3.h"
#include "mathstypes.h"
//class CoordFrame;

class Plane
{
public:
	inline Plane();
	inline Plane(const Vec3d& origin, const Vec3d& normal);
	inline Plane(const Vec3d& normal, double dist_from_origin);
	inline Plane(const Plane& rhs);
	inline ~Plane();

//	Plane& operator = (const Plane& rhs);

	inline void set(const Vec3d& normal, double dist_from_origin);
	inline void set(const Vec3d& origin, const Vec3d& normal);

	inline void setUnnormalised(const Vec3d& origin, const Vec3d& nonunit_normal);

	


	inline bool pointOnFrontSide(const Vec3d& point) const;

	inline bool pointOnBackSide(const Vec3d& point) const;

	inline bool isPointOnPlane(const Vec3d& point) const;

	inline const Vec3d calcOrigin() const;//SLOW!

	inline const Vec3d projOnPlane(const Vec3d& vec) const;

	inline const Vec3d compNormalToPlane(const Vec3d& vec) const;

	inline double signedDistToPoint(const Vec3d& p) const;

	inline const Vec3d closestPointOnPlane(const Vec3d& p) const;

	inline double getDist() const { return d; }
	inline double getD() const { return d; }

	inline const Vec3d& getNormal() const { return normal; }

	//returns fraction of ray travelled. Will be in range [0, 1] if ray hit
	inline double finiteRayIntersect(const Vec3d& raystart, const Vec3d& rayend) const;

	inline double rayIntersect(const Vec3d& raystart, const Vec3d ray_unitdir) const;

	//NOTE: will return a position even if ray points AWAY from plane.
	inline const Vec3d getRayIntersectPos(const Vec3d& raystart, const Vec3d ray_unitdir) const;

	//const Plane transformToLocal(const CoordFrame& coordframe) const;

	inline bool isSpherePartiallyOnFrontSide(const Vec3d& sphere_center, double radius) const;
	inline bool isSphereTotallyOnFrontSide(const Vec3d& sphere_center, double radius) const;

	inline bool isPlaneOnFrontSide(const Plane& p) const;

	inline const Vec3d reflectPointInPlane(const Vec3d& point) const;
	inline const Vec3d reflectVectorInPlane(const Vec3d& vec) const;

private:
	double d;
	Vec3d normal;
};


Plane::Plane()
{}

Plane::Plane(const Vec3d& origin, const Vec3d& normal_)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dot(origin, normal);
}

Plane::Plane(const Vec3d& normal_, double dist_from_origin)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dist_from_origin;
}

Plane::Plane(const Plane& rhs)
{
	normal = rhs.normal;
	d = rhs.d;
}

Plane::~Plane()
{}

/*Plane& Plane::operator = (const Plane& rhs)
{
	normal = rhs.normal;
	d = rhs.d;

	return *this;
}*/
void Plane::set(const Vec3d& normal_, double dist_from_origin)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dist_from_origin;
}



void Plane::set(const Vec3d& origin, const Vec3d& normal_)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dot(origin, normal);
}

void Plane::setUnnormalised(const Vec3d& origin, const Vec3d& nonunit_normal)
{
	normal = nonunit_normal;

	normal.normalise();

	d = dot(origin, normal);
}

bool Plane::pointOnFrontSide(const Vec3d& point) const
{
	return (dot(point, normal) >= d);
}

bool Plane::pointOnBackSide(const Vec3d& point) const
{
	return (dot(point, normal) < d);
}

bool Plane::isPointOnPlane(const Vec3d& point) const 
{
	if(fabs(signedDistToPoint(point)) < 0.00001f)
		return true;
	else
		return false;
}


const Vec3d Plane::calcOrigin() const
{
	return normal * d;
}

const Vec3d Plane::projOnPlane(const Vec3d& vec) const
{
	return vec - compNormalToPlane(vec);
}

const Vec3d Plane::compNormalToPlane(const Vec3d& vec) const
{
	return dot(vec, normal) * normal;
}

double Plane::signedDistToPoint(const Vec3d& p) const
{
	return dot(p, normal) - d;
}

const Vec3d Plane::closestPointOnPlane(const Vec3d& p) const
{
	return p - (signedDistToPoint(p) * normal);
}

double Plane::finiteRayIntersect(const Vec3d& raystart, const Vec3d& rayend) const
{
	const double raystart_dot_n = dot(raystart, normal);

	const double rayend_dot_n = dot(rayend, normal);

	const double denom = rayend_dot_n - raystart_dot_n;

	if(denom == 0.0f)
		return -1.0f;

	return (d - raystart_dot_n) / denom;

}

double Plane::rayIntersect(const Vec3d& raystart, const Vec3d ray_unitdir) const
{

	const double start_to_plane_dist = signedDistToPoint(raystart);

	return start_to_plane_dist / -dot(ray_unitdir, normal);

	//NOTE: deal with div by 0?

}


const Vec3d Plane::getRayIntersectPos(const Vec3d& raystart, const Vec3d ray_unitdir) const
{
	const double dist_till_intersect = rayIntersect(raystart, ray_unitdir);

	return raystart + ray_unitdir * dist_till_intersect;
}


/*
const Plane Plane::transformToLocal(const CoordFrame& coordframe) const
{
	const Vec3d pointonplane = closestPointOnPlane(Vec3d(0,0,0));

	const Vec3d& transformed_point = coordframe.transformPointToLocal(pointonplane);

	const Vec3d& transformed_normal = coordframe.transformVecToLocal(getNormal());

	return Plane(transformed_point, transformed_normal);

	//NOTE: check this is right
}*/

bool Plane::isSpherePartiallyOnFrontSide(const Vec3d& sphere_center, double radius) const
{
	return signedDistToPoint(sphere_center) <= radius * -1.0f;
}

bool Plane::isSphereTotallyOnFrontSide(const Vec3d& sphere_center, double radius) const
{
	return signedDistToPoint(sphere_center) >= radius;
}

bool Plane::isPlaneOnFrontSide(const Plane& p) const
{
	return epsEqual(p.getNormal(), this->getNormal())
				&& p.getDist() > this->getDist();
}

inline const Vec3d Plane::reflectPointInPlane(const Vec3d& point) const
{
	return point - getNormal() * signedDistToPoint(point) * 2.0f;
}

inline const Vec3d Plane::reflectVectorInPlane(const Vec3d& vec) const
{
	return vec - getNormal() * dot(getNormal(), vec) * 2.0f;
}


#endif //__PLANE_H__
