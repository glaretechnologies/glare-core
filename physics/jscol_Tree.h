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
class Tree : public Intersectable
{
public:
	/*=====================================================================
	Tree
	----
	
	=====================================================================*/
	Tree();

	virtual ~Tree();

	virtual void build() = 0;
	virtual bool diskCachable() = 0;
	virtual void buildFromStream(std::istream& stream) = 0;
	virtual void saveTree(std::ostream& stream) = 0;
	virtual unsigned int checksum() = 0;


	//intersectable interface
	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const = 0;
	virtual const js::AABBox& getAABBoxWS() const = 0;
	virtual const std::string debugName() const { return "kd-tree"; }
	//end

	virtual void getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const = 0;
	virtual bool doesFiniteRayHit(const ::Ray& ray, double raylength, js::TriTreePerThreadData& context) const = 0;

	virtual const Vec3f& triGeometricNormal(unsigned int tri_index) const = 0;
};





} //end namespace js


#endif //__TREE_H_666_




