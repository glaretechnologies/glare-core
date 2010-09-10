/*=====================================================================
HashedGridTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Sep 09 12:53:49 +1200 2010
=====================================================================*/
#include "HashedGridTests.h"


#if (BUILD_TESTS)

#include "HashedGrid.h"

#include <iostream>
#include "../utils/clock.h"
#include "../utils/MTwister.h"
#include "../indigo/TestUtils.h"

HashedGridTests::HashedGridTests()
{

}


HashedGridTests::~HashedGridTests()
{

}


void HashedGridTests::test()
{
	int num_grid_resolutions = 7;
	int grid_resolutions[] = { 3, 4, 8, 16, 64, 1024, 4096 };

	int num_grid_sizes = 6;
	float grid_sizes[] = { 0.1f, 1.0f, 16.0f, 64.0f, 1024.0f, 65536.0f };

	for(int grid_res_count = 0; grid_res_count < num_grid_resolutions; ++grid_res_count)
	for(int grid_size_count = 0; grid_size_count < num_grid_sizes; ++grid_size_count)
	{
		const int grid_res = grid_resolutions[grid_res_count];
		const float grid_size = grid_sizes[grid_size_count];

		js::AABBox aabb;
		aabb.min_ = Vec4f(-grid_size, -grid_size, -grid_size, 1);
		aabb.max_ = Vec4f( grid_size,  grid_size,  grid_size, 1);

		const float grid_cell_width = 2 * grid_size / (float)grid_res;

		Vec4f insertion_point;
		js::AABBox insert_aabb;
		HashedGrid<int, true> grid(aabb, grid_res, grid_res, grid_res);

		std::cout << "testing grid resolution " << grid_res << "^3, size = " << grid_size << "^3" << ", num buckets = " << grid.getNumBuckets() << std::endl;


		// --------------------------------------------------------------------------------------------------------
		// --------------------------------------------------------------------------------------------------------

		std::cout << "testing insertion at " << (grid_res - 2) * (grid_res - 2) * (grid_res - 2) << " interior integer lattice points" << std::endl;

		for(int interior_z = 1; interior_z < (grid_res - 1); ++interior_z)
		for(int interior_y = 1; interior_y < (grid_res - 1); ++interior_y)
		for(int interior_x = 1; interior_x < (grid_res - 1); ++interior_x)
		{
			insertion_point = aabb.min_ + Vec4f(interior_x , interior_y, interior_z, 0) * grid_cell_width;

			// Bounding box is two grid cells wide in each dimension, centred around the insertion point
			insert_aabb.min_ = insertion_point - Vec4f(grid_cell_width, grid_cell_width, grid_cell_width, 0);
			insert_aabb.max_ = insertion_point + Vec4f(grid_cell_width, grid_cell_width, grid_cell_width, 0);

			// Insert value with associated bound into grid
			grid.clear();
			grid.insert(666, insert_aabb);

			// Now check that the grid cells have a total of 27 references to the inserted value
			unsigned int num_refs = 0;
			for(int z = 0; z < grid_res; ++z)
			for(int y = 0; y < grid_res; ++y)
			for(int x = 0; x < grid_res; ++x)
			{
				HashBucket<int>& bucket = grid.getBucketForIndices(x, y, z);

				const int x_minbound = (int)(interior_x - 1), y_minbound = (int)(interior_y - 1), z_minbound = (int)(interior_z - 1);
				const int x_maxbound = (int)(interior_x + 1), y_maxbound = (int)(interior_y + 1), z_maxbound = (int)(interior_z + 1);

				const bool should_have_data =	(x >= x_minbound) && (x <= x_maxbound) &&
												(y >= y_minbound) && (y <= y_maxbound) &&
												(z >= z_minbound) && (z <= z_maxbound);

				if(should_have_data && bucket.data.size() < 1)
				{
					std::cout << "error: bucket " << x << ", " << y << ", " << z << " has no data elements (insertion at " << interior_x << ", " << interior_y << ", " << interior_z << ")" << std::endl;
					exit(1);
				}

				num_refs += bucket.data.size();
			}

			if(num_refs != 27)
			{
				std::cout << "error: counted " << num_refs << " references in the grid, should be 27." << std::endl;
				exit(1);
			}
		}

		// --------------------------------------------------------------------------------------------------------
		// --------------------------------------------------------------------------------------------------------

		// Now we test with random insertion points

		int num_random_insertions = grid_res * grid_res * grid_res * 64;
		std::cout << "testing insertion at " << num_random_insertions << " random points." << std::endl;

		MTwister rng(::getSecsSince1970());

		for(int i = 0; i < num_random_insertions; ++i)
		{
			const float insertion_x = 1 + rng.unitRandom() * (grid_res - 2);
			const float insertion_y = 1 + rng.unitRandom() * (grid_res - 2);
			const float insertion_z = 1 + rng.unitRandom() * (grid_res - 2);

			insertion_point = aabb.min_ + Vec4f(insertion_x, insertion_y, insertion_z, 0) * grid_cell_width;

			// Bounding box is one grid cell big, centred around the insertion point
			insert_aabb.min_ = insertion_point - Vec4f(grid_cell_width, grid_cell_width, grid_cell_width, 0);
			insert_aabb.max_ = insertion_point + Vec4f(grid_cell_width, grid_cell_width, grid_cell_width, 0);

			// Insert value with associated bound into grid
			grid.insert(666, insert_aabb);

			// Now check that the grid cells have a total of 27 references to the inserted value
			unsigned int num_refs = 0;
			for(int z = 0; z < grid_res; ++z)
			for(int y = 0; y < grid_res; ++y)
			for(int x = 0; x < grid_res; ++x)
			{
				HashBucket<int>& bucket = grid.getBucketForIndices(x, y, z);

				const int x_minbound = (int)(insertion_x - 1), y_minbound = (int)(insertion_y - 1), z_minbound = (int)(insertion_z - 1);
				const int x_maxbound = (int)(insertion_x + 1), y_maxbound = (int)(insertion_y + 1), z_maxbound = (int)(insertion_z + 1);

				const bool should_have_data =	(x >= x_minbound) && (x <= x_maxbound) &&
												(y >= y_minbound) && (y <= y_maxbound) &&
												(z >= z_minbound) && (z <= z_maxbound);

				if(should_have_data && bucket.data.size() < 1)
				{
					std::cout << "error: bucket " << x << ", " << y << ", " << z << " has no data elements" << std::endl;
					exit(1);
				}

				num_refs += bucket.data.size();
			}

			if(num_refs != 27)
			{
				std::cout << "error: counted " << num_refs << " references in the grid, should be 27." << std::endl;
				exit(1);
			}

			grid.clear();
		}

		std::cout << std::endl;
	}
}


#endif
