/*=====================================================================
HashedGridTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Sep 09 12:53:49 +1200 2010
=====================================================================*/
#include "HashedGridTests.h"


#if (BUILD_TESTS)


#include "HashedGrid.h"
#include "../utils/Clock.h"
#include "../maths/PCG32.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/TestUtils.h"
#include "../maths/vec3.h"


HashedGridTests::HashedGridTests()
{

}


HashedGridTests::~HashedGridTests()
{

}


static void perfTests()
{
	const int num_points = 1000000;

	float grid_w = 400;
	js::AABBox aabb(Vec4f(0,0,0,1), Vec4f(grid_w,grid_w,grid_w,1));
	HashedGrid<Vec3f> grid(aabb, 1.0f, num_points * 2);

	

	PCG32 rng(1);

	

	conPrint("num buckets: " + toString(grid.getNumBuckets()));
	conPrint("num points:  " + toString(num_points));

	Timer timer;

	for(int i=0; i<num_points; ++i)
	{
		Vec3f p = Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) * grid_w;
		//js::AABBox query_box(p.toVec4fPoint() - Vec4f(1,1,1,0), p.toVec4fPoint() + Vec4f(1,1,1,0));
		grid.insert(p, p.toVec4fPoint());
	}

	conPrint("Insertion took " + timer.elapsedString());
	timer.reset();

	int count = 0;
	for(int i=0; i<num_points; ++i)
	{
		// Do a lookup for the query box around some random points
		Vec3f p = Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) * grid_w;
		js::AABBox query_box(p.toVec4fPoint() - Vec4f(1,1,1,0), p.toVec4fPoint() + Vec4f(1,1,1,0));

		const Vec4i begin = grid.getGridMinBound(query_box.min_);
		const Vec4i end   = grid.getGridMaxBound(query_box.max_);

		for(int z = begin[2]; z <= end[2]; ++z)
		for(int y = begin[1]; y <= end[1]; ++y)
		for(int x = begin[0]; x <= end[0]; ++x)
		{
			const HashBucket<Vec3f>& bucket = grid.getBucketForIndices(x, y, z);

			for(size_t n=0; n<bucket.data.size(); ++n)
			{
				if(bucket.data[n].getDist2(p) < 1.0f)
					count++;
			}
		}
	}

	conPrint("Lookups took " + timer.elapsedString());
	printVar(count);



}


void HashedGridTests::test()
{
	conPrint("HashedGridTests::test()");

	perfTests();

	/*
	int num_grid_resolutions = 6;
	int grid_resolutions[] = { 3, 4, 8, 16, 64, 1024 };

	int num_grid_sizes = 5;
	float grid_sizes[] = { 0.1f, 1.0f, 16.0f, 1024.0f, 65536.0f };

	int num_insertion_scales = 4;
	float insertion_scales[] = { 0.3f, 1.0f, 1.4f, 3.1f };

	for(int insertion_scale_count = 0; insertion_scale_count < num_insertion_scales; ++insertion_scale_count)
	for(int grid_res_count = 0; grid_res_count < num_grid_resolutions; ++grid_res_count)
	for(int grid_size_count = 0; grid_size_count < num_grid_sizes; ++grid_size_count)
	{
		const float insertion_scale = insertion_scales[insertion_scale_count];
		const int grid_res = grid_resolutions[grid_res_count];
		const float grid_size = grid_sizes[grid_size_count];

		if(insertion_scale_count != 3) continue; // TEMP HACK

		js::AABBox aabb;
		aabb.min_ = Vec4f(-grid_size, -grid_size, -grid_size, 1);
		aabb.max_ = Vec4f( grid_size,  grid_size,  grid_size, 1);

		const float grid_cell_width = 2 * grid_size / (float)grid_res;

		Vec4f insertion_point;
		js::AABBox insert_aabb;

		HashedGrid<int, true> grid(aabb, grid_res, grid_res, grid_res);
		//HashedGridSTL<int, true> grid(aabb, grid_res, grid_res, grid_res);

		conPrint("testing grid resolution " + toString(grid_res) + "^3, size = " + toString(grid_size) + "^3, insertion box scale = " + toString(insertion_scale));


		// --------------------------------------------------------------------------------------------------------
		// --------------------------------------------------------------------------------------------------------

		conPrint("testing insertion at " + toString((grid_res - 2) * (grid_res - 2) * (grid_res - 2)) + " interior integer lattice points");

		for(int interior_z = 1; interior_z < (grid_res - 1); ++interior_z)
		for(int interior_y = 1; interior_y < (grid_res - 1); ++interior_y)
		for(int interior_x = 1; interior_x < (grid_res - 1); ++interior_x)
		{
			insertion_point = aabb.min_ + Vec4f((float)interior_x , (float)interior_y, (float)interior_z, 0) * grid_cell_width;
			//std::cout << "insertion point is (" << insertion_point.x[0] << ", " << insertion_point.x[1] << ", " << insertion_point.x[2] << ", " << ")" << std::endl;

			const float aabb_size = grid_cell_width * insertion_scale;

			// Bounding box is two grid cells wide in each dimension (scaled by insertion_scale), centred around the insertion point
			insert_aabb.min_ = insertion_point - Vec4f(aabb_size, aabb_size, aabb_size, 0);
			insert_aabb.max_ = insertion_point + Vec4f(aabb_size, aabb_size, aabb_size, 0);

			// Insert value with associated bound into grid
			grid.clear();
			grid.insert(666, insert_aabb);

			// Now check that the grid cells have a data in the cells we expect it to
			//unsigned int num_refs = 0;
			for(int z = 0; z < grid_res; ++z)
			for(int y = 0; y < grid_res; ++y)
			for(int x = 0; x < grid_res; ++x)
			{
				//HashBucketSTL<int>& bucket = grid.getBucketForIndices(x, y, z);
				HashBucket<int>& bucket = grid.getBucketForIndices(x, y, z);

				const int
					x_minbound = myMax<int>(0, (int)(interior_x - insertion_scale)),
					y_minbound = myMax<int>(0, (int)(interior_y - insertion_scale)),
					z_minbound = myMax<int>(0, (int)(interior_z - insertion_scale));

				const int
					x_maxbound = myMin<int>(grid_res - 1, (int)(interior_x + insertion_scale)),
					y_maxbound = myMin<int>(grid_res - 1, (int)(interior_y + insertion_scale)),
					z_maxbound = myMin<int>(grid_res - 1, (int)(interior_z + insertion_scale));

				const bool should_have_data =	(x >= x_minbound) && (x <= x_maxbound) &&
												(y >= y_minbound) && (y <= y_maxbound) &&
												(z >= z_minbound) && (z <= z_maxbound);

				if(should_have_data && bucket.data.size() < 1)
				{
					conPrint("error: bucket " + toString(x) + ", " + toString(y) + ", " + toString(z) + " has no data elements (insertion at " + toString(interior_x) + ", " + toString(interior_y) + ", " + toString(interior_z) + ")");
					exit(1);
				}
			}
		}

		// --------------------------------------------------------------------------------------------------------
		// --------------------------------------------------------------------------------------------------------

		// Now we test with random insertion points

		int num_random_insertions = grid_res * grid_res * grid_res * 4;//64;
		//std::cout << "testing insertion at " << num_random_insertions << " random points." << std::endl;

		PCG32 rng(1);

		for(int i = 0; i < num_random_insertions; ++i)
		{
			const float insertion_x = 1 + rng.unitRandom() * (grid_res - 2);
			const float insertion_y = 1 + rng.unitRandom() * (grid_res - 2);
			const float insertion_z = 1 + rng.unitRandom() * (grid_res - 2);

			insertion_point = aabb.min_ + Vec4f(insertion_x, insertion_y, insertion_z, 0) * grid_cell_width;

			const float aabb_size = grid_cell_width * insertion_scale;

			// Bounding box is one grid cell big, centred around the insertion point
			insert_aabb.min_ = insertion_point - Vec4f(aabb_size, aabb_size, aabb_size, 0);
			insert_aabb.max_ = insertion_point + Vec4f(aabb_size, aabb_size, aabb_size, 0);

			// Insert value with associated bound into grid
			grid.clear();
			grid.insert(666, insert_aabb);

			// Now check that the grid cells have a data in the cells we expect it to
			for(int z = 0; z < grid_res; ++z)
			for(int y = 0; y < grid_res; ++y)
			for(int x = 0; x < grid_res; ++x)
			{
				//HashBucketSTL<int>& bucket = grid.getBucketForIndices(x, y, z);
				HashBucket<int>& bucket = grid.getBucketForIndices(x, y, z);

				const int x_minbound = (int)(insertion_x - insertion_scale), y_minbound = (int)(insertion_y - insertion_scale), z_minbound = (int)(insertion_z - insertion_scale);
				const int x_maxbound = (int)(insertion_x + insertion_scale), y_maxbound = (int)(insertion_y + insertion_scale), z_maxbound = (int)(insertion_z + insertion_scale);

				const bool should_have_data =	(x >= x_minbound) && (x <= x_maxbound) &&
												(y >= y_minbound) && (y <= y_maxbound) &&
												(z >= z_minbound) && (z <= z_maxbound);

				if(should_have_data && bucket.data.size() < 1)
				{
					conPrint("error: bucket " + toString(x) + ", " + toString(y) + ", " + toString(z) + " has no data elements");
					exit(1);
				}
			}
		}

		//std::cout << std::endl;
	}*/
}


#endif
