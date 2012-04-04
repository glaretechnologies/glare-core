/*=====================================================================
Numeric.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-03-27 17:29:07 +0100
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Vec4f.h"


/*=====================================================================
Numeric
-------------------
Some utility methods for numerical analysis.
=====================================================================*/
namespace Numeric
{


	template <class R> inline R L1Norm(const Vec3<R>& v);
	inline float L1Norm(const Vec4f& v);


	template <class R>
	R L1Norm(const Vec3<R>& v)
	{
		return std::fabs(v.x) + std::fabs(v.y) + std::fabs(v.z);
	}


	float L1Norm(const Vec4f& v)
	{
		return std::fabs(v.x[0]) + std::fabs(v.x[1]) + std::fabs(v.x[2]);
	}
}
