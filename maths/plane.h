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


template <class Real>
class Plane
{
public:
	inline Plane();
	inline Plane(const Vec3<Real>& origin, const Vec3<Real>& normal);
	inline Plane(const Vec3<Real>& normal, Real dist_from_origin);
	//inline Plane(const Plane& rhs);
	inline ~Plane();

//	Plane& operator = (const Plane& rhs);

	//inline void set(const Vec3<Real>& normal, Real dist_from_origin);
	//inline void set(const Vec3<Real>& origin, const Vec3<Real>& normal);

	//inline void setUnnormalised(const Vec3<Real>& origin, const Vec3<Real>& nonunit_normal);

	


	inline bool pointTouchingFrontHalfSpace(const Vec3<Real>& point) const;

	inline bool pointTouchingBackHalfSpace(const Vec3<Real>& point) const;

	//inline bool isPointOnPlane(const Vec3<Real>& point) const;

	inline const Vec3<Real> calcOrigin() const;//SLOW!

	inline const Vec3<Real> projOnPlane(const Vec3<Real>& vec) const;

	inline const Vec3<Real> compNormalToPlane(const Vec3<Real>& vec) const;

	inline Real signedDistToPoint(const Vec3<Real>& p) const;

	inline const Vec3<Real> closestPointOnPlane(const Vec3<Real>& p) const;

	inline Real getDist() const { return d; }
	inline Real getD() const { return d; }

	inline const Vec3<Real>& getNormal() const { return normal; }

	//returns fraction of ray travelled. Will be in range [0, 1] if ray hit
	inline Real finiteRayIntersect(const Vec3<Real>& raystart, const Vec3<Real>& rayend) const;

	inline Real rayIntersect(const Vec3<Real>& raystart, const Vec3<Real> ray_unitdir) const;

	//NOTE: will return a position even if ray points AWAY from plane.
	inline const Vec3<Real> getRayIntersectPos(const Vec3<Real>& raystart, const Vec3<Real> ray_unitdir) const;

	//const Plane transformToLocal(const CoordFrame& coordframe) const;

	inline bool isSpherePartiallyOnFrontSide(const Vec3<Real>& sphere_center, Real radius) const;
	inline bool isSphereTotallyOnFrontSide(const Vec3<Real>& sphere_center, Real radius) const;

	inline bool isPlaneOnFrontSide(const Plane& p) const;

	inline const Vec3<Real> reflectPointInPlane(const Vec3<Real>& point) const;
	inline const Vec3<Real> reflectVectorInPlane(const Vec3<Real>& vec) const;

private:
	Vec3<Real> normal;
	Real d;
};

template <class Real>
Plane<Real>::Plane()
{}

template <class Real>
Plane<Real>::Plane(const Vec3<Real>& origin, const Vec3<Real>& normal_)
:	normal(normal_),
	d(dot(origin, normal))
{
	//normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	//d = dot(origin, normal);
}

template <class Real>
Plane<Real>::Plane(const Vec3<Real>& normal_, Real d_)
:	normal(normal_),
	d(d_)
{
	//normal = normal_;

	assert( epsEqual(normal.length(), (Real)1.0) );

	//d = dist_from_origin;
}

/*template <class Real>
Plane<Real>::Plane(const Plane& rhs)
{
	normal = rhs.normal;
	d = rhs.d;
}*/

template <class Real>
Plane<Real>::~Plane()
{}

/*Plane& Plane::operator = (const Plane& rhs)
{
	normal = rhs.normal;
	d = rhs.d;

	return *this;
}*/

/*
template <class Real>
void Plane<Real>::set(const Vec3<Real>& normal_, Real dist_from_origin)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dist_from_origin;
}*/


/*
template <class Real>
void Plane<Real>::set(const Vec3<Real>& origin, const Vec3<Real>& normal_)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1.0) );

	d = dot(origin, normal);
}

template <class Real>
void Plane<Real>::setUnnormalised(const Vec3<Real>& origin, const Vec3<Real>& nonunit_normal)
{
	normal = nonunit_normal;

	normal.normalise();

	d = dot(origin, normal);
}*/

template <class Real>
bool Plane<Real>::pointTouchingFrontHalfSpace(const Vec3<Real>& point) const
{
	return dot(point, normal) >= d;
}

template <class Real>
bool Plane<Real>::pointTouchingBackHalfSpace(const Vec3<Real>& point) const
{
	return dot(point, normal) <= d;
}

/*template <class Real>
bool Plane<Real>::isPointOnPlane(const Vec3<Real>& point) const 
{
	if(fabs(signedDistToPoint(point)) < 0.00001f)
		return true;
	else
		return false;
}*/


template <class Real>
const Vec3<Real> Plane<Real>::calcOrigin() const
{
	return normal * d;
}

template <class Real>
const Vec3<Real> Plane<Real>::projOnPlane(const Vec3<Real>& vec) const
{
	return vec - compNormalToPlane(vec);
}

template <class Real>
const Vec3<Real> Plane<Real>::compNormalToPlane(const Vec3<Real>& vec) const
{
	return dot(vec, normal) * normal;
}

template <class Real>
Real Plane<Real>::signedDistToPoint(const Vec3<Real>& p) const
{
	return dot(p, normal) - d;
}

template <class Real>
const Vec3<Real> Plane<Real>::closestPointOnPlane(const Vec3<Real>& p) const
{
	return p - (signedDistToPoint(p) * normal);
}

template <class Real>
Real Plane<Real>::finiteRayIntersect(const Vec3<Real>& raystart, const Vec3<Real>& rayend) const
{
	const Real raystart_dot_n = dot(raystart, normal);

	const Real rayend_dot_n = dot(rayend, normal);

	const Real denom = rayend_dot_n - raystart_dot_n;

	if(denom == 0.0f)
		return -1.0f;

	return (d - raystart_dot_n) / denom;

}

template <class Real>
Real Plane<Real>::rayIntersect(const Vec3<Real>& raystart, const Vec3<Real> ray_unitdir) const
{

	const Real start_to_plane_dist = signedDistToPoint(raystart);

	return start_to_plane_dist / -dot(ray_unitdir, normal);

	//NOTE: deal with div by 0?

}


template <class Real>
const Vec3<Real> Plane<Real>::getRayIntersectPos(const Vec3<Real>& raystart, const Vec3<Real> ray_unitdir) const
{
	const Real dist_till_intersect = rayIntersect(raystart, ray_unitdir);

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

template <class Real>
bool Plane<Real>::isSpherePartiallyOnFrontSide(const Vec3<Real>& sphere_center, Real radius) const
{
	return signedDistToPoint(sphere_center) <= radius * -1.0f;
}

template <class Real>
bool Plane<Real>::isSphereTotallyOnFrontSide(const Vec3<Real>& sphere_center, Real radius) const
{
	return signedDistToPoint(sphere_center) >= radius;
}

template <class Real>
bool Plane<Real>::isPlaneOnFrontSide(const Plane& p) const
{
	return epsEqual(p.getNormal(), this->getNormal())
				&& p.getDist() > this->getDist();
}

template <class Real>
inline const Vec3<Real> Plane<Real>::reflectPointInPlane(const Vec3<Real>& point) const
{
	return point - getNormal() * signedDistToPoint(point) * (Real)2.0;
}

template <class Real>
inline const Vec3<Real> Plane<Real>::reflectVectorInPlane(const Vec3<Real>& vec) const
{
	return vec - getNormal() * dot(getNormal(), vec) * (Real)2.0;
}


#endif //__PLANE_H__
