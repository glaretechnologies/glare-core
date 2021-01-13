/*=====================================================================
BVHObjectTree.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at 2012-11-10 19:47:31 +0000
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include <vector>
namespace glare { class TaskManager; }
class Object;
class PrintOutput;
class ShouldCancelCallback;
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


	//void insertObject(const Object* object);
	
	// hitob_out will be set to a non-null value if the ray hit somethig in the interval, and null otherwise.
	Real traceRay(const Ray& ray, float time,
		const Object*& hitob_out, HitInfo& hitinfo_out) const;

	// Throws glare::CancelledException if cancelled.
	void build(glare::TaskManager& task_manager, ShouldCancelCallback& should_cancel_callback, PrintOutput& print_output);

	void printStats();

//private:
	int32 root_node_index;
	js::Vector<const Object*, 16> objects;
	js::Vector<BVHObjectTreeNode, 64> nodes;
	js::Vector<const Object*, 16> leaf_objects;


	// stats
	mutable uint64 num_traceray_calls;
	mutable uint64 num_interior_nodes_traversed;
	mutable uint64 num_leaf_nodes_traversed;
	mutable uint64 num_object_traceray_calls;
	mutable uint64 num_hit_opaque_ob_early_outs;
};
