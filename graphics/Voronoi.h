/*=====================================================================
Voronoi.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2013-01-04 17:14:42 +0000
=====================================================================*/
#pragma once


#include "../maths/vec2.h"
#include "../maths/Vec4f.h"


/*=====================================================================
Voronoi
-------------------

=====================================================================*/
namespace Voronoi
{
	
	void evaluate(
		const Vec2f& uv, // input point
		float irregularity, // irregularity: Should be in [0, 1] for best results.  default is 1.
		Vec2f& coords_ret_out, // Closest point found
		float& dist_out // Distance to closest point.
	);

	float voronoiFBM(const Vec2f& p, int octaves);


	void evaluate3d(
		const Vec4f& p, // input point
		float irregularity, // irregularity: Should be in [0, 1] for best results.  default is 1.
		Vec4f& closest_p_out, // Closest point found
		float& dist_out // Distance to closest point.
	);

} // namespace Voronoi
