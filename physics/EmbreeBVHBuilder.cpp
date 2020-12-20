/*=====================================================================
EmbreeBVHBuilder.cpp
--------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "EmbreeBVHBuilder.h"


#include "jscol_aabbox.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/ProfilerStore.h"
#include "../utils/EmbreeDeviceHandle.h"


static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


static void embreeErrorCallback(void* userPtr, enum RTCError code, const char* str)
{
	conPrint("EMBREE ERROR: " + std::string(str));
	throw Indigo::Exception("EMBREE ERROR: " + std::string(str));
}


static bool embreeMemoryMonitorFunction(void* user_ptr, ssize_t bytes, bool post)
{
	EmbreeBVHBuilder* builder = (EmbreeBVHBuilder*)user_ptr;
	builder->embree_mem_usage += bytes;
	builder->embree_max_mem_usage = myMax(builder->embree_max_mem_usage, builder->embree_mem_usage);
	return true; // Continue normally
}


EmbreeBVHBuilder::EmbreeBVHBuilder(bool do_high_quality_build_, int max_num_objects_per_leaf_, float intersection_cost_,
	const BVHBuilderTri* triangles_,
	const int num_objects_
)
:	do_high_quality_build(do_high_quality_build_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_),
	should_cancel_callback(NULL)
{
	assert(intersection_cost > 0.f);

	triangles = triangles_;
	m_num_objects = num_objects_;

	this->embree_device = rtcNewDevice("verbose=0,frequency_level=simd128");

	rtcSetDeviceMemoryMonitorFunction(this->embree_device, embreeMemoryMonitorFunction, /*user ptr=*/this);

	rtcSetDeviceErrorFunction(this->embree_device, embreeErrorCallback, /*user ptr=*/NULL);
}


EmbreeBVHBuilder::~EmbreeBVHBuilder()
{
	if(embree_device) rtcReleaseDevice(embree_device);
}


struct EmbreeBVHBuilderNode
{
	bool interior;
};


struct EmbreeBVHBuilderInnerNode : public EmbreeBVHBuilderNode
{
	js::AABBox bounds[2];
	const EmbreeBVHBuilderNode* children[2];

	EmbreeBVHBuilderInnerNode()
	{
		this->interior = true;
		bounds[0] = bounds[1] = js::AABBox::emptyAABBox();
		children[0] = children[1] = nullptr;
	}

	static void* create (RTCThreadLocalAllocator alloc, unsigned int numChildren, void* userPtr)
	{
		assert(numChildren == 2);
		void* ptr = rtcThreadLocalAlloc(alloc, sizeof(EmbreeBVHBuilderInnerNode), 16);
		return (void*) new (ptr) EmbreeBVHBuilderInnerNode;
	}

	static void setChildren (void* nodePtr, void** childPtr, unsigned int numChildren, void* userPtr)
	{
		assert(numChildren == 2);
		for (size_t i=0; i<2; i++)
			((EmbreeBVHBuilderInnerNode*)nodePtr)->children[i] = (EmbreeBVHBuilderNode*)childPtr[i];
	}

	static void setBounds (void* nodePtr, const RTCBounds** bounds, unsigned int numChildren, void* userPtr)
	{
		assert(numChildren == 2);
		for(size_t i=0; i<2; i++)
		{
			((EmbreeBVHBuilderInnerNode*)nodePtr)->bounds[i].min_ = setWToOne(loadVec4f(&bounds[i]->lower_x));
			((EmbreeBVHBuilderInnerNode*)nodePtr)->bounds[i].max_ = setWToOne(loadVec4f(&bounds[i]->upper_x));
		}
	}
};

struct EmbreeBVHBuilderLeafNode : public EmbreeBVHBuilderNode
{
	uint32 prim_ids[31];
	uint32 num_prim_ids;

	EmbreeBVHBuilderLeafNode()
	{
		this->interior = false;
	}

	static void* create (RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
	{
		assert(numPrims >= 0 && numPrims <= 31);
		void* ptr = rtcThreadLocalAlloc(alloc, sizeof(EmbreeBVHBuilderLeafNode), 16);
		new (ptr) EmbreeBVHBuilderLeafNode();
		EmbreeBVHBuilderLeafNode* leaf = (EmbreeBVHBuilderLeafNode*)ptr;

		leaf->num_prim_ids = (uint32)numPrims;
		for(size_t i=0; i<numPrims; ++i)
			leaf->prim_ids[i] = prims[i].primID;

		return ptr;
	}
};


// See also embree-3.12.1\kernels\builders\splitter.h
static void splitPrimitiveFunction(
	const struct RTCBuildPrimitive* primitive,
	unsigned int dim,
	float position,
	struct RTCBounds* leftBounds,
	struct RTCBounds* rightBounds,
	void* userPtr
)
{
	assert(dim < 3);
	assert(primitive->geomID == 0);

	EmbreeBVHBuilder* builder = (EmbreeBVHBuilder*)userPtr;
	assert(primitive->primID < builder->m_num_objects);
	const BVHBuilderTri& tri = builder->triangles[primitive->primID];

	const Vec4f prim_min = loadVec4f(&primitive->lower_x);
	const Vec4f prim_max = loadVec4f(&primitive->upper_x);
	js::AABBox aabb(prim_min, prim_max);

	// Clip
	js::AABBox tri_left_aabb  = js::AABBox(Vec4f(std::numeric_limits<float>::infinity()), Vec4f(-std::numeric_limits<float>::infinity())); // empty_aabb;
	js::AABBox tri_right_aabb = js::AABBox(Vec4f(std::numeric_limits<float>::infinity()), Vec4f(-std::numeric_limits<float>::infinity())); // empty_aabb;
	Vec4f v = tri.v[2]; // current vert
	for(int i=0; i<3; ++i) // For each triangle vertex:
	{
		const Vec4f prev_v = v;
		v = tri.v[i];
		if((prev_v[dim] < position && v[dim] > position) || (prev_v[dim] > position && v[dim] < position)) // If edge from prev_v to v straddles clip plane:
		{
			const float t = (position - prev_v[dim]) / (v[dim] - prev_v[dim]); // Solve for fraction along edge of position on split plane.
			const Vec4f p = prev_v * (1 - t) + v * t;
			tri_left_aabb.enlargeToHoldPoint(p);
			tri_right_aabb.enlargeToHoldPoint(p);
		}

		if(v[dim] <= position) tri_left_aabb .enlargeToHoldPoint(v);
		if(v[dim] >= position) tri_right_aabb.enlargeToHoldPoint(v);
	}

	// Make sure AABBs of clipped triangle don't extend past the bounds of the triangle clipped to the current AABB.
	tri_left_aabb  = intersection(tri_left_aabb,  aabb);
	tri_right_aabb = intersection(tri_right_aabb, aabb);

	storeVec4f(tri_left_aabb.min_, &leftBounds->lower_x);
	storeVec4f(tri_left_aabb.max_, &leftBounds->upper_x);

	storeVec4f(tri_right_aabb.min_, &rightBounds->lower_x);
	storeVec4f(tri_right_aabb.max_, &rightBounds->upper_x);
}

static bool progressMonitorFunction(
	void* userPtr, double n
)
{
	//EmbreeBVHBuilder* builder = (EmbreeBVHBuilder*)userPtr;
	// TODO: Add cancelling
	return true;
}


struct StackEntry
{
	const EmbreeBVHBuilderNode* node;
	int node_depth;
	int parent_node_index; // -1 if no parent.
	bool is_left;
};

// Top-level build method
void EmbreeBVHBuilder::doBuild(
		Indigo::TaskManager& task_manager_,
		ShouldCancelCallback& should_cancel_callback_,
		PrintOutput& print_output, 
		bool verbose, 
		js::Vector<ResultInteriorNode, 64>& result_nodes_out
	)
{
	max_leaf_depth = 0;

	Timer build_timer;
	ScopeProfiler _scope("EmbreeBVHBuilder::build");

	RTCBVH bvh = rtcNewBVH(this->embree_device);

	RTCBuildArguments build_args = rtcDefaultBuildArguments();
	build_args.buildQuality = this->do_high_quality_build ? RTC_BUILD_QUALITY_HIGH : RTC_BUILD_QUALITY_MEDIUM;
	build_args.maxBranchingFactor = 2;
	build_args.maxDepth = 60;
	build_args.minLeafSize = 1;
	build_args.maxLeafSize = max_num_objects_per_leaf;
	build_args.traversalCost = 1.f;
	build_args.intersectionCost = intersection_cost;

	const size_t build_prims_size = this->do_high_quality_build ? (m_num_objects * 2) : m_num_objects; // Allocate extra space for split primitives as Embree requires.
	js::Vector<RTCBuildPrimitive, 32> build_prims(build_prims_size);
	for(size_t i=0; i<m_num_objects; ++i)
	{
		const BVHBuilderTri& tri = triangles[i];

		js::AABBox tri_aabb(tri.v[0], tri.v[0]);
		tri_aabb.enlargeToHoldPoint(tri.v[1]);
		tri_aabb.enlargeToHoldPoint(tri.v[2]);

		build_prims[i].lower_x = tri_aabb.min_[0];
		build_prims[i].lower_y = tri_aabb.min_[1];
		build_prims[i].lower_z = tri_aabb.min_[2];
		build_prims[i].geomID = 0;
		build_prims[i].upper_x = tri_aabb.max_[0];
		build_prims[i].upper_y = tri_aabb.max_[1];
		build_prims[i].upper_z = tri_aabb.max_[2];
		build_prims[i].primID = (unsigned int)i;
	}

	build_args.bvh = bvh;
	build_args.primitives = build_prims.data();
	build_args.primitiveCount = m_num_objects;
	build_args.primitiveArrayCapacity = build_prims.size();

	build_args.createNode = EmbreeBVHBuilderInnerNode::create;
	build_args.setNodeChildren = EmbreeBVHBuilderInnerNode::setChildren;
	build_args.setNodeBounds = EmbreeBVHBuilderInnerNode::setBounds;
	build_args.createLeaf = EmbreeBVHBuilderLeafNode::create;
	build_args.splitPrimitive = this->do_high_quality_build ? splitPrimitiveFunction : NULL;
	build_args.buildProgress = progressMonitorFunction;
	build_args.userPtr = this;

	//Timer timer;
	EmbreeBVHBuilderNode* root = (EmbreeBVHBuilderNode*)rtcBuildBVH(&build_args);
	//conPrint("rtcBuildBVH took " + timer.elapsedString());
	//timer.reset();

	// Flatten embree nodes to result_nodes_out.  Convert internal pointers to children to indices.
	result_nodes_out.reserveNoCopy(m_num_objects);
	result_indices.reserveNoCopy(m_num_objects * 2);

	CircularBuffer<StackEntry> stack; // Stack of references to interior nodes to process.

	// Push root node onto stack
	{
		StackEntry entry;
		entry.node = root;
		entry.node_depth = 0;
		entry.parent_node_index = -1;
		stack.push_back(entry);
	}

	while(!stack.empty())
	{
		const StackEntry entry = stack.back();
		const EmbreeBVHBuilderNode* n = entry.node;
		stack.pop_back();
		/*const StackEntry entry = stack.front();
		const Node* n = entry.node;
		stack.pop_front();*/

		this->max_leaf_depth = myMax(this->max_leaf_depth, entry.node_depth + 1); // Plus 1 for depth of the leaves of this interior node.

		const int node_index = (int)result_nodes_out.size();
		
		// Fix up index in parent node to this node.
		if(entry.parent_node_index >= 0)
		{
			if(entry.is_left)
				result_nodes_out[entry.parent_node_index].left = node_index;
			else
				result_nodes_out[entry.parent_node_index].right = node_index;
		}

		assert(n->interior);
		const EmbreeBVHBuilderInnerNode* inner = (const EmbreeBVHBuilderInnerNode*)n;

		// Add this node to result nodes vector
		result_nodes_out.push_back_uninitialised();
		ResultInteriorNode& cur_result_node = result_nodes_out[node_index];
		cur_result_node.child_aabbs[0] = inner->bounds[0];
		cur_result_node.child_aabbs[1] = inner->bounds[1];

		const EmbreeBVHBuilderNode* left_child  = inner->children[0];
		const EmbreeBVHBuilderNode* right_child = inner->children[1];
		if(left_child->interior)
		{
			StackEntry new_entry;
			new_entry.node = left_child;
			new_entry.node_depth = entry.node_depth + 1;
			new_entry.parent_node_index = node_index;
			new_entry.is_left = true;
			stack.push_back(new_entry);

			cur_result_node.left_num_prims = -1; // Mark left child as interior.
		}
		else // Else if left child is a leaf:
		{
			cur_result_node.left = (int)result_indices.size();
			EmbreeBVHBuilderLeafNode* left_leaf = (EmbreeBVHBuilderLeafNode*)left_child;
			cur_result_node.left_num_prims = left_leaf->num_prim_ids;

			for(uint32 i=0; i<left_leaf->num_prim_ids; ++i)
				result_indices.push_back(left_leaf->prim_ids[i]);
		}

		if(right_child->interior)
		{
			StackEntry new_entry;
			new_entry.node = right_child;
			new_entry.node_depth = entry.node_depth + 1;
			new_entry.parent_node_index = node_index;
			new_entry.is_left = false;
			stack.push_back(new_entry);

			cur_result_node.right_num_prims = -1; // Mark right child as interior.
		}
		else
		{
			cur_result_node.right = (int)result_indices.size();
			EmbreeBVHBuilderLeafNode* right_leaf = (EmbreeBVHBuilderLeafNode*)right_child;
			cur_result_node.right_num_prims = right_leaf->num_prim_ids;

			for(uint32 i=0; i<right_leaf->num_prim_ids; ++i)
				result_indices.push_back(right_leaf->prim_ids[i]);
		}
	}

	//printVar(m_num_objects);
	//printVar(result_nodes_out.size());
	//printVar(result_indices.size());
	//conPrint("node flattening took " + timer.elapsedString());

	rtcReleaseBVH(bvh);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../maths/PCG32.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/Plotter.h"
#include "../utils/FileUtils.h"


void EmbreeBVHBuilder::test()
{
	conPrint("EmbreeBVHBuilder::test()");


	
	conPrint("EmbreeBVHBuilder::test() done");
}


#endif
