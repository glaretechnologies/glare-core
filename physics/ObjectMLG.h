/*=====================================================================
ObjectMLG.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-12 23:08:01 +0000
=====================================================================*/
#pragma once


#include "../utils/Vector.h"
#include "../physics/MultiLevelGrid.h"
#include "../raytracing/hitinfo.h"
class Object;
class PrintOutput;
class ThreadContext;
class HitInfo;
class Ray;


// Each MLG node has one of these. 
struct ObjectMLGNodeData
{
	int objects_index;
	int num_objects;
};


struct ObjectMLGResult
{
	const Object* hit_object;
	HitInfo hitinfo;
	float dist;
	double time;
	ThreadContext* thread_context;
};

class ObjectMLG;
struct ObjectMLGCellTest
{
	void visit(const Ray& ray, uint32 node, const Vec4i& cell, const ObjectMLGNodeData& data, float t_enter, float t_exit, float& max_t_in_out, ObjectMLGResult& result_out);

	ObjectMLG* object_mlg;
};


/*=====================================================================
ObjectMLG
-------------------

=====================================================================*/
class ObjectMLG
{
public:
	ObjectMLG();
	~ObjectMLG();

	friend class BuildCallBacks;

	typedef float Real;

	void insertObject(const Object* object);

	Real traceRay(const Ray& ray, ThreadContext& thread_context, double time, 
		const Object* last_object_hit,
		unsigned int last_triangle_hit,
		const Object*& hitob_out, HitInfo& hitinfo_out) const;

	bool doesFiniteRayHit(const Ray& ray, Real length, ThreadContext& thread_context, double time, const Object* ignore_object, unsigned int ignore_tri) const;

	void build(PrintOutput& print_output, bool verbose);

private:
	//void doBuild(const js::Vector<js::AABBox, 64>& primitive_aabbs, const std::vector<uint32>& primitives, uint32 node_index, const Vec4f& node_orig, int depth);

	js::Vector<const Object*, 16> objects;


	MultiLevelGrid<ObjectMLGCellTest, ObjectMLGResult, ObjectMLGNodeData>* grid;
	ObjectMLGCellTest cell_test;
public:
	js::Vector<const Object*, 16> leaf_objects;
};
