/*=====================================================================
FormatDecoderSubVox.h
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../physics/jscol_aabbox.h"
#include <string>


class SubVoxVoxel
{
public:
	SubVoxVoxel(const Vec3<int>& pos_, int mat_index_) : pos(pos_), mat_index(mat_index_) {}
	SubVoxVoxel() {}

	bool operator == (const SubVoxVoxel& other) const { return pos == other.pos && mat_index == other.mat_index; }
	bool operator != (const SubVoxVoxel& other) const { return pos != other.pos || mat_index != other.mat_index; }

	Vec3<int> pos;
	int mat_index; // Index into materials
};


class SubVoxVoxelGroup
{
public:
	// Iterate over voxels and get voxel position bounds
	js::AABBox getAABB() const;

	glare::AllocatorVector<SubVoxVoxel, 16> voxels;
};


struct SubVoxFileContents
{
	uint32 version;

	SubVoxVoxelGroup group;

	Vec3<int> min_bounds;
	Vec3<int> max_bounds;

	size_t materials_size; // = max material index + 1.
};


/*=====================================================================
FormatDecoderSubVox
-------------------
SubVox (.subvox) voxel file format.

File layout:
  [0..7]   Magic bytes: "SubVox\0\0" (8 bytes)
  [8..11]  Version: uint32 = 1
  [12..23] AABB min (3 int32s: x, y, z)
  [24..35] AABB max (3 int32s: x, y, z)
  ...
=====================================================================*/
class FormatDecoderSubVox
{
public:
	// Throws glare::Exception on failure.
	static void writeSubVoxFile(const std::string& path, const SubVoxVoxelGroup& group);

	// Throws glare::Exception on failure.
	static void readSubVoxFile(const std::string& path, SubVoxFileContents& content_out);

	// Throws glare::Exception on failure.
	static void readSubVoxFileFromData(const uint8* data, size_t datalen, SubVoxFileContents& content_out);

	static void test();
};
