/*#include "js_meshobject.h"

#include "jscol_simpletrimeshshape.h"

js::MeshObject::MeshObject(const CoordFrame& coordframe, SimpleTriMeshShape* shape_, bool moveable, float mass)
:	Object(coordframe, moveable, mass)
{
	shape = shape_;

}

js::MeshObject::~MeshObject()
{}



bool js::MeshObject::lineStabsObject(float& t_out, Vec3& normal_out, const Vec3& raystart_ws, const Vec3& rayend_ws)
{
	//-----------------------------------------------------------------
	//translate ray to object space
	//-----------------------------------------------------------------
	Vec3 raystart_os = corestate.mat.transformPointToLocal(raystart_ws);
	Vec3 rayend_os = corestate.mat.transformPointToLocal(rayend_ws);

	//-----------------------------------------------------------------
	//apply ray to the mesh collision surface
	//-----------------------------------------------------------------
	bool hitsurface = shape->lineStabsShape(t_out, normal_out, raystart_os, rayend_os);

	return hitsurface;
}

void js::MeshObject::impulseOS(const Vec3& impulse_point_os, const Vec3& impulse)
{}

void js::MeshObject::impulseWS(const Vec3& impulse_point_ws, const Vec3& impulse)
{}*/