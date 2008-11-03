/*=====================================================================
Tree.h
------
File created by ClassTemplate on Fri Apr 20 22:04:29 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREE_H_666_
#define __TREE_H_666_


#include "jscol_Intersectable.h"
#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include <ostream>
class RayMesh;
class FullHitInfo;
class DistanceHitInfo;
class Object;
class ThreadContext;
namespace js { class TriTreePerThreadData; };

namespace js
{


class TreeExcep
{
public:
	TreeExcep(const std::string& message_) : message(message_) {}
	~TreeExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
Tree
----

=====================================================================*/
class Tree/* : public Intersectable*/
{
public:
	/*=====================================================================
	Tree
	----
	
	=====================================================================*/
	Tree();

	virtual ~Tree();

	static const unsigned int MAX_TREE_DEPTH = 63;

	virtual void build() = 0; // throws TreeExcep
	virtual bool diskCachable() = 0;
	virtual void buildFromStream(std::istream& stream) = 0; // throws TreeExcep
	virtual void saveTree(std::ostream& stream) = 0;
	virtual uint32 checksum() = 0;

	typedef Object OPACITY_TEST_TYPE; // type that implements isOpaque()


	//intersectable interface
	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual const std::string debugName() const { return "kd-tree"; }
	//end

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;
	virtual bool doesFiniteRayHit(const ::Ray& ray, double raylength, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object) const = 0;

	virtual const Vec3f triGeometricNormal(unsigned int tri_index) const = 0;

	virtual void printStats() const = 0;
	virtual void printTraceStats() const = 0;

	//For Debugging:
	//virtual double traceRayAgainstAllTris(const Ray& ray, double max_t, HitInfo& hitinfo_out) const = 0;
	//virtual void getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;

};





} //end namespace js


#endif //__TREE_H_666_




