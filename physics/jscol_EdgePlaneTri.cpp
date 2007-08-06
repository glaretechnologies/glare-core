/*=====================================================================
EdgePlaneTri.cpp
----------------
File created by ClassTemplate on Mon Jun 27 00:20:26 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_EdgePlaneTri.h"




namespace js
{




EdgePlaneTri::EdgePlaneTri()
{
	assert(sizeof(EdgePlaneTri) == 64);
}


EdgePlaneTri::~EdgePlaneTri()
{
	
}

void EdgePlaneTri::set(const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
	const Vec3 normal_ = normalise(::crossProduct(v1-v0, v2-v0));
	const float dist_ = normal_.dot(v0);

	set(normal_, dist_, v0, v1, v2);
}

void EdgePlaneTri::set(const Vec3& normal_, float dist_, const Vec3& v0, const Vec3& v1, const Vec3& v2)
{
	normal = normal_;
	dist = dist_;

	e0normal = normalise(::crossProduct(v1 - v0, normal));
	e0dist = e0normal.dot(v0);

	e1normal = normalise(::crossProduct(v2 - v1, normal));
	e1dist = e1normal.dot(v1);
	
	e2normal = normalise(::crossProduct(v0 - v2, normal));
	e2dist = e2normal.dot(v2);
}





} //end namespace js

















