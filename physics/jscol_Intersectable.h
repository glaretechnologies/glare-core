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
class Ray;
class HitInfo;
class PerThreadData;
namespace js{ class TriTreePerThreadData; }


namespace js
{

class AABBox;

/*=====================================================================
Intersectable
-------------

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

	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual bool doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const = 0;
	//virtual void getAllHits(const Ray& ray, std::vector<FullHitInfo>& hitinfos_out) const = 0;

	virtual const std::string debugName() const = 0;

	//int last_test_time;
	int object_index;
};

} //end namespace js

#endif //__INTERSECTABLE_H_666_





