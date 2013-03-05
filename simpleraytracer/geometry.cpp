/*=====================================================================
geometry.cpp
------------
File created by ClassTemplate on Wed Apr 14 21:19:37 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "geometry.h"


#include "../maths/vec2.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/globals.h"


/*bool Geometry::doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object) const
{
	HitInfo hitinfo;
	const Real hitdist = traceRay(ray, raylength, thread_context, object, hitinfo);
	
	return hitdist >= 0.0f && hitdist < raylength;
}*/


/*
const Vec2 Geometry::getTexCoords(const FullHitInfo& hitinfo, unsigned int tri_index, float tri_u, float tri_v, unsigned int texcoords_set) const
{
	return Vec2(0,0);
}*/


/*bool Geometry::getTangents(const FullHitInfo& hitinfo, unsigned int texcoord_set, Vec3d& tangent_out, Vec3d& bitangent_out) const
{
	//just use geometric basis that is already calced
	assert(0);
	return false;
}
*/
