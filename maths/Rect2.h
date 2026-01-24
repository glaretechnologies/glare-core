/*=====================================================================
Rect2.h
-------
File created by ClassTemplate on Sun Jan 13 17:00:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "vec2.h"
#include <assert.h>


/*=====================================================================
Rect2
-----

=====================================================================*/
template <class Real>
class Rect2
{
public:
	/*=====================================================================
	Rect2
	-----
	
	=====================================================================*/
	inline Rect2(){}
	inline Rect2(const Vec2<Real>& min, const Vec2<Real>& max);

	inline const Vec2<Real>& getMin() const { return min; }
	inline const Vec2<Real>& getMax() const { return max; }

	inline bool inOpenRectangle(const Vec2<Real>& p) const;// does p lie in [min, max]?
	inline bool inHalfClosedRectangle(const Vec2<Real>& p) const; // does p lie in [min, max)?
	inline bool inClosedRectangle(const Vec2<Real>& p) const; // does p lie in (min, max)?

	inline const Vec2<Real> getWidths() const;
	inline Real area() const { return (max.x - min.x) * (max.y - min.y); }

	inline void enlargeToHoldPoint(const Vec2<Real>& p);
	inline void enlargeToHoldRect(const Rect2<Real>& p);

	// TODO: tests for these
	inline bool disjoint(const Rect2<Real>& other) const;
	inline bool intersectsRect2(const Rect2<Real>& other) const;

private:
	Vec2<Real> min;
	Vec2<Real> max;
};


template <class Real>
Rect2<Real>::Rect2(const Vec2<Real>& min_, const Vec2<Real>& max_)
:	min(min_), max(max_)
{
	assert(getWidths().x >= 0.0 && getWidths().y >= 0.0);
}


template <class Real>
bool Rect2<Real>::inOpenRectangle(const Vec2<Real>& p) const // does p lie in [min, max]?
{
	return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

template <class Real>
bool Rect2<Real>::inHalfClosedRectangle(const Vec2<Real>& p) const // does p lie in [min, max)?
{
	return p.x >= min.x && p.x < max.x && p.y >= min.y && p.y < max.y;
}

template <class Real>
bool Rect2<Real>::inClosedRectangle(const Vec2<Real>& p) const // does p lie in (min, max)?
{
	return p.x > min.x && p.x < max.x && p.y > min.y && p.y < max.y;
}

template <class Real>
const Vec2<Real> Rect2<Real>::getWidths() const
{
	return max - min;
}

template <class Real>
void Rect2<Real>::enlargeToHoldPoint(const Vec2<Real>& p)
{
	min.x = myMin(min.x, p.x);
	min.y = myMin(min.y, p.y);
	max.x = myMax(max.x, p.x);
	max.y = myMax(max.y, p.y);
}


template<class Real>
inline void Rect2<Real>::enlargeToHoldRect(const Rect2<Real>& other)
{
	min.x = myMin(min.x, other.min.x);
	min.y = myMin(min.y, other.min.y);
	max.x = myMax(max.x, other.max.x);
	max.y = myMax(max.y, other.max.y);
}


template <class Real>
bool Rect2<Real>::disjoint(const Rect2<Real>& other) const
{
	return max.x < other.min.x || max.y < other.min.y ||
		other.max.x < min.x || other.max.y < min.y;
}


template <class Real>
bool Rect2<Real>::intersectsRect2(const Rect2<Real>& other) const
{
	return !disjoint(other);
}


template <class Real>
Rect2<Real> intersection(const Rect2<Real>& a, const Rect2<Real>& b)
{
	return Rect2<Real>(max(a.getMin(), b.getMin()), min(a.getMax(), b.getMax()));
}


template <class Real>
bool operator == (const Rect2<Real>& a, const Rect2<Real>& b)
{
	return (a.getMin() == b.getMin()) && (a.getMax() == b.getMax());
}


template <class Real>
bool operator != (const Rect2<Real>& a, const Rect2<Real>& b)
{
	return (a.getMin() != b.getMin()) || (a.getMax() != b.getMax());
}


typedef Rect2<float> Rect2f;
typedef Rect2<double> Rect2d;
typedef Rect2<int> Rect2i;
