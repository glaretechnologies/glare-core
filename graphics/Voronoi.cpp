/*=====================================================================
Voronoi.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
Generated at 2013-01-04 17:14:42 +0000
=====================================================================*/
#include "Voronoi.h"


#include "GridNoise.h"


namespace Voronoi
{


static inline Vec2f getPoint(int x, int y, float irregularity)
{
	// Generate random values in range [-0.5, 0.5]
	float x_offset = -0.5f + GridNoise::eval(x, y, 0);
	float y_offset = -0.5f + GridNoise::eval(x, y, 1);

	return Vec2f(
		x + 0.5f + x_offset * irregularity,
		y + 0.5f + y_offset * irregularity
	);
}


void evaluate(const Vec2f& uv, float irregularity, Vec2f& coords_ret_out, float& dist_out)
{
	const int fx = (int)floor(uv.x);
	const int fy = (int)floor(uv.y);

	Vec2f points[9];
	points[0] = getPoint(fx-1, fy-1, irregularity);
	points[1] = getPoint(fx,   fy-1, irregularity);
	points[2] = getPoint(fx+1, fy-1, irregularity);
	points[3] = getPoint(fx-1, fy  , irregularity);
	points[4] = getPoint(fx,   fy  , irregularity);
	points[5] = getPoint(fx+1, fy  , irregularity);
	points[6] = getPoint(fx-1, fy+1, irregularity);
	points[7] = getPoint(fx,   fy+1, irregularity);
	points[8] = getPoint(fx+1, fy+1, irregularity);

	Vec2f best_point(0, 0);
	float least_d2 = std::numeric_limits<float>::max();
	for(int i=0; i<9; ++i)
	{
		float d2 = uv.getDist2(points[i]);
		if(d2 < least_d2)
		{
			least_d2 = d2;
			best_point = points[i];
		}
	}

	coords_ret_out = best_point;
	dist_out = std::sqrt(least_d2);
}


static inline Vec4f getPoint3d(int x, int y, int z, float irregularity)
{
	// Generate random values in range [-0.5, 0.5]
	float x_offset = -0.5f + GridNoise::eval(x, y, z, 0);
	float y_offset = -0.5f + GridNoise::eval(x, y, z, 1);
	float z_offset = -0.5f + GridNoise::eval(x, y, z, 2);

	return Vec4f(
		x + 0.5f + x_offset * irregularity,
		y + 0.5f + y_offset * irregularity,
		z + 0.5f + z_offset * irregularity,
		1
	);
}


void evaluate3d(
		const Vec4f& p, // input point
		float irregularity, // irregularity: Should be in [0, 1] for best results.  default is 1.
		Vec4f& closest_p_out, // Closest point found
		float& dist_out // Distance to closest point.
	)
{
	const int fx = (int)floor(p.x[0]);
	const int fy = (int)floor(p.x[1]);
	const int fz = (int)floor(p.x[2]);

	Vec4f best_point;
	float least_d2 = std::numeric_limits<float>::max();
	for(int x=-1; x<2; ++x)
	for(int y=-1; y<2; ++y)
	for(int z=-1; z<2; ++z)
	{
		Vec4f v_p = getPoint3d(fx + x, fy + y, fz + z, irregularity);

		float d2 = p.getDist2(v_p);
		if(d2 < least_d2)
		{
			least_d2 = d2;
			best_point = v_p;
		}
	}

	closest_p_out = best_point;
	dist_out = std::sqrt(least_d2);
}


} // end namespace Voronoi
