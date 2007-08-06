#ifndef __MESHOBJECT_H__
#define __MESHOBJECT_H__


#include "js_object.h"


namespace js
{

class SimpleTriMeshShape;

class MeshObject : public Object
{
public:
	MeshObject(const CoordFrame& coordframe, SimpleTriMeshShape* shape,  bool moveable, float mass);
	virtual ~MeshObject();



	virtual bool lineStabsObject(float& t_out, Vec3& normal_out, const Vec3& raystart, const Vec3& rayend);

	virtual void impulseOS(const Vec3& impulse_point_os, const Vec3& impulse);

	virtual void impulseWS(const Vec3& impulse_point_ws, const Vec3& impulse);


private:
	SimpleTriMeshShape* shape;
};



}//end namespace js
#endif //__MESHOBJECT_H__