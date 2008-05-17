/*=====================================================================
Intersectable.h
---------------
File created by ClassTemplate on Sat Apr 15 19:01:14 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INTERSECTABLE_H_666_
#define __INTERSECTABLE_H_666_

#include <vector>
#include <string>
#include <assert.h>
class Ray;
class HitInfo;
class PerThreadData;
class Object;
//namespace js{ class TriTreePerThreadData; }
namespace js{ class ObjectTreePerThreadData; }


namespace js
{

class AABBox;

/*=====================================================================
Intersectable
-------------
This is the type of objects that can be inserted into the object tree.
=====================================================================*/
class Intersectable
{
public:
	/*=====================================================================
	Intersectable
	-------------
	
	=====================================================================*/
	Intersectable();

	virtual ~Intersectable(){}

	virtual double traceRay(const Ray& ray, double max_t, js::ObjectTreePerThreadData& context, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::ObjectTreePerThreadData& context) const = 0;
	//virtual void getAllHits(const Ray& ray, std::vector<FullHitInfo>& hitinfos_out) const = 0;

	virtual const std::string debugName() const = 0;

	void setObjectIndex(int i);
	inline int getObjectIndex() const { assert(object_index >= 0); return object_index; }

private:
	//int last_test_time;
	int object_index;
};

} //end namespace js

#endif //__INTERSECTABLE_H_666_





