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
	float least_d2 = 100000000000.0f;
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


} // end namespace Voronoi
