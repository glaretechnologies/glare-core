/*=====================================================================
HashedGrid.h
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at Tue Sep 07 16:03:27 +1200 2010
=====================================================================*/
#pragma once


#include "../utils/Vector.h"
#include "../utils/SmallVector.h"
#include "../maths/Vec4f.h"
#include "../maths/SSE.h"
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
-------------------

=====================================================================*/
template<typename T>
class HashedGrid
{
public:
	HashedGrid(	const js::AABBox& aabb,
				float cell_w_, int num_buckets_)
	:	grid_aabb(aabb),
		cell_w(cell_w_),
		recip_cell_w(1 / cell_w_),
		num_buckets((unsigned int)num_buckets_)
	{
		assert(num_buckets_ > 0);

		buckets.resize(num_buckets_);
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
		return Vec4i(
			(int)((p[0] - grid_aabb.min_.x[0]) * recip_cell_w),
			(int)((p[1] - grid_aabb.min_.x[1]) * recip_cell_w),
			(int)((p[2] - grid_aabb.min_.x[2]) * recip_cell_w),
			0
		);
	}


	inline Vec4i getGridMaxBound(const Vec4f& p) const
	{
		return Vec4i(
			(int)((p[0] - grid_aabb.min_.x[0]) * recip_cell_w),
			(int)((p[1] - grid_aabb.min_.x[1]) * recip_cell_w),
			(int)((p[2] - grid_aabb.min_.x[2]) * recip_cell_w),
			0
		);
	}


	inline void insert(const T& t, const Vec4f& p)
	{
		unsigned int bucket_i = getBucketIndexForPoint(p);
		buckets[bucket_i].data.push_back(t);
	}


	inline const HashBucket<T>& getBucketForIndices(const int x, const int y, const int z) const
	{
		return buckets[computeHash(x, y, z)];
	}

private:

	inline unsigned int getBucketIndexForPoint(const Vec4f& p) const
	{
		const int x = (int)((p.x[0] - grid_aabb.min_.x[0]) * recip_cell_w);
		const int y = (int)((p.x[1] - grid_aabb.min_.x[1]) * recip_cell_w);
		const int z = (int)((p.x[2] - grid_aabb.min_.x[2]) * recip_cell_w);

		return computeHash(x, y, z);
	}


	inline unsigned int computeHash(const int x, const int y, const int z) const
	{
		// Convert to unsigned ints so we can have well-defined overflow.
		unsigned int ux;
		unsigned int uy;
		unsigned int uz;
		std::memcpy(&ux, &x, sizeof(int));
		std::memcpy(&uy, &y, sizeof(int));
		std::memcpy(&uz, &z, sizeof(int));

		return ((ux * 73856093) ^ (uy * 19349663) ^ (uz * 83492791)) % num_buckets;
	}


	const js::AABBox grid_aabb;
	unsigned int num_buckets;
	float recip_cell_w;
	float cell_w;
	std::vector<HashBucket<T> > buckets;
};
