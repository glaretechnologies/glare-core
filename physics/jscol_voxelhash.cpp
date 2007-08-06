#include "jscol_voxelhash.h"

js::VoxelHash::VoxelHash(float voxel_length_, int num_buckets_)
{
	voxel_length = voxel_length_;
	num_buckets = num_buckets_;

	buckets.resize(num_buckets);
	/*Vec3 dim = big_x_y_z_corner - smallx_y_z_corner;

	assert(dim.x > 0 && dim.y > 0 && dim.z > 0);

	int buckets_x = (int)(floor(dim.x / voxel_length)) + 1;
	int buckets_y = (int)(floor(dim.y / voxel_length)) + 1;
	int buckets_z = (int)(floor(dim.z / voxel_length)) + 1;

	float hashratio = (float)(buckets_x * buckets_y * buckets_z) / (float)num_buckets;*/

	//buckets = new VHashBucket[num_buckets];
}

js::VoxelHash::~VoxelHash()
{
	//delete[] buckets;

	//NOTE: check buckets are properly destroyed
}