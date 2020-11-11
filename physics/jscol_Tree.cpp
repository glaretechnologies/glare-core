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


Tree::DistType Tree::traceSphere(const Ray& ray_dir_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_normal_ws_out) const
{
	assert(0);
	return -1.f;
}


void Tree::appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_in_out) const
{
	assert(0);
}


} //end namespace js
