/*=====================================================================
HashedGrid.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue Sep 07 16:03:27 +1200 2010
=====================================================================*/
#pragma once

#include "../utils/Vector.h"
#include "../maths/Vec4f.h"
#include "jscol_aabbox.h"


template<typename T>
class HashBucket
{
public:
	HashBucket() { }
	~HashBucket() { }

	js::Vector<T, 64> data;
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
				int x_res, int y_res, int z_res)
	:	grid_aabb(aabb)
	{
		grid_res[0] = x_res; inv_grid_res[0] = 1.0f / grid_res[0];
		grid_res[1] = y_res; inv_grid_res[1] = 1.0f / grid_res[1];
		grid_res[2] = z_res; inv_grid_res[2] = 1.0f / grid_res[2];

		cells.resize(4096);
	}

	virtual ~HashedGrid()
	{

	}

	inline unsigned int computeHash(const unsigned int x, const unsigned int y, const unsigned int z) const
	{
		return ((x * 73856093) ^ (y * 19349663) ^ (z * 83492791)) % cells.size();
	}

	inline void insert(const T& t, const js::AABBox& aabb_t) const
	{
		assert(grid_aabb.containsAABBox(aabb_t));

		const unsigned int grid_minbound[3] =
		{
			(unsigned int)((aabb_t.min_.x[0] - grid_aabb.min_.x[0]) * inv_grid_res[0]),
			(unsigned int)((aabb_t.min_.x[1] - grid_aabb.min_.x[1]) * inv_grid_res[1]),
			(unsigned int)((aabb_t.min_.x[2] - grid_aabb.min_.x[2]) * inv_grid_res[2])
		};
		const unsigned int grid_maxbound[3] =
		{
			(unsigned int)((aabb_t.max_.x[0] - grid_aabb.min_.x[0]) * inv_grid_res[0]),
			(unsigned int)((aabb_t.max_.x[1] - grid_aabb.min_.x[1]) * inv_grid_res[1]),
			(unsigned int)((aabb_t.max_.x[2] - grid_aabb.min_.x[2]) * inv_grid_res[2])
		};

		assert(grid_maxbound[0] < grid_res[0]);
		assert(grid_maxbound[1] < grid_res[1]);
		assert(grid_maxbound[2] < grid_res[2]);

		for(unsigned int z = grid_minbound[2]; z <= grid_maxbound[2]; ++z)
		for(unsigned int y = grid_minbound[1]; y <= grid_maxbound[1]; ++y)
		for(unsigned int x = grid_minbound[0]; x <= grid_maxbound[0]; ++x)
		{
			const unsigned int id = computeHash(x, y, z);

			cells[id].data.push_back(t);
		}
	}

	inline HashBucket<T>& getBucketForPoint(const Vec4f& p)
	{
		const unsigned int x = (unsigned int)((p.x[0] - grid_aabb.min_.x[0]) * inv_grid_res[0]); assert(x < grid_res[0]);
		const unsigned int y = (unsigned int)((p.x[1] - grid_aabb.min_.x[1]) * inv_grid_res[1]); assert(y < grid_res[0]);
		const unsigned int z = (unsigned int)((p.x[2] - grid_aabb.min_.x[2]) * inv_grid_res[2]); assert(z < grid_res[0]);

		return cells[computeHash(x, y, z)];
	}

	const js::AABBox& grid_aabb;
	float inv_grid_res[3];
	int grid_res[3];

	js::Vector<HashBucket<T>, 64> cells;

private:

};



/*
	struct HPoint
	{
		Vec f, pos, nrm, flux;
		
		double r2;
		unsigned int n;
		int pix;
	};

	struct List
	{
		HPoint *id;
		List *next;
	};

	List* ListAdd(HPoint *i, List* h)
	{
		List* p = new List;
		p->id = i;
		p->next = h;

		return p;
	}

	unsigned int num_hash, pixel_index, num_photon;

	double hash_s;
	List **hash_grid;
	List *hitpoints = NULL;

	AABB hpbbox;

	inline unsigned int hash(const int ix, const int iy, const int iz)
	{
		return (unsigned int)((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791)) % num_hash;
	}

	void build_hash_grid(const int w, const int h)
	{
		hpbbox.reset();
		List *lst = hitpoints;

		while (lst != NULL)
		{
			HPoint *hp = lst->id;
			lst = lst->next;
			hpbbox.fit(hp->pos);
		}

		// compute initial radius from bounding box of all hit points
		Vec ssize = hpbbox.max - hpbbox.min;
		double irad = ((ssize.x + ssize.y + ssize.z) / 3.0) / ((w + h) / 2.0) * 2.0;

		hpbbox.reset(); lst = hitpoints; int vphoton = 0; // determine hash size

		while (lst != NULL)
		{
			HPoint *hp = lst->id; lst = lst->next;

			hp->r2 = irad *irad;
			hp->n = 0;
			hp->flux = Vec();

			vphoton++;
			
			hpbbox.fit(hp->pos - irad);
			hpbbox.fit(hp->pos + irad);
		}

		hash_s = 1.0 / (irad * 2.0);
		num_hash = vphoton;

		hash_grid = new List*[num_hash];

		for (unsigned int i = 0; i < num_hash; i++) hash_grid[i] = NULL;

		lst = hitpoints;

		while (lst != NULL)
		{
			// store hitpoints in the hashed grid

			HPoint *hp = lst->id; lst = lst->next;

			Vec BMin = ((hp->pos - irad) - hpbbox.min) * hash_s;
			Vec BMax = ((hp->pos + irad) - hpbbox.min) * hash_s;

			for (int iz = abs(int(BMin.z)); iz <= abs(int(BMax.z)); iz++)
				for (int iy = abs(int(BMin.y)); iy <= abs(int(BMax.y)); iy++)
					for (int ix = abs(int(BMin.x)); ix <= abs(int(BMax.x)); ix++)
					{
						int hv = hash(ix, iy, iz);

						hash_grid[hv] = ListAdd(hp, hash_grid[hv]);
					}
			}
		}

*/