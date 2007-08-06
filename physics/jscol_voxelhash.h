#ifndef __VOXELHASH_H__
#define __VOXELHASH_H__

#include <list>
#include <vector>
#include <assert.h>
#include "C:\programming\maths\maths.h"



namespace js
{

class Model;
class VHashBucket;


	//assuming all points positive here
class VoxelHash
{
public:
	VoxelHash(float voxel_length, int num_buckets);
	~VoxelHash();




	inline VHashBucket& getBucketForVoxel(int x, int y, int z);	

	inline VHashBucket& getBucketForPoint(const Vec3& point);

	float getVoxelLength() const { return voxel_length; }

	typedef std::vector<VHashBucket>::iterator bucket_iter;
	bucket_iter bucketBegin(){ return buckets.begin(); }
	bucket_iter bucketEnd(){ return buckets.end(); }

private:
	inline int hashFunc(int x, int y, int z);


	std::vector<VHashBucket> buckets;
	int num_buckets;
	float voxel_length;
};






//this could be a hash table hashed by pointers..
//probably best as binary tree
class VHashBucket
{
public:
	VHashBucket(){}
	~VHashBucket(){}

	void insert(Model* p)
	{
		pointers.push_back(p);
	}

	void remove(Model* p)
	{
		pointers.remove(p);
	}

	bool doesContain(const Model* p) const
	{
		for(std::list<Model*>::const_iterator i = pointers.begin(); i != pointers.end(); ++i)
		{
			if((*i) == p)
				return true;
		}

		return false;
	}

	int numPointers() const { return pointers.size(); }


	typedef std::list<Model*>::iterator pointer_iterator;

	pointer_iterator begin(){ return pointers.begin(); }
	pointer_iterator end(){ return pointers.end(); }

private:
	std::list<Model*> pointers;
};







VHashBucket& VoxelHash::getBucketForVoxel(int x, int y, int z)
{
	int i = hashFunc(x, y, z);

	assert(i >= 0 && i < num_buckets);

	return buckets[i];
}


VHashBucket& VoxelHash::getBucketForPoint(const Vec3& point)
{
	int x = (int)floor(point.x / voxel_length) + 1;
	int y = (int)floor(point.y / voxel_length) + 1;
	int z = (int)floor(point.z / voxel_length) + 1;

	return getBucketForVoxel(x, y, z);
}

int VoxelHash::hashFunc(int x, int y, int z)
{
	return abs(x * y * z) % num_buckets;
}















}//end namepsace js
#endif //__VOXELHASH_H__