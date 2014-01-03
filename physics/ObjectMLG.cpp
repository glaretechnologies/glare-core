/*=====================================================================
ObjectMLG.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-12 23:08:01 +0000
=====================================================================*/
#include "ObjectMLG.h"


#include "../indigo/object.h"
#include "MultiLevelGridBuilder.h"


ObjectMLG::ObjectMLG()
{

}


ObjectMLG::~ObjectMLG()
{
	delete grid;
}


void ObjectMLG::insertObject(const Object* object)
{
	objects.push_back(object);
}


ObjectMLG::Real ObjectMLG::traceRay(const Ray& ray, ThreadContext& thread_context, double time, 
		const Object* last_object_hit,
		unsigned int last_triangle_hit,
		const Object*& hitob_out, HitInfo& hitinfo_out) const
{
	ObjectMLGResult results;
	results.hit_object = NULL;
	results.time = time;
	results.thread_context = &thread_context;

	grid->trace(ray, std::numeric_limits<float>::max(), results);

	if(results.hit_object != NULL)
	{
		hitob_out = results.hit_object;
		return results.dist;
	}
	else
		return -1.0f;
}


bool ObjectMLG::doesFiniteRayHit(const Ray& ray, Real length, ThreadContext& thread_context, double time, const Object* ignore_object, unsigned int ignore_tri) const
{
	ObjectMLGResult results;
	results.hit_object = NULL;
	results.time = time;
	results.thread_context = &thread_context;

	grid->trace(ray, length, results);

	if(results.hit_object != NULL)
		return true;
	else
		return false;
}


class BuildCallBacks
{
public:
	void markNodeAsLeaf(const js::Vector<js::AABBox, 64>& primitive_aabbs, const std::vector<uint32>& node_primitives, uint32 node_index,
		const Vec4f& node_orig, int depth)
	{
		object_mlg->grid->nodes[node_index].data.objects_index = (int)object_mlg->leaf_objects.size();
		object_mlg->grid->nodes[node_index].data.num_objects = (int)node_primitives.size();

		for(size_t i=0; i<node_primitives.size(); ++i)
			object_mlg->leaf_objects.push_back(object_mlg->objects[i]);
	}
	ObjectMLG* object_mlg;
};


void ObjectMLG::build(PrintOutput& print_output, bool verbose)
{
	// Make vector of object AABBs
	js::Vector<js::AABBox, 64> aabbs(objects.size());

	js::AABBox total_aabb = objects[0]->getAABBoxWS();

	for(size_t i=0; i<objects.size(); ++i)
	{
		aabbs[i] = objects[i]->getAABBoxWS();
		total_aabb.enlargeToHoldAABBox(objects[i]->getAABBoxWS());
	}

	cell_test.object_mlg = this;

	grid = new MultiLevelGrid<ObjectMLGCellTest, ObjectMLGResult, ObjectMLGNodeData>(total_aabb.min_, total_aabb.max_, cell_test);

	

	MultiLevelGridBuilder<ObjectMLGCellTest, ObjectMLGResult, ObjectMLGNodeData, BuildCallBacks> builder(*grid);

	BuildCallBacks build_callbacks;
	build_callbacks.object_mlg = this;

	builder.build(aabbs, build_callbacks);
}


/*
The traversal has hit a cell.
*/
void ObjectMLGCellTest::visit(const Ray& ray, uint32 node, const Vec4i& cell, const ObjectMLGNodeData& data, float t_enter, float t_exit, float& max_t_in_out, ObjectMLGResult& result_out)
{
	size_t offset = data.objects_index;
	size_t num = data.num_objects;

	for(size_t i=offset; i<offset + num; ++i)
	{
		HitInfo hitinfo;
		float d = 0.0; // TEMP object_mlg->leaf_objects[i]->traceRay(ray, max_t_in_out, result_out.time, *result_out.thread_context, std::numeric_limits<unsigned int>::max(), hitinfo);
		if(d >= 0 && d < max_t_in_out)
		{
			max_t_in_out = d;
			result_out.dist = d;
			result_out.hitinfo = hitinfo;
			result_out.hit_object = object_mlg->leaf_objects[i];
		}
	}
}
