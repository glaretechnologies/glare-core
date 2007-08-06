/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/

#ifndef __PLANE2_H__
#define __PLANE2_H__


#include "vec2.h"
#include "mathstypes.h"
#include <assert.h>

template <class Real>
class Plane2
{
public:
	inline Plane2();
	inline Plane2(const Vec2<Real>& origin, const Vec2<Real>& normal);
	inline Plane2(const Vec2<Real>& normal, Real dist_from_origin);
	inline Plane2(const Plane2& rhs);
	inline ~Plane2();

//	Plane2& operator = (const Plane2& rhs);

	inline void set(const Vec2<Real>& normal, Real dist_from_origin);
	inline void set(const Vec2<Real>& origin, const Vec2<Real>& normal);

	inline void setUnnormalised(const Vec2<Real>& origin, const Vec2<Real>& nonunit_normal);

	


	inline bool pointOnFrontSide(const Vec2<Real>& point) const;

	inline bool pointOnBackSide(const Vec2<Real>& point) const;

	inline bool isPointOnPlane2(const Vec2<Real>& point) const;

	inline const Vec2<Real> calcOrigin() const;//SLOW!

	inline const Vec2<Real> projOnPlane2(const Vec2<Real>& vec) const;

	inline const Vec2<Real> compNormalToPlane2(const Vec2<Real>& vec) const;

	inline Real signedDistToPoint(const Vec2<Real>& p) const;

	inline const Vec2<Real> closestPointOnPlane2(const Vec2<Real>& p) const;

	inline Real getDist() const { return d; }
	inline Real getD() const { return d; }

	inline const Vec2<Real>& getNormal() const { return normal; }

	//returns fraction of ray travelled. Will be in range [0, 1] if ray hit
	inline Real rayIntersect(const Vec2<Real>& raystart, const Vec2<Real>& rayend) const;

	//returns true if line segment intersected plane
	inline bool getSegIntersectPos(const Vec2<Real>& raystart, const Vec2<Real>& rayend, Vec2<Real>& in_pos_out) const;

	//returns true if line clipped away completely
	inline bool clipLineInPlane(Vec2<Real>& start, Vec2<Real>& end) const;


private:
	Real d;
	Vec2<Real> normal;
};

template <class Real>
Plane2<Real>::Plane2()
{}

template <class Real>
Plane2<Real>::Plane2(const Vec2<Real>& origin, const Vec2<Real>& normal_)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1) );

	d = dot(origin, normal);
}

template <class Real>
Plane2<Real>::Plane2(const Vec2<Real>& normal_, Real dist_from_origin)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1) );

	d = dist_from_origin;
}

template <class Real>
Plane2<Real>::Plane2(const Plane2& rhs)
{
	normal = rhs.normal;
	d = rhs.d;
}

template <class Real>
Plane2<Real>::~Plane2()
{}

/*Plane2& Plane2<Real>::operator = (const Plane2& rhs)
{
	normal = rhs.normal;
	d = rhs.d;

	return *this;
}*/
template <class Real>
void Plane2<Real>::set(const Vec2<Real>& normal_, Real dist_from_origin)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1) );

	d = dist_from_origin;
}



template <class Real>
void Plane2<Real>::set(const Vec2<Real>& origin, const Vec2<Real>& normal_)
{
	normal = normal_;

	assert( epsEqual(normal.length(), 1) );

	d = dot(origin, normal);
}

template <class Real>
void Plane2<Real>::setUnnormalised(const Vec2<Real>& origin, const Vec2<Real>& nonunit_normal)
{
	normal = nonunit_normal;

	normal.normalise();

	d = dot(origin, normal);
}

template <class Real>
bool Plane2<Real>::pointOnFrontSide(const Vec2<Real>& point) const
{
	return (dot(point, normal) >= d);
}

template <class Real>
bool Plane2<Real>::pointOnBackSide(const Vec2<Real>& point) const
{
	return (dot(point, normal) < d);
}

template <class Real>
bool Plane2<Real>::isPointOnPlane2(const Vec2<Real>& point) const 
{
	if(fabs(signedDistToPoint(point)) < 0.00001)
		return true;
	else
		return false;
}


template <class Real>
const Vec2<Real> Plane2<Real>::calcOrigin() const
{
	return normal * d;
}

template <class Real>
const Vec2<Real> Plane2<Real>::projOnPlane2(const Vec2<Real>& vec) const
{
	return vec - compNormalToPlane2(vec);
}

template <class Real>
const Vec2<Real> Plane2<Real>::compNormalToPlane2(const Vec2<Real>& vec) const
{
	return normal * dot(vec, normal);
}

template <class Real>
Real Plane2<Real>::signedDistToPoint(const Vec2<Real>& p) const
{
	return dot(p, normal) - d;
}

template <class Real>
const Vec2<Real> Plane2<Real>::closestPointOnPlane2(const Vec2<Real>& p) const
{
	return p - (normal * signedDistToPoint(p));
}

template <class Real>
Real Plane2<Real>::rayIntersect(const Vec2<Real>& raystart, const Vec2<Real>& rayend) const
{
	const Real raystart_dot_n = dot(raystart, normal);

	const Real rayend_dot_n = dot(rayend, normal);

	const Real denom = rayend_dot_n - raystart_dot_n;

	if(denom == 0)
		return -1;

	return (d - raystart_dot_n) / denom;

}


template <class Real>
bool Plane2<Real>::getSegIntersectPos(const Vec2<Real>& raystart, const Vec2<Real>& rayend, Vec2<Real>& in_pos_out) const
{
	const Real fraction = rayIntersect(raystart, rayend);

	if(fraction > 0 && fraction < 1)
	{
		in_pos_out = rayend;
		in_pos_out.sub(raystart);
		in_pos_out *= fraction;

		in_pos_out += raystart;
		return true;
	}

	return false;
}






//returns true if line clipped away completely
template <class Real>
bool Plane2<Real>::clipLineInPlane(Vec2<Real>& start, Vec2<Real>& end/*, bool& waspartiallyclipped_out*/) const
{

	if(pointOnFrontSide(start))
	{
		if(pointOnFrontSide(end))
		{
			//waspartiallyclipped_out = true;
			return true;
		}

		//const Real fraction_out = dot(start, normal) / (-dot(end, normal) + dot(start, normal));
		//start += (end - start) * fraction_out;
		start += (end-start) * rayIntersect(start, end);

		//waspartiallyclipped_out = true;
		return false;
	}
	

	if(pointOnFrontSide(end))
	{
		if(pointOnFrontSide(start))
		{
			//waspartiallyclipped_out = true;
			return true;
		}

	//	const Real fraction_out = dot(end, normal) / (-dot(start, normal) + dot(end, normal));
	//	end += (start - end) * fraction_out;
		end += (start - end) * rayIntersect(end, start);

		//waspartiallyclipped_out = true;
		return false;
	}

	//waspartiallyclipped_out = false;
	return false;
}










#endif //__PLANE2_H__
