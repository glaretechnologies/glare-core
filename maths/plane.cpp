#include "plane.h"

/*
#include "coordframe.h"

const Plane Plane::transformToLocal(const CoordFrame& coordframe) const
{
	const Vec3 pointonplane = closestPointOnPlane(Vec3(0,0,0));

	const Vec3& transformed_point = coordframe.transformPointToLocal(pointonplane);

	const Vec3& transformed_normal = coordframe.transformVecToLocal(getNormal());

	return Plane(transformed_point, transformed_normal);

	//NOTE: check this is right
}

*/
