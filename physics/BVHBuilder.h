/*=====================================================================
BVHBuilder.h
------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#pragma once


#include "../physics/jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
namespace js { class AABBox; }
namespace Indigo { class TaskManager; }
class PrintOutput;
class ShouldCancelCallback;


struct ResultNode
{
	js::AABBox aabb;
	int left;  // if interior, this is the index of the left child node.  Will be zero if this is the root node of a different chunk.  If Leaf, this is the object begin index.
	int right; // if interior, this is the index of the right child node.  Will be zero if this is the root node of a different chunk.  If Leaf, this is the object end index.
	
	int right_child_chunk_index; // valid if interior: -1 if right child is in the same chunk as this node, otherwise set to the index of the chunk the right child node is in.  For internal builder use.
	bool interior;
	uint8 depth;

	inline void operator = (const ResultNode& other)
	{
		aabb = other.aabb;
		_mm_store_ps((float*)this + 8, _mm_load_ps((float*)&other + 8)); // Copy remaining data members with an SSE load and store for efficiency.
	}
};


struct BVHBuilderTri
{
	GLARE_ALIGNED_16_NEW_DELETE

	Vec4f v[3];
};


// Enables flushing denorms to zero, then restores previous flag values on destruction.
struct SetFlushDenormsMode
{
	SetFlushDenormsMode()
	{
		this->old_flush_zero_mode     = _MM_GET_FLUSH_ZERO_MODE();
		this->old_denormals_zero_mode = _MM_GET_DENORMALS_ZERO_MODE();
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	}
	~SetFlushDenormsMode()
	{
		// Restore old mode
		_MM_SET_FLUSH_ZERO_MODE(old_flush_zero_mode);
		_MM_SET_DENORMALS_ZERO_MODE(old_denormals_zero_mode);
	}

	unsigned int old_flush_zero_mode, old_denormals_zero_mode;
};


/*=====================================================================
BVHBuilder
----------
BVH builder interface.
=====================================================================*/
class BVHBuilder : public RefCounted
{
public:
	virtual ~BVHBuilder();

	// Throws Indigo::CancelledException if cancelled.
	virtual void build(
		Indigo::TaskManager& task_manager,
		ShouldCancelCallback& should_cancel_callback,
		PrintOutput& print_output,
		js::Vector<ResultNode, 64>& result_nodes_out
	) = 0;

	typedef js::Vector<uint32, 16> ResultObIndicesVec;

	virtual const ResultObIndicesVec& getResultObjectIndices() const = 0;

	// Required stack size for traversal of the BVH will be max(1, max_leaf_depth)
	// We will always require at least 1 entry in the stack since the root node is always pushed.
	// We push a node onto the stack at interior nodes, not leaf nodes.
	// Therefore the lowest level of leaf nodes does not contribute to the stack depth,
	// so with e.g. 3 node levels (max depth=2), we require a stack size of 2.
	//
	//
	//     depth
	//       0            A
	//                 /     \
	//       1       B         C
	//             /  \       /  \
	//       2    D    E     F    G
	//
	//  Max size stack will look like
	//
	//       E
	//       C
	//
	// upon traversal to D.
	virtual int getMaxLeafDepth() const = 0; // Root node is considered to have depth 0.

	virtual const js::AABBox getRootAABB() const = 0; // root AABB will have been computed after build() has been called. 

	static float getSAHCost(const js::Vector<ResultNode, 64>& nodes, float intersection_cost);
};


typedef Reference<BVHBuilder> BVHBuilderRef;


GLARE_STRONG_INLINE Vec4f setW(const Vec4f& a, int new_w_val_i)
{
	const Vec4f new_w = bitcastToVec4f(loadIntToLowElem(new_w_val_i)); // [new_w, 0, 0, 0]
	const Vec4f v1 = shuffle<2, 2, 0, 0>(a, new_w); // [z, z, new_w, new_w]
	return shuffle<0, 1, 0, 2>(a, v1); // [x, y, z, new_w]
}

