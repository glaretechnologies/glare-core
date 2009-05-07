/*=====================================================================
TreeUtils.h
-----------
File created by ClassTemplate on Tue Jul 17 18:46:24 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREEUTILS_H_666_
#define __TREEUTILS_H_666_


#include "../simpleraytracer/ray.h"
class RayMesh;
class PrintOutput;


namespace js
{


class AABBox;


/*=====================================================================
TreeUtils
---------

=====================================================================*/
class TreeUtils
{
public:
	/*=====================================================================
	TreeUtils
	---------
	
	=====================================================================*/
	TreeUtils();

	~TreeUtils();



	//static inline void buildRayChildIndices(const Ray& ray, unsigned int ray_child_indices[4][2]);

	static inline void buildFlatRayChildIndices(const Ray& ray, unsigned int ray_child_indices[8]);

	static void buildRootAABB(const RayMesh& raymesh, js::AABBox& aabb_out, PrintOutput& print_output);

	static float getTreeSpecificMinT(const AABBox& aabb);
};

#if 0
void TreeUtils::buildRayChildIndices(const Ray& ray, unsigned int ray_child_indices[4][2])
{
//TEMP OLD WAY:///////

	/*
	ray_child_indices[0][0] = ray_child_indices[1][0] = ray_child_indices[2][0] = ray_child_indices[3][0] = 0;
	ray_child_indices[0][1] = ray_child_indices[1][1] = ray_child_indices[2][1] = ray_child_indices[3][1] = 1;

	if(ray.unitDir().x < 0.0f) 
		mySwap(ray_child_indices[0][0], ray_child_indices[0][1]);
	if(ray.unitDir().y < 0.0f) 
		mySwap(ray_child_indices[1][0], ray_child_indices[1][1]);
	if(ray.unitDir().z < 0.0f) 
		mySwap(ray_child_indices[2][0], ray_child_indices[2][1]);*/

////////////////


	/*const SSE_ALIGN int one_int_array[4] = {1, 1, 1, 1};

	const __m128 one_int = _mm_load_ps((float*)one_int_array);
	const __m128 raydir = _mm_load_ps(&ray.unitDirF()[0]);
	const __m128 mask = _mm_cmplt_ps(raydir, zeroVec()); // mask[i] = raydir[i] < 0.0 ? 0xFFFFFFFF : 0x0

	const __m128 result = and4Vec(mask, one_int); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0
	
	ray_child_indices[0][0] = result.m128_i32[0];
	ray_child_indices[1][0] = result.m128_i32[1];
	ray_child_indices[2][0] = result.m128_i32[2];
	ray_child_indices[3][0] = result.m128_i32[3];

	const __m128 result2 = andNot4Vec(mask, one_int); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0

	ray_child_indices[0][1] = result2.m128_i32[0];
	ray_child_indices[1][1] = result2.m128_i32[1];
	ray_child_indices[2][1] = result2.m128_i32[2];
	ray_child_indices[3][1] = result2.m128_i32[3];*/

	

	const __m128 mask = _mm_cmplt_ps(_mm_load_ps(&ray.unitDirF()[0]), zeroVec()); // mask[i] = raydir[i] < 0.0 ? 0xFFFFFFFF : 0x0

//	const SSE_ALIGN int one_int_array[4] = {1, 1, 1, 1};

	float one;
	*((int*)(&one)) = 1;

	//const __m128 result = and4Vec(mask, _mm_load_ps((float*)one_int_array)); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0
	const __m128 result = and4Vec(mask, _mm_set1_ps(one)); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0

	
	ray_child_indices[0][0] = result.m128_i32[0];
	ray_child_indices[1][0] = result.m128_i32[1];
	ray_child_indices[2][0] = result.m128_i32[2];
	ray_child_indices[3][0] = result.m128_i32[3];

	const __m128 result2 = andNot4Vec(mask, _mm_set1_ps(one)); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0

	ray_child_indices[0][1] = result2.m128_i32[0];
	ray_child_indices[1][1] = result2.m128_i32[1];
	ray_child_indices[2][1] = result2.m128_i32[2];
	ray_child_indices[3][1] = result2.m128_i32[3];


#ifdef DEBUG
	//compute normally and check results
	unsigned int ray_child_indices_d[4][2] = {{0, 1}, {0, 1}, {0, 1}, {0,0}};
	
	if(ray.unitDir().x < 0.0f) 
		mySwap(ray_child_indices_d[0][0], ray_child_indices_d[0][1]);
	if(ray.unitDir().y < 0.0f) 
		mySwap(ray_child_indices_d[1][0], ray_child_indices_d[1][1]);
	if(ray.unitDir().z < 0.0f) 
		mySwap(ray_child_indices_d[2][0], ray_child_indices_d[2][1]);

	for(unsigned int z=0; z<3; ++z)
	{
		assert(ray_child_indices[z][0] == ray_child_indices_d[z][0]);
		assert(ray_child_indices[z][1] == ray_child_indices_d[z][1]);
	}

#endif
}
#endif


void TreeUtils::buildFlatRayChildIndices(const Ray& ray, unsigned int ray_child_indices[8])
{

	/*ray_child_indices[0] = ray_child_indices[1] = ray_child_indices[2] = 0;
	ray_child_indices[4] = ray_child_indices[5] = ray_child_indices[6] = 1;

	if(ray.unitDir().x < 0.0f) 
		mySwap(ray_child_indices[0], ray_child_indices[4]);
	if(ray.unitDir().y < 0.0f) 
		mySwap(ray_child_indices[1], ray_child_indices[5]);
	if(ray.unitDir().z < 0.0f) 
		mySwap(ray_child_indices[2], ray_child_indices[6]);*/

	const __m128 mask = _mm_cmplt_ps(ray.unitDirF().v, zeroVec()); // mask[i] = raydir[i] < 0.0 ? 0xFFFFFFFF : 0x0

//	const SSE_ALIGN int one_int_array[4] = {1, 1, 1, 1};

	union
	{
		float f;
		int i;
	} one;

	one.i = 1;

	//const __m128 result = and4Vec(mask, _mm_load_ps((float*)one_int_array)); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0
	_mm_store_ps((float*)ray_child_indices, and4Vec(mask, _mm_set1_ps(one.f))); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0

	_mm_store_ps((float*)ray_child_indices + 4, andNot4Vec(mask, _mm_set1_ps(one.f))); //result[i] = mask & 0x1 = (raydir[i] < 0.0) ? 1 : 0

#ifdef DEBUG
	unsigned int ray_child_indices_d[8] = {0, 0, 0, 0, 1, 1, 1, 1};

	if(ray.unitDir()[0] < 0.0f) 
		mySwap(ray_child_indices_d[0], ray_child_indices_d[4]);
	if(ray.unitDir()[1] < 0.0f) 
		mySwap(ray_child_indices_d[1], ray_child_indices_d[5]);
	if(ray.unitDir()[2] < 0.0f) 
		mySwap(ray_child_indices_d[2], ray_child_indices_d[6]);
	for(unsigned int z=0; z<3; ++z)
	{
		assert(ray_child_indices[z] == ray_child_indices_d[z]);
		assert(ray_child_indices[z+4] == ray_child_indices_d[z+4]);
	}
#endif

}





} //end namespace js


#endif //__TREEUTILS_H_666_




