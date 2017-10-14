/*=====================================================================
Tree.h
------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <istream>
#include <ostream>
#include <vector>
class RayMesh;
class HitInfo;
class FullHitInfo;
class DistanceHitInfo;
class Object;
class ThreadContext;
class PrintOutput;
class Ray;
class Vec4f;
namespace js { class TriTreePerThreadData; };
namespace js { class AABBox; };
namespace Indigo { class TaskManager; }


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
Interface for triangle mesh ray tracing acceleration structures.
=====================================================================*/
class Tree
{
public:
	Tree();
	virtual ~Tree();

	typedef float Real;
	typedef double DistType;

	static const unsigned int MAX_TREE_DEPTH = 63;

	virtual void build(PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager) = 0; // throws TreeExcep

	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const = 0;

	virtual DistType traceSphere(const Ray& ray, float radius, DistType max_t, ThreadContext& thread_context, Vec4f& hit_normal_out) const;

	virtual void appendCollPoints(const Vec4f& sphere_pos, float radius, ThreadContext& thread_context, std::vector<Vec4f>& points_in_out) const;
	
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const = 0;

	virtual const js::AABBox& getAABBoxWS() const = 0;
	//virtual const std::string debugName() const = 0;

	virtual void printStats() const = 0;
	virtual void printTraceStats() const = 0;
		
	virtual size_t getTotalMemUsage() const = 0;

};


} //end namespace js
