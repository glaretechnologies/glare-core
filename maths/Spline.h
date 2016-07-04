/*=====================================================================
Spline.h
--------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once

#include "../utils/ThreadSafeRefCounted.h"
#include <vector>
#include <algorithm>
#include <assert.h>


/*=====================================================================
Spline
------
A cubic Hermite spline.
Reference: http://en.wikipedia.org/wiki/Cubic_Hermite_spline#Interpolating_a_data_set

This class is unfortunately quite inefficient for many calls to inserPoint and removePoint.

TODO Make it work with < 2 data points (constant and 0).
=====================================================================*/
template<typename real_type, typename vec_type>
class Spline : public ThreadSafeRefCounted
{
public:
	Spline() { }
	Spline(const std::vector<real_type> & knots_, const std::vector<vec_type> & points_) { init(knots_, points_); }
	~Spline() {	}


	bool init(const std::vector<real_type> & knots_, const std::vector<vec_type> & points_)
	{
		const size_t n = knots_.size();
		if(n != points_.size()) return false; // Different number of knots and points
		if(n < 2) return false; // No constant values

		// Create a re-mapping of knots to ensure they are in sorted order
		std::vector<std::pair<float, int>> knot_remap(n);
		for(int i = 0; i < (int)n; ++i)
			knot_remap[i] = std::make_pair(knots_[i], i);
		std::stable_sort(knot_remap.begin(), knot_remap.end());

		// Create the sorted knots and points
		knots.reserve(n); knots.resize(0);
		std::vector<vec_type> points(n); points.resize(0);
		for(size_t i = 0; i < n; ++i)
		{
			const int remap_idx = knot_remap[i].second;
			const float curr_knot = knots_[remap_idx];
			knots.push_back(curr_knot);
			points.push_back(points_[remap_idx]);

			while((i + 1) < n && knots_[knot_remap[i + 1].second] <= curr_knot) ++i; // Skip subsequent knots with equal paremeters
		}

		const size_t final_n = knots.size();
		point_tangent_pairs.resize(final_n);

		// Special case for first knot
		point_tangent_pairs[0] = std::make_pair(points[0], (points[1] - points[0]) / (knots[1] - knots[0]));

		for(size_t i = 1; i < final_n - 1; ++i)
		{
			const vec_type a = (points[i + 1] - points[i]) / (2 * (knots[i + 1] - knots[i]));
			const vec_type b = (points[i] - points[i - 1]) / (2 * (knots[i] - knots[i - 1]));

			point_tangent_pairs[i] = std::make_pair(points[i], a + b);
		}

		// Special case for last knot
		point_tangent_pairs[final_n - 1] = std::make_pair(points[final_n - 1], (points[final_n - 1] - points[final_n - 2]) / (knots[final_n - 1] - knots[final_n - 2]));

		return true;
	}


	bool insertPoint(real_type knot_, vec_type val_)
	{
		const size_t prev_num_knots = knots.size();
		std::vector<real_type> new_knots(prev_num_knots + 1);
		std::vector<vec_type> new_points(prev_num_knots + 1);

		for(size_t i = 0; i < prev_num_knots; ++i)
		{
			new_knots[i] = knots[i];
			new_points[i] = point_tangent_pairs[i].second;
		}

		// Append the new knot and point at the end of the vector (gets sorted later)
		new_knots[prev_num_knots] = knot_;
		new_points[prev_num_knots] = val_;

		return init(new_knots, new_points);
	}


	bool removePoint(real_type knot_, vec_type val_)
	{
		const size_t n = knots.size();
		if(n <= 2) return false; // We only support N >= 2 points

		const size_t prev_num_knots = n;
		const size_t new_num_knots = n - 1;

		std::vector<real_type> new_knots(new_num_knots);
		std::vector<vec_type> new_points(new_num_knots);
		for(size_t i = 0, j = 0; i < prev_num_knots; ++i)
		{
			if(knots[j] != knot_)
			{
				if(j >= prev_num_knots) { return false; } // Point not found

				new_knots[j] = point_tangent_pairs[i].second;
				++j;
			}
			else continue;
		}

		return init(new_knots, new_points);
	}


	int baseKnotIndex(const real_type t) const
	{
		if(t <= knots[0]) return 0;

		typename std::vector<real_type>::const_iterator iter = std::upper_bound(knots.begin(), knots.end(), t);
		return (int)std::min(knots.size() - 2, iter - knots.begin() - 1);
	}


	vec_type evaluate(const real_type t) const
	{
		const size_t n = knots.size();
		//if(n == 0) return 0;

		// Early exit for eval outside of range (where it simply returns the closest data point)
		if(t <= knots[0])     return point_tangent_pairs[0].first;
		if(t >= knots[n - 1]) return point_tangent_pairs[n - 1].first;

		const size_t i1 = std::upper_bound(knots.begin(), knots.end(), t) - knots.begin();

		const real_type t0 = knots[i1 - 1]; assert(t >= t0);
		const real_type t1 = knots[i1 - 0]; assert(t <  t1); assert(t1 > t0);
		const real_type dt = t1 - t0;

		//if(dt > 1e-5f)
		{
			const real_type u  = (t - t0) / dt; // u is normalised in [0, 1]
			const real_type u2 = u * u;
			const real_type u3 = u2 * u;

			// Cubic Hermite spline
			return
				point_tangent_pairs[i1 - 1].first * ( 2 * u3 - 3 * u2 + 1) + point_tangent_pairs[i1 - 1].second * ((u3 - 2 * u2 + u) * dt) +
				point_tangent_pairs[i1 - 0].first * (-2 * u3 + 3 * u2) +     point_tangent_pairs[i1 - 0].second * ((u3 - u2) * dt);
		}
		//else
		//{
		//	return point_tangent_pairs[i1 - 1].first;
		//}
	}


	bool isIdentity() const
	{
		if(knots.size() != 5 || point_tangent_pairs.size() != 5) return false;

		for(int i = 0; i < 5; ++i)
		{
			const float v = i * 0.25f;
			if(knots[i] != v || point_tangent_pairs[i].first != v)
				return false;
		}

		return true;
	}

//private:
	typedef std::pair<vec_type, vec_type> PointTangentPair;

	// These are expected to correspond 1:1, and are kept separate for better cache locality in the knot vector
	std::vector<real_type> knots;
	std::vector<PointTangentPair> point_tangent_pairs;
};
