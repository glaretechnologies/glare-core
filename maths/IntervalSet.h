/*=====================================================================
IntervalSet.h
-------------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


#include "mathstypes.h"
#include "vec2.h"
#include <limits>
#include <vector>
#include <algorithm>


template <class T> inline T intervalSetMinValue();
template <class T> inline T intervalSetMaxValue();

template<> inline int intervalSetMinValue<int>() { return std::numeric_limits<int>::min(); }
template<> inline int intervalSetMaxValue<int>() { return std::numeric_limits<int>::max(); }
template<> inline float intervalSetMinValue<float>() { return -std::numeric_limits<float>::infinity(); }
template<> inline float intervalSetMaxValue<float>() { return std::numeric_limits<float>::infinity(); }


// A list of non-overlapping intervals
template <class T>
class IntervalSet
{
public:
	IntervalSet() : intervals(1, Vec2<T>(intervalSetMinValue<T>(), intervalSetMaxValue<T>())) {}
	IntervalSet(T lower, T upper) : intervals(1, Vec2<T>(lower, upper)) {}
	IntervalSet(const Vec2<T>& int_a, const Vec2<T>& int_b)
	:	intervals(2)
	{
		intervals[0] = int_a;
		intervals[1] = int_b;
	}
	IntervalSet(const Vec2<T>& int_a, const Vec2<T>& int_b, const Vec2<T>& int_c)
	:	intervals(3)
	{
		intervals[0] = int_a;
		intervals[1] = int_b;
		intervals[2] = int_c;
	}


	inline T lower() const; // Lower bound (inclusive).
	inline T upper() const; // Upper bound (inclusive).

	inline bool includesValue(T x) const;


	static IntervalSet<T> emptySet() { return IntervalSet<T>(std::numeric_limits<T>::max(), std::numeric_limits<T>::min()); }

	

	std::vector<Vec2<T> > intervals;
};


void intervalSetTest();


typedef IntervalSet<int> IntervalSetInt;
typedef IntervalSet<float> IntervalSetFloat;


template <class T>
inline bool operator == (const IntervalSet<T>& a, const IntervalSet<T>& b)
{
	return a.intervals == b.intervals;
}


template <class T>
IntervalSet<T> intervalWithHole(T hole)
{
	if(hole == intervalSetMinValue<T>())
		return IntervalSet<T>(hole + 1, intervalSetMaxValue<T>());
	else if(hole == intervalSetMaxValue<T>())
		return IntervalSet<T>(intervalSetMinValue<T>(), hole - 1);
	else
		return IntervalSet<T>(Vec2<T>(intervalSetMinValue<T>(), hole - 1), Vec2<T>(hole + 1, intervalSetMaxValue<T>()));;
}


template <class T>
class IntervalEvent
{
public:
	IntervalEvent(T x_, bool is_a_, bool is_lower_) : x(x_), is_a(is_a_), is_lower(is_lower_) {}

	bool operator < (const IntervalEvent<T>& other) const
	{
		if(x < other.x)
			return true;
		else if(x > other.x)
			return false;
		else
		{
			if(is_lower)
			{
				if(other.is_lower)
					return is_a;
				else
					return true; // Other is upper bound.  We want lower bounds to come first.
			}
			else // Else this is an upper bound
			{
				if(other.is_lower)
					return false; // We want lower bounds to come first.
				else
				{
					assert(!is_lower && !other.is_lower);
					return is_a;
				}
			}
		}
	}

	T x;
	bool is_a;
	bool is_lower;
};


template <class T>
inline IntervalSet<T> intervalSetIntersection(const IntervalSet<T>& a, const IntervalSet<T>& b)
{
	IntervalSet<T> res;
	res.intervals.resize(0);

	std::vector<IntervalEvent<T> > events;
	//events.reserve((a.intervals.size() + b.intervals.size()) * 2);

	for(size_t i=0; i<a.intervals.size(); ++i)
	{
		events.push_back(IntervalEvent<T>(a.intervals[i].x, true, true));
		events.push_back(IntervalEvent<T>(a.intervals[i].y, true, false));
	}

	for(size_t i=0; i<b.intervals.size(); ++i)
	{
		events.push_back(IntervalEvent<T>(b.intervals[i].x, false, true));
		events.push_back(IntervalEvent<T>(b.intervals[i].y, false, false));
	}

	std::sort(events.begin(), events.end());

	bool in_a = false;
	bool in_b = false;
	T interval_start;
	for(size_t i=0; i<events.size(); ++i)
	{
		if(events[i].is_lower) // If we are on the lower bound of an interval
		{
			if(events[i].is_a)
				in_a = true;
			else
				in_b = true;

			if(in_a && in_b)
				interval_start = events[i].x;
		}
		else // We are at the upper bound of an interval
		{
			if(events[i].is_a)
			{
				if(in_b)
					res.intervals.push_back(Vec2<T>(interval_start, events[i].x)); // We are no longer in both a and b, so this is the end of the interval.
				in_a = false;
			}
			else
			{
				if(in_a)
					res.intervals.push_back(Vec2<T>(interval_start, events[i].x)); // We are no longer in both a and b, so this is the end of the interval.
				in_b = false;
			}
		}
	}

	if(res.intervals.empty())
		return IntervalSet<T>::emptySet();
	else
		return res;
}


// NOTE: untested
template <class T>
inline IntervalSet<T> intervalSetUnion(const IntervalSet<T>& a, const IntervalSet<T>& b)
{
	IntervalSet<T> res;
	res.intervals.resize(0);

	std::vector<IntervalEvent<T> > events;

	for(size_t i=0; i<a.intervals.size(); ++i)
	{
		events.push_back(IntervalEvent(a.intervals[i].x, true, true));
		events.push_back(IntervalEvent(a.intervals[i].y, true, false));
	}

	for(size_t i=0; i<b.intervals.size(); ++i)
	{
		events.push_back(IntervalEvent(b.intervals[i].x, false, true));
		events.push_back(IntervalEvent(b.intervals[i].y, false, false));
	}

	std::sort(events);

	bool in_a = false;
	bool in_b = false;
	T interval_start;
	for(size_t i=0; i<events.size(); ++i)
	{
		if(events[i].is_lower) // If we are on the lower bound of an interval
		{
			if(events[i].is_a) // this is the lower bound of an 'a' interval
			{
				if(!in_b) // If we were not in a 'b' interval
					interval_start = events[i].x;
				in_a = true;
			}
			else //  else this is the lower bound of an 'a' interval
			{
				if(!in_a) // If we were not in an 'a' interval
					interval_start = events[i].x;
				in_b = true;
			}
		}
		else // We are at the upper bound of an interval
		{
			if(events[i].is_a) // if at the end of 'a':
			{
				if(!in_b)
					res.intervals.push_back(Vec2<T>(interval_start, events[i].x)); // We are no longer in a or b, so this is the end of the interval.
				in_a = false;
			}
			else // Else if at the end of 'b':
			{
				if(!in_a)
					res.intervals.push_back(Vec2<T>(interval_start, events[i].x)); // We are no longer in a or b, so this is the end of the interval.
				in_b = false;
			}
		}
	}

	if(res.intervals.empty())
		return emptySet();
	else
		return res;
}


template <class T>
T IntervalSet<T>::lower() const
{
	return intervals[0].x;
}


template <class T>
T IntervalSet<T>::upper() const
{
	return intervals.back().y;
}


template <class T>
bool IntervalSet<T>::includesValue(T x) const
{
	for(size_t i=0; i<intervals.size(); ++i)
		if(x >= intervals[i].x && x <= intervals[i].y)
			return true;
	return false;
}
