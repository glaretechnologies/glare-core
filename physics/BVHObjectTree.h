/*=====================================================================
BVHObjectTree.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-10 19:47:31 +0000
=====================================================================*/
#pragma once


#include "../utils/platform.h"
#include "../utils/Vector.h"
#include "../maths/Vec4f.h"
#include <vector>
class Object;
class PrintOutput;
class ThreadContext;
class HitInfo;
class Ray;


class BVHObjectTreeNode
{
public:
	Vec4f x; // (left_min_x, right_min_x, left_max_x, right_max_x)
	Vec4f y; // (left_min_y, right_min_y, left_max_y, right_max_y)
	Vec4f z; // (left_min_z, right_min_z, left_max_z, right_max_z)

	int32 child[2];
	int32 padding[2]; // Pad to 64 bytes.
};


/*=====================================================================
BVHObjectTree
-------------------

=====================================================================*/
class BVHObjectTree
{
public:
	BVHObjectTree();
	~BVHObjectTree();

	friend class BVHObjectTreeCallBack;

	typedef float Real;


	void insertObject(const Object* object);
	

	Real traceRay(const Ray& ray, ThreadContext& thread_context, double time, 
		const Object* last_object_hit,
		unsigned int last_triangle_hit,
		const Object*& hitob_out, HitInfo& hitinfo_out) const;

	bool doesFiniteRayHit(const Ray& ray, Real length, ThreadContext& thread_context, double time, const Object* ignore_object, unsigned int ignore_tri) const;

	void build(PrintOutput& print_output, bool verbose);

private:
	int32 root_node_index;
	js::Vector<const Object*, 16> objects;
	js::Vector<BVHObjectTreeNode, 64> nodes;
	js::Vector<const Object*, 16> leaf_objects;
};
