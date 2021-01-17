/*=====================================================================
HashedGrid.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/Vector.h"
#include "../utils/SmallVector.h"
#include "../maths/Vec4f.h"
#include "jscol_aabbox.h"
#include <vector>


template<typename T>
class HashBucket
{
public:
	HashBucket() {}

	//js::Vector<T, 64> data;
	SmallVector<T, 2> data;
};


/*=====================================================================
HashedGrid
----------
TODO: Try using open addressing (don't use a vector in buckets, spill into adjacent buckets)

Tests in HashedGridTests
=====================================================================*/
template<typename T>
class HashedGrid
{
public:
	HashedGrid(	const js::AABBox& aabb,
				float cell_w_, int expected_num_items)
	:	grid_aabb(aabb),
		cell_w(cell_w_),
		recip_cell_w(1 / cell_w_)
	{
		assert(expected_num_items > 0);

		const unsigned int num_buckets = myMax<unsigned int>(8, (unsigned int)Maths::roundToNextHighestPowerOf2((unsigned int)expected_num_items/* * 2*/));
		buckets.resize(num_buckets);

		hash_mask = num_buckets - 1;
	}


	inline int getNumBuckets() { return (int)buckets.size(); }


	inline int getNumCells() const
	{ 
		return (int)(std::ceil((1.0e-5f + grid_aabb.axisLength(0)) / cell_w) * std::ceil((1.0e-5f + grid_aabb.axisLength(1)) / cell_w) * std::ceil((1.0e-5f + grid_aabb.axisLength(2)) / cell_w));
	}


	inline void clear()
	{
		for(size_t i = 0; i < buckets.size(); ++i)
			buckets[i].data.resize(0);
	}


	inline Vec4i getGridMinBound(const Vec4f& p) const
	{
		return truncateToVec4i((p - grid_aabb.min_) * recip_cell_w);
	}


	inline Vec4i getGridMaxBound(const Vec4f& p) const
	{
		return truncateToVec4i((p - grid_aabb.min_) * recip_cell_w);
	}


	inline void insert(const T& t, const Vec4f& p)
	{
		unsigned int bucket_i = getBucketIndexForPoint(p);
		buckets[bucket_i].data.push_back(t);
	}


	inline const HashBucket<T>& getBucketForIndices(const Vec4i& p) const
	{
		return buckets[computeHash(p)];
	}

	inline const HashBucket<T>& getBucketForIndices(const int x, const int y, const int z) const
	{
		return buckets[computeHash(x, y, z)];
	}

	inline unsigned int getBucketIndexForPoint(const Vec4f& p) const
	{
		const Vec4i p_i = truncateToVec4i((p - grid_aabb.min_) * recip_cell_w);

		return computeHash(p_i);
	}


	inline unsigned int computeHash(const Vec4i& p_i) const
	{
		// NOTE: technically possible undefined behaviour here (signed overflow)

		//unsigned int u[4];
		//storeVec4i(p_i, u);

		return ((p_i[0] * 73856093) ^ (p_i[1] * 19349663) ^ (p_i[2] * 83492791)) & hash_mask;
	}

	inline unsigned int computeHash(int x, int y, int z) const
	{
		// NOTE: technically possible undefined behaviour here (signed overflow)

		return ((x * 73856093) ^ (y * 19349663) ^ (z * 83492791)) & hash_mask;
	}


	const js::AABBox grid_aabb;
	float recip_cell_w;
	float cell_w;
	std::vector<HashBucket<T> > buckets;
	size_t hash_mask; // hash_mask = buckets_size - 1;
};
