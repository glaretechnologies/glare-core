/*=====================================================================
Tree.h
------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <vector>
#include <cstddef> // For size_t
class HitInfo;
class DistanceHitInfo;
class PrintOutput;
class ShouldCancelCallback;
class Ray;
class Vec4f;
class Matrix4f;
namespace js { class AABBox; };
namespace Indigo { class TaskManager; }


namespace js
{


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

	virtual void build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, Indigo::TaskManager& task_manager) = 0; // throws Indigo::Exception

	virtual DistType traceRay(const Ray& ray, HitInfo& hitinfo_out) const = 0;

	virtual DistType traceSphere(const Ray& ray_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_normal_ws_out) const;

	virtual void appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const;
	
	virtual const js::AABBox& getAABBox() const = 0;

	virtual void printStats() const = 0;
	virtual void printTraceStats() const = 0;
		
	virtual size_t getTotalMemUsage() const = 0;
};


} //end namespace js
