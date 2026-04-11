/*=====================================================================
FormatDecoderSubVox.cpp
-----------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "FormatDecoderSubVox.h"


#include <utils/Exception.h>
#include <utils/StringUtils.h>
#include <utils/FileInStream.h>
#include <utils/FileOutStream.h>
#include <utils/BufferViewInStream.h>
#include <utils/RandomAccessInStream.h>
#include <utils/Vector.h>
#include <utils/Sort.h>
#include <utils/ArrayRef.h>
#include <utils/BufferInStream.h>
#include <utils/BufferOutStream.h>
#include <zstd.h>
#include <cstring> // for memcmp


static const char SUBVOX_MAGIC[8] = { 'S', 'u', 'b', 'V', 'o', 'x', '\0', '\0' };
static const uint32 SUBVOX_VERSION = 1;


static void readFromSubVoxFileStream(RandomAccessInStream& stream, SubVoxFileContents& content_out)
{
	// Read and verify magic bytes
	char magic[8];
	stream.readData(magic, 8);
	if(memcmp(magic, SUBVOX_MAGIC, 8) != 0)
		throw glare::Exception("Invalid SubVox file: incorrect magic bytes.");

	// Read and verify version
	content_out.version = stream.readUInt32();
	//if(content_out.version != SUBVOX_VERSION)
	//	throw glare::Exception("Unsupported SubVox version: " + toString(content_out.version));

	// Read AABB
	content_out.min_bounds.x = stream.readInt32();
	content_out.min_bounds.y = stream.readInt32();
	content_out.min_bounds.z = stream.readInt32();
	content_out.max_bounds.x = stream.readInt32();
	content_out.max_bounds.y = stream.readInt32();
	content_out.max_bounds.z = stream.readInt32();

	// Read num materials
	const uint32 num_mats = stream.readUInt32();
	content_out.materials_size = num_mats;

	// Read voxel counts for each material
	size_t total_num_voxels = 0;
	std::vector<uint32> mat_vox_counts(num_mats);
	for(size_t i=0; i<num_mats; ++i)
	{
		mat_vox_counts[i] = stream.readUInt32();
		total_num_voxels += mat_vox_counts[i];
	}

	if(total_num_voxels > (uint64)1024 * 1024 * 1024) // sanity check
		throw glare::Exception("too many voxels in SubVox file: " + toString(total_num_voxels));

	// Read compressed data size
	const uint64 compressed_size = stream.readUInt64();
	if(compressed_size > (uint64)1024 * 1024 * 1024) // 1 GB sanity check
		throw glare::Exception("SubVox compressed data size is too large: " + toString(compressed_size));

	if(!stream.canReadNBytes(compressed_size))
		throw glare::Exception("invalid compressed size");

	// Decompress data
	const uint64 decompressed_size = ZSTD_getFrameContentSize(stream.currentReadPtr(), compressed_size);
	if(decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN || decompressed_size == ZSTD_CONTENTSIZE_ERROR)
		throw glare::Exception("Failed to get decompressed_size");

	if(decompressed_size != total_num_voxels * 3 * sizeof(int))
		throw glare::Exception("Invalid decompressed_size");

	BufferInStream decompressed_instream;
	//if(mem_allocator)
	//	instream.buf.setAllocator(mem_allocator);
	decompressed_instream.buf.resizeNoCopy(decompressed_size);

	const size_t res = ZSTD_decompress(/*dest=*/decompressed_instream.buf.data(), /*dest capacity=*/decompressed_size, /*src=*/stream.currentReadPtr(), /*compressed size=*/compressed_size);
	if(ZSTD_isError(res))
		throw glare::Exception("Decompression of buffer failed: " + toString(res));
	if(res < decompressed_size)
		throw glare::Exception("Decompression of buffer failed: not enough bytes in result");

	stream.advanceReadIndex(compressed_size);

	
	// Unfilter voxel data, reading from decompressed_instream, writing to group_out.voxels.
	content_out.group.voxels.resizeNoCopy(total_num_voxels);

	Vec3<int> current_pos(0, 0, 0);

	size_t write_i = 0;
	for(uint32 m=0; m<num_mats; ++m)
	{
		const size_t count = mat_vox_counts[m]; // Number of voxels with this material.

		const Vec3<int>* const relative_positions = (const Vec3<int>*)decompressed_instream.currentReadPtr(); // Pointer should be sufficiently aligned.

		decompressed_instream.advanceReadIndex(sizeof(Vec3<int>) * count); // Advance past relative positions.  (Checks relative_positions pointer points to a valid range)

		for(size_t i=0; i<count; ++i)
		{
			const Vec3<int> pos = current_pos + relative_positions[i];

			content_out.group.voxels[write_i++] = SubVoxVoxel(pos, m);

			current_pos = pos;
		}
	}

	if(!decompressed_instream.endOfStream())
		throw glare::Exception("Didn't reach EOF while reading voxels.");
}


struct GetMatIndex
{
	size_t operator() (const SubVoxVoxel& v)
	{
		return (size_t)v.mat_index;
	}
};


static void writeAsSubVoxFileToStream(OutStream& file, const SubVoxVoxelGroup& group)
{
	// Compute number of material buckets (=max material index + 1), also compute bounds
	Vec3<int> min_pos(std::numeric_limits<int>::max());
	Vec3<int> max_pos(-std::numeric_limits<int>::max());
	size_t max_mat_index = 0;
	for(size_t i=0; i<group.voxels.size(); ++i)
	{
		max_mat_index = myMax<size_t>(max_mat_index, group.voxels[i].mat_index);
		min_pos = min(min_pos, group.voxels[i].pos);
		max_pos = max(max_pos, group.voxels[i].pos);
	}

	const Vec3<int> max_bounds = max_pos + Vec3i(1);

	// Write magic bytes
	file.writeData(SUBVOX_MAGIC, 8);

	// Write version
	file.writeUInt32(SUBVOX_VERSION);

	// Write AABB
	file.writeInt32(min_pos.x);
	file.writeInt32(min_pos.y);
	file.writeInt32(min_pos.z);
	file.writeInt32(max_bounds.x);
	file.writeInt32(max_bounds.y);
	file.writeInt32(max_bounds.z);

	// Compress and write voxel data
	{
		const size_t num_mat_buckets = max_mat_index + 1;

		// Step 1: sort by materials
		glare::AllocatorVector<SubVoxVoxel, 16> sorted_voxels(group.voxels.size());
		Sort::serialCountingSortWithNumBuckets(group.voxels.data(), sorted_voxels.data(), group.voxels.size(), num_mat_buckets, GetMatIndex());

		// Count num voxels in each material bucket
		std::vector<size_t> counts(num_mat_buckets, 0);
		for(size_t i=0; i<group.voxels.size(); ++i)
			counts[group.voxels[i].mat_index]++;

		file.writeUInt32((uint32)counts.size()); // Write num materials

		// For each material, write num voxels with that material
		for(size_t z=0; z<counts.size(); ++z)
			file.writeUInt32((uint32)counts[z]); // Write num voxels with material z

		// Now write relative (filtered) position voxel data to data
		Vec3<int> current_pos(0, 0, 0);
		size_t v_i = 0;
		js::Vector<int, 16> filtered_data(group.voxels.size() * 3);
		size_t write_i = 0;
		for(size_t z=0; z<counts.size(); ++z)
		{
			const size_t count = counts[z];

			for(size_t i=0; i<count; ++i)
			{
				const Vec3<int> relative_pos = sorted_voxels[v_i].pos - current_pos;

				filtered_data[write_i++] = relative_pos.x;
				filtered_data[write_i++] = relative_pos.y;
				filtered_data[write_i++] = relative_pos.z;

				current_pos = sorted_voxels[v_i].pos;

				v_i++;
			}
		}

		assert(write_i == filtered_data.size());

		// Compress filtered data
		const size_t compressed_bound = ZSTD_compressBound(filtered_data.dataSizeBytes());
		
		js::Vector<uint8> compressed_data(compressed_bound);
	
		const size_t compressed_size = ZSTD_compress(/*dest=*/compressed_data.data(), /*dest capacity=*/compressed_data.size(), /*src=*/filtered_data.data(), /*src size=*/filtered_data.dataSizeBytes(),
			ZSTD_CLEVEL_DEFAULT // compression level
		);
		if(ZSTD_isError(compressed_size))
			throw glare::Exception("Compression failed: " + toString(compressed_size));

		compressed_data.resize(compressed_size);

		// Write compressed data size and data
		file.writeUInt64(compressed_data.size());
		file.writeData(compressed_data.data(), compressed_data.size());
	}
}


void FormatDecoderSubVox::writeSubVoxFile(const std::string& path, const SubVoxVoxelGroup& group)
{
	FileOutStream file(path);
	writeAsSubVoxFileToStream(file, group);
}


void FormatDecoderSubVox::readSubVoxFile(const std::string& path, SubVoxFileContents& content_out)
{
	FileInStream file(path);
	readFromSubVoxFileStream(file, content_out);
}


void FormatDecoderSubVox::readSubVoxFileFromData(const uint8* data, size_t datalen, SubVoxFileContents& content_out)
{
	BufferViewInStream stream(ArrayRef<uint8>(data, datalen));
	readFromSubVoxFileStream(stream, content_out);
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


static Vec3<int> computeMinBounds(const SubVoxVoxelGroup& group)
{
	Vec3<int> min_pos(std::numeric_limits<int>::max());
	for(size_t i=0; i<group.voxels.size(); ++i)
		min_pos = min(min_pos, group.voxels[i].pos);
	return min_pos;
}

static Vec3<int> computeMaxBounds(const SubVoxVoxelGroup& group)
{
	Vec3<int> max_pos(-std::numeric_limits<int>::max());
	for(size_t i=0; i<group.voxels.size(); ++i)
		max_pos = max(max_pos, group.voxels[i].pos);
	return max_pos + Vec3i(1);
}

static int computeMaterialsSize(const SubVoxVoxelGroup& group)
{
	int max_mat_index = 0;
	for(size_t i=0; i<group.voxels.size(); ++i)
		max_mat_index = myMax(max_mat_index, group.voxels[i].mat_index);
	return max_mat_index + 1;
}


static void testWritingAndReadingGroup(const SubVoxVoxelGroup& group)
{
	try
	{
		// Write to a buffer
		BufferOutStream out_buffer;
		writeAsSubVoxFileToStream(out_buffer, group);

		// Test reading back
		BufferViewInStream in_stream(ArrayRef<uint8>(out_buffer.buf.data(), out_buffer.buf.size()));

		SubVoxFileContents content;
		readFromSubVoxFileStream(in_stream, content);

		testAssert(content.version == 1);
		testAssert(group.voxels == content.group.voxels);
		testEqual(content.min_bounds, computeMinBounds(group));
		testEqual(content.max_bounds, computeMaxBounds(group));
		testAssert(content.materials_size == computeMaterialsSize(group));
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void FormatDecoderSubVox::test()
{
	conPrint("FormatDecoderSubVox::test()");

	//---------------- Test empty voxel group ----------------
	{
		SubVoxVoxelGroup group;
		testWritingAndReadingGroup(group);
	}

	//---------------- Test with 1 voxel ----------------
	{
		SubVoxVoxelGroup group;
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,0,0), /*mat index=*/0));

		testWritingAndReadingGroup(group);
	}

	//---------------- Test with multiple voxels ----------------
	{
		SubVoxVoxelGroup group;
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,0,0), /*mat index=*/0));
		group.voxels.push_back(SubVoxVoxel(Vec3i(1,0,0), /*mat index=*/0));
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,1,0), /*mat index=*/0));

		testWritingAndReadingGroup(group);
	}

	//---------------- Test with multiple voxels and multiple materials ----------------
	{
		SubVoxVoxelGroup group;
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,0,0), /*mat index=*/0));
		group.voxels.push_back(SubVoxVoxel(Vec3i(1,0,0), /*mat index=*/1));
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,1,0), /*mat index=*/2));

		testWritingAndReadingGroup(group);
	}
	{
		SubVoxVoxelGroup group;
		group.voxels.push_back(SubVoxVoxel(Vec3i(-10,0,-30), /*mat index=*/0));
		group.voxels.push_back(SubVoxVoxel(Vec3i(1,0,0), /*mat index=*/10));
		group.voxels.push_back(SubVoxVoxel(Vec3i(0,1,0), /*mat index=*/20));

		testWritingAndReadingGroup(group);
	}

	conPrint("FormatDecoderSubVox::test() done.");
}


#endif
