/*=====================================================================
Tree.cpp
--------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#include "jscol_Tree.h"


#include <assert.h>


namespace js
{


Tree::Tree()
{
	
}


Tree::~Tree()
{
	
}


Tree::DistType Tree::traceSphere(const Ray& ray, float radius, DistType max_t, ThreadContext& thread_context, Vec4f& hit_normal_out) const
{
	assert(0);
	return -1.f;
}


void Tree::appendCollPoints(const Vec4f& sphere_pos, float radius, ThreadContext& thread_context, std::vector<Vec4f>& points_in_out) const
{
	assert(0);
}


} //end namespace js
