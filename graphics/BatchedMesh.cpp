/*=====================================================================
BatchedMesh.cpp
---------------
Copyright Glare Technologies Limited 2020
=====================================================================*/
#include "BatchedMesh.h"


#include "../dll/include/IndigoMesh.h"
#include "../maths/vec3.h"
#include "../maths/vec2.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"
#include "../utils/FileInStream.h"
#include "../utils/FileOutStream.h"
#include "../utils/IncludeHalf.h"
#include <limits>
#include <zstd.h>


BatchedMesh::BatchedMesh()
{
}


BatchedMesh::~BatchedMesh()
{
}


struct BMeshTakeFirstElement
{
	inline uint32 operator() (const std::pair<uint32, uint32>& pair) const { return pair.first; }
};


struct BMeshUVsAtVert
{
	BMeshUVsAtVert() : merged_v_index(-1) {}

	Vec2f uv;
	int merged_v_index;
};


inline static uint32 BMeshPackNormal(const Indigo::Vec3f& normal)
{
	int x = (int)(normal.x * 511.f);
	int y = (int)(normal.y * 511.f);
	int z = (int)(normal.z * 511.f);
	// ANDing with 1023 isolates the bottom 10 bits.
	return (x & 1023) | ((y & 1023) << 10) | ((z & 1023) << 20);
}


// Requires dest and src to be 4-byte aligned.
// size is in bytes.
inline static void copyUInt32s(void* const dest, const void* const src, size_t size_B)
{
	assert(((uint64)dest % 4 == 0) && ((uint64)src % 4 == 0));

	//for(size_t z=0; z<size_B; z += sizeof(uint32))
	//	*((uint32*)(dest + z)) = *((const uint32*)(src + z));

	const size_t num_uints = size_B / 4;

	for(size_t z=0; z<num_uints; ++z)
		*((uint32*)dest + z) = *((const uint32*)src + z);
}


// Adapted from OpenGLEngine::buildIndigoMesh().
void BatchedMesh::buildFromIndigoMesh(const Indigo::Mesh& mesh_)
{
	const Indigo::Mesh* const mesh				= &mesh_;
	const Indigo::Triangle* const tris			= mesh->triangles.data();
	const size_t num_tris						= mesh->triangles.size();
	const Indigo::Quad* const quads				= mesh->quads.data();
	const size_t num_quads						= mesh->quads.size();
	const Indigo::Vec3f* const vert_positions	= mesh->vert_positions.data();
	const size_t vert_positions_size			= mesh->vert_positions.size();
	const Indigo::Vec3f* const vert_normals		= mesh->vert_normals.data();
	const Indigo::Vec3f* const vert_colours		= mesh->vert_colours.data();
	const Indigo::Vec2f* const uv_pairs			= mesh->uv_pairs.data();
	const size_t uvs_size						= mesh->uv_pairs.size();

	const bool mesh_has_shading_normals			= !mesh->vert_normals.empty();
	bool mesh_has_uvs							= mesh->num_uv_mappings > 0;
	const uint32 num_uv_sets					= mesh->num_uv_mappings;
	const bool mesh_has_vert_cols				= !mesh->vert_colours.empty();


	// If UVs are somewhat small in magnitude, use GL_HALF_FLOAT instead of GL_FLOAT.
	// If the magnitude is too high we can get articacts if we just use half precision.
	const bool use_half_uvs = false; // TEMP canUseHalfUVs(mesh);

	const size_t pos_size = sizeof(float)*3;
#if NO_PACKED_NORMALS // GL_INT_2_10_10_10_REV is not present in our OS X header files currently.
	const size_t packed_normal_size = sizeof(float)*3;
#else
	const size_t packed_normal_size = 4; // 4 bytes since we are using GL_INT_2_10_10_10_REV format.
#endif
	const size_t packed_uv_size = use_half_uvs ? sizeof(half)*2 : sizeof(float)*2;

	/*
	Vertex data layout is
	position [always present]
	normal   [optional]
	uv_0     [optional]
	colour   [optional]
	*/
	const size_t normal_offset      = pos_size;
	const size_t uv_offset          = normal_offset   + (mesh_has_shading_normals ? packed_normal_size : 0);
	const size_t vert_col_offset    = uv_offset       + (mesh_has_uvs ? packed_uv_size : 0);
	const size_t num_bytes_per_vert = vert_col_offset + (mesh_has_vert_cols ? sizeof(float)*3 : 0);
	js::Vector<uint8, 16>& vert_data = this->vertex_data;
	vert_data.reserve(mesh->vert_positions.size() * num_bytes_per_vert);

	js::Vector<uint32, 16> uint32_indices(mesh->triangles.size() * 3 + mesh->quads.size() * 6);

	size_t vert_index_buffer_i = 0; // Current write index into vert_index_buffer
	size_t next_merged_vert_i = 0;
	size_t last_pass_start_index = 0;
	uint32 current_mat_index = std::numeric_limits<uint32>::max();

	std::vector<BMeshUVsAtVert> uvs_at_vert(mesh->vert_positions.size());

	if(mesh->triangles.size() > 0)
	{
		// Create list of triangle references sorted by material index
		js::Vector<std::pair<uint32, uint32>, 16> unsorted_tri_indices(num_tris);
		js::Vector<std::pair<uint32, uint32>, 16> tri_indices(num_tris); // Sorted by material

		for(uint32 t = 0; t < num_tris; ++t)
			unsorted_tri_indices[t] = std::make_pair(tris[t].tri_mat_index, t);

		Sort::serialCountingSort(/*in=*/unsorted_tri_indices.data(), /*out=*/tri_indices.data(), num_tris, BMeshTakeFirstElement());

		for(uint32 t = 0; t < num_tris; ++t)
		{
			// If we've switched to a new material then start a new triangle range
			if(tri_indices[t].first != current_mat_index)
			{
				if(t > 0) // Don't add zero-length passes.
				{
					IndicesBatch batch;
					batch.material_index = current_mat_index;
					batch.indices_start = (uint32)(last_pass_start_index);
					batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
					this->batches.push_back(batch);
				}
				last_pass_start_index = vert_index_buffer_i;
				current_mat_index = tri_indices[t].first;
			}

			const Indigo::Triangle& tri = tris[tri_indices[t].second];
			for(uint32 i = 0; i < 3; ++i) // For each vert in tri:
			{
				const uint32 pos_i		= tri.vertex_indices[i];
				const uint32 base_uv_i	= tri.uv_indices[i];
				const uint32 uv_i = base_uv_i * num_uv_sets; // Index of UV for UV set 0.
				if(pos_i >= vert_positions_size)
					throw Indigo::Exception("vert index out of bounds");
				if(mesh_has_uvs && uv_i >= uvs_size)
					throw Indigo::Exception("UV index out of bounds");

				// Look up merged vertex
				const Vec2f uv = mesh_has_uvs ? Vec2f(uv_pairs[uv_i].x, uv_pairs[uv_i].y) : Vec2f(0.f);

				BMeshUVsAtVert& at_vert = uvs_at_vert[pos_i];
				const bool found = at_vert.merged_v_index != -1 && at_vert.uv == uv;

				uint32 merged_v_index;
				if(!found) // Not created yet:
				{
					merged_v_index = (uint32)next_merged_vert_i++;

					if(at_vert.merged_v_index == -1) // If there is no UV inserted yet for this vertex position:
					{
						at_vert.uv = uv; // Insert it
						at_vert.merged_v_index = merged_v_index;
					}

					const size_t cur_size = vert_data.size();
					vert_data.resize(cur_size + num_bytes_per_vert);
					copyUInt32s(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f)); // Copy vert position

					if(mesh_has_shading_normals)
					{
						const uint32 n = BMeshPackNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
						copyUInt32s(&vert_data[cur_size + normal_offset], &n, 4);
					}

					if(mesh_has_uvs)
					{
						if(use_half_uvs)
						{
							half half_uv[2];
							half_uv[0] = half(uv.x);
							half_uv[1] = half(uv.y);
							copyUInt32s(&vert_data[cur_size + uv_offset], half_uv, 4);
						}
						else
							copyUInt32s(&vert_data[cur_size + uv_offset], &uv.x, sizeof(Indigo::Vec2f));
					}

					if(mesh_has_vert_cols)
						copyUInt32s(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
				}
				else
				{
					merged_v_index = at_vert.merged_v_index; // Else a vertex with this position index and UV has already been created, use it
				}

				uint32_indices[vert_index_buffer_i++] = (uint32)merged_v_index;
			}
		}
	}

	if(mesh->quads.size() > 0)
	{
		// Create list of quad references sorted by material index
		js::Vector<std::pair<uint32, uint32>, 16> unsorted_quad_indices(num_quads);
		js::Vector<std::pair<uint32, uint32>, 16> quad_indices(num_quads); // Sorted by material

		for(uint32 q = 0; q < num_quads; ++q)
			unsorted_quad_indices[q] = std::make_pair(quads[q].mat_index, q);

		Sort::serialCountingSort(/*in=*/unsorted_quad_indices.data(), /*out=*/quad_indices.data(), num_quads, BMeshTakeFirstElement());

		for(uint32 q = 0; q < num_quads; ++q)
		{
			// If we've switched to a new material then start a new quad range
			if(quad_indices[q].first != current_mat_index)
			{
				if(vert_index_buffer_i > last_pass_start_index) // Don't add zero-length passes.
				{
					IndicesBatch batch;
					batch.material_index = current_mat_index;
					batch.indices_start = (uint32)(last_pass_start_index);
					batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
					this->batches.push_back(batch);
				}
				last_pass_start_index = vert_index_buffer_i;
				current_mat_index = quad_indices[q].first;
			}

			const Indigo::Quad& quad = quads[quad_indices[q].second];
			uint32 vert_merged_index[4];
			for(uint32 i = 0; i < 4; ++i) // For each vert in quad:
			{
				const uint32 pos_i  = quad.vertex_indices[i];
				const uint32 uv_i   = quad.uv_indices[i];
				if(pos_i >= vert_positions_size)
					throw Indigo::Exception("vert index out of bounds");
				if(mesh_has_uvs && uv_i >= uvs_size)
					throw Indigo::Exception("UV index out of bounds");

				// Look up merged vertex
				const Vec2f uv = mesh_has_uvs ? Vec2f(uv_pairs[uv_i].x, uv_pairs[uv_i].y) : Vec2f(0.f);

				BMeshUVsAtVert& at_vert = uvs_at_vert[pos_i];
				const bool found = at_vert.merged_v_index != -1 && at_vert.uv == uv;

				uint32 merged_v_index;
				if(!found)
				{
					merged_v_index = (uint32)next_merged_vert_i++;

					if(at_vert.merged_v_index == -1) // If there is no UV inserted yet for this vertex position:
					{
						at_vert.uv = uv; // Insert it
						at_vert.merged_v_index = merged_v_index;
					}

					const size_t cur_size = vert_data.size();
					vert_data.resize(cur_size + num_bytes_per_vert);
					copyUInt32s(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f));
					if(mesh_has_shading_normals)
					{
						const uint32 n = BMeshPackNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
						copyUInt32s(&vert_data[cur_size + normal_offset], &n, 4);
					}

					if(mesh_has_uvs)
					{
						if(use_half_uvs)
						{
							half half_uv[2];
							half_uv[0] = half(uv.x);
							half_uv[1] = half(uv.y);
							copyUInt32s(&vert_data[cur_size + uv_offset], half_uv, 4);
						}
						else
							copyUInt32s(&vert_data[cur_size + uv_offset], &uv_pairs[uv_i].x, sizeof(Indigo::Vec2f));
					}

					if(mesh_has_vert_cols)
						copyUInt32s(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
				}
				else
				{
					merged_v_index = at_vert.merged_v_index; // Else a vertex with this position index and UV has already been created, use it.
				}

				vert_merged_index[i] = merged_v_index;
			} // End for each vert in quad:

			// Tri 1 of quad
			uint32_indices[vert_index_buffer_i + 0] = vert_merged_index[0];
			uint32_indices[vert_index_buffer_i + 1] = vert_merged_index[1];
			uint32_indices[vert_index_buffer_i + 2] = vert_merged_index[2];
			// Tri 2 of quad
			uint32_indices[vert_index_buffer_i + 3] = vert_merged_index[0];
			uint32_indices[vert_index_buffer_i + 4] = vert_merged_index[2];
			uint32_indices[vert_index_buffer_i + 5] = vert_merged_index[3];

			vert_index_buffer_i += 6;
		}
	}

	// Build last pass data that won't have been built yet.
	IndicesBatch batch;
	batch.material_index = current_mat_index;
	batch.indices_start = (uint32)(last_pass_start_index);
	batch.num_indices = (uint32)(vert_index_buffer_i - last_pass_start_index);
	this->batches.push_back(batch);

	const size_t num_merged_verts = next_merged_vert_i;

	// Build index data
	const size_t num_indices = uint32_indices.size();

	if(num_merged_verts < 128)
	{
		this->index_type = ComponentType_UInt8;

		index_data.resize(num_indices * sizeof(uint8));

		uint8* const dest_indices = index_data.data();
		for(size_t i=0; i<num_indices; ++i)
			dest_indices[i] = (uint8)uint32_indices[i];
	}
	else if(num_merged_verts < 32768)
	{
		this->index_type = ComponentType_UInt16;

		index_data.resize(num_indices * sizeof(uint16));

		uint16* const dest_indices = (uint16*)index_data.data();
		for(size_t i=0; i<num_indices; ++i)
			dest_indices[i] = (uint16)uint32_indices[i];
	}
	else
	{
		this->index_type = ComponentType_UInt32;

		index_data.resize(num_indices * sizeof(uint32));

		uint32* const dest_indices = (uint32*)index_data.data();
		for(size_t i=0; i<num_indices; ++i)
			dest_indices[i] = uint32_indices[i];
	}


	VertAttribute pos_attrib;
	pos_attrib.type = VertAttribute_Position;
	pos_attrib.component_type = ComponentType_Float;
	vert_attributes.push_back(pos_attrib);

	if(mesh_has_shading_normals)
	{
		VertAttribute normal_attrib;
		normal_attrib.type = VertAttribute_Normal;
		normal_attrib.component_type = ComponentType_PackedNormal;
		vert_attributes.push_back(normal_attrib);
	}

	if(num_uv_sets >= 1)
	{
		VertAttribute uv_attrib;
		uv_attrib.type = VertAttribute_UV_0;
		uv_attrib.component_type = ComponentType_Float;
		vert_attributes.push_back(uv_attrib);
	}
	if(num_uv_sets >= 2)
	{
		VertAttribute uv_attrib;
		uv_attrib.type = VertAttribute_UV_1;
		uv_attrib.component_type = ComponentType_Float;
		vert_attributes.push_back(uv_attrib);
	}

	if(mesh_has_vert_cols)
	{
		VertAttribute colour_attrib;
		colour_attrib.type = VertAttribute_Colour;
		colour_attrib.component_type = ComponentType_Float;
		vert_attributes.push_back(colour_attrib);
	}


	aabb_os.min_ = Vec4f(mesh->aabb_os.bound[0].x, mesh->aabb_os.bound[0].y, mesh->aabb_os.bound[0].z, 1.f);
	aabb_os.max_ = Vec4f(mesh->aabb_os.bound[1].x, mesh->aabb_os.bound[1].y, mesh->aabb_os.bound[1].z, 1.f);
}


bool BatchedMesh::operator == (const BatchedMesh& other) const
{
	return
		vert_attributes == other.vert_attributes &&
		batches == other.batches &&
		index_type == other.index_type &&
		index_data == other.index_data &&
		vertex_data == other.vertex_data &&
		aabb_os == other.aabb_os;
}


static const uint32 MAGIC_NUMBER = 12456751;
static const uint32 FORMAT_VERSION = 1;


static const uint32 FLAG_USE_COMPRESSION = 1;


struct BatchedMeshHeader
{
	uint32 magic_number;
	uint32 format_version;
	uint32 header_size;
	uint32 flags;

	uint32 num_vert_attributes;
	uint32 num_batches;
	uint32 index_type;
	uint32 index_data_size_B;
	uint32 vertex_data_size_B;

	Vec3f aabb_min;
	Vec3f aabb_max;
};
static_assert(sizeof(BatchedMeshHeader) == sizeof(uint32) * 15, "sizeof(BatchedMeshHeader) == sizeof(uint32) * 15");


void BatchedMesh::writeToFile(const std::string& dest_path, const WriteOptions& write_options) const // throws Indigo::Exception on failure
{
	//Timer write_timer;

	FileOutStream file(dest_path);

	BatchedMeshHeader header;
	header.magic_number = MAGIC_NUMBER;
	header.format_version = FORMAT_VERSION;
	header.header_size = sizeof(BatchedMeshHeader);
	header.flags = write_options.use_compression ? FLAG_USE_COMPRESSION : 0;
	header.num_vert_attributes = (uint32)vert_attributes.size();
	header.num_batches = (uint32)batches.size();
	header.index_type = (uint32)index_type;
	header.index_data_size_B = (uint32)index_data.size();
	header.vertex_data_size_B = (uint32)vertex_data.size();
	header.aabb_min = Vec3f(aabb_os.min_);
	header.aabb_max = Vec3f(aabb_os.max_);

	file.writeData(&header, sizeof(BatchedMeshHeader));

	// Write vert attributes
	for(size_t i=0; i<vert_attributes.size(); ++i)
	{
		file.writeUInt32((uint32)vert_attributes[i].type);
		file.writeUInt32((uint32)vert_attributes[i].component_type);
	}

	// Write batches
	if(!batches.empty())
		file.writeData(batches.data(), batches.size() * sizeof(IndicesBatch));

	
	// Write the rest of the data compressed
	if(write_options.use_compression)
	{
		// We will write the index data and vertex data into separate compressed buffers, for a few reasons:
		// * Since we're processing in two parts, can reuse smaller buffers.
		// * Buffers for each part can be 4-byte aligned, without using padding
		// * Resulting compressed size is smaller (as measured with perf tests)

		js::Vector<uint8, 16> filtered_data_vec;
		filtered_data_vec.reserve(myMax(index_data.size(), vertex_data.size()));

		const size_t compressed_bound = myMax(ZSTD_compressBound(index_data.size()), ZSTD_compressBound(vertex_data.size())); // Make the buffer large enough that we don't need to resize it later.
		js::Vector<uint8, 16> compressed_data(compressed_bound);

		size_t total_compressed_size = 0;
		Timer timer;
		timer.pause();

		{
			// Build filtered index data.
			filtered_data_vec.resizeNoCopy(index_data.size());
			
			uint32 last_index = 0;
			const size_t num_indices = index_data.size() / componentTypeSize(index_type);
			if(index_type == ComponentType_UInt8)
			{
				const uint8* const index_data_uint8    = (const uint8*)index_data.data();
					  uint8* const filtered_index_data = (uint8*)filtered_data_vec.data();
				for(size_t i=0; i<num_indices; ++i)
				{
					const uint8 index = index_data_uint8[i];
					assert(index <= (uint8)std::numeric_limits<int8>::max());
					const int8 rel_index = (int8)((int32)index - (int32)last_index);
					filtered_index_data[i] = rel_index;
					last_index = index;
				}
			}
			else if(index_type == ComponentType_UInt16)
			{
				const uint16* const index_data_uint16   = (const uint16*)index_data.data();
					  uint16* const filtered_index_data = (uint16*)filtered_data_vec.data();
				for(size_t i=0; i<num_indices; ++i)
				{
					const uint16 index = index_data_uint16[i];
					assert(index <= (uint16)std::numeric_limits<int16>::max());
					const int16 rel_index = (int16)((int32)index - (int32)last_index);
					filtered_index_data[i] = rel_index;
					last_index = index;
				}
			}
			else if(index_type == ComponentType_UInt32)
			{
				const uint32* const index_data_uint32   = (const uint32*)index_data.data();
					  uint32* const filtered_index_data = (uint32*)filtered_data_vec.data();
				for(size_t i=0; i<num_indices; ++i)
				{
					const uint32 index = index_data_uint32[i];
					assert(index <= (uint32)std::numeric_limits<int32>::max());
					const int32 rel_index = (int32)((int32)index - (int32)last_index);
					filtered_index_data[i] = rel_index;
					last_index = index;
				}
			}
			else
			{
				assert(0);
			}

			// Compress the index buffer with zstandard
			timer.unpause();
			const size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), filtered_data_vec.data(), filtered_data_vec.size(),
				write_options.compression_level // compression level
			);
			if(ZSTD_isError(compressed_size))
				throw Indigo::Exception("Compression failed: " + toString(compressed_size));
			timer.pause();

			// Write compressed size as uint64
			file.writeUInt64(compressed_size);

			// Now write compressed data to disk
			file.writeData(compressed_data.data(), compressed_size);
		
			total_compressed_size += compressed_size;
		}

		// Build de-interleaved vertex data, compress it and then write it to disk.
		{
			filtered_data_vec.resizeNoCopy(vertex_data.size());

			/*
			Separate interleaved vertex data into separate arrays

			p0 n0 c0 p1 n1 c1 p2 n2 c2 p3 n3 c3 ... pN nN cN
			=>
			p0 p1 p2 p3 ... pN n0 n1 n2 n3 ... nN c0 c1 c2 c3 ... cN
			*/
			const size_t vert_size = vertexSize(); // in bytes
			assert(vert_size % 4 == 0);
			const size_t num_verts = vertex_data.size() / vert_size;

			size_t attr_offset = 0;
			uint8* dst_ptr = filtered_data_vec.data();
			for(size_t b=0; b<vert_attributes.size(); ++b)
			{
				const size_t attr_size = vertAttributeSize(vert_attributes[b]);
				assert(attr_size % 4 == 0);
				const uint8* src_ptr = vertex_data.data() + attr_offset;

				for(size_t i=0; i<num_verts; ++i) // For each vertex
				{
					// Copy data for this attribute, for this vertex, to filtered_data
					assert(src_ptr + attr_size <= vertex_data.data() + vertex_data.size());
					assert(dst_ptr + attr_size <= filtered_data_vec.data() + filtered_data_vec.size());

					copyUInt32s(dst_ptr, src_ptr, attr_size);

					src_ptr += vert_size;
					dst_ptr += attr_size;
				}

				attr_offset += attr_size;
			}

			// Compress the vertex data with zstandard
			timer.unpause();
			const size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), filtered_data_vec.data(), filtered_data_vec.size(),
				write_options.compression_level // compression level
			);
			if(ZSTD_isError(compressed_size))
				throw Indigo::Exception("Compression failed: " + toString(compressed_size));
			timer.pause();

			// Write compressed size as uint64
			file.writeUInt64(compressed_size);

			// Now write compressed data to disk
			file.writeData(compressed_data.data(), compressed_size);

			total_compressed_size += compressed_size;
		}

		
		const size_t uncompressed_size = index_data.size() + vertex_data.size();
		const double compression_speed = uncompressed_size / timer.elapsed();
		conPrint("");
		conPrint("Uncompressed size:   " + toString(uncompressed_size) + " B");
		conPrint("Compressed size:     " + toString(total_compressed_size) + " B");
		conPrint("Compression ratio:   " + doubleToStringNSigFigs((double)uncompressed_size / total_compressed_size, 4));
		conPrint("Compression took     " + timer.elapsedStringNSigFigs(4) + " (" + doubleToStringNSigFigs(compression_speed / (1024ull*1024ull), 4) + " MB/s)");
	}
	else
	{
		file.writeData(index_data.data(),  index_data.dataSizeBytes());
		file.writeData(vertex_data.data(), vertex_data.dataSizeBytes());
	}

	//conPrint("Total time to write to disk: " + write_timer.elapsedString());
}


static const uint32 MAX_NUM_VERT_ATTRIBUTES = 100;
static const uint32 MAX_NUM_BATCHES = 1000000;


void BatchedMesh::readFromFile(const std::string& src_path, BatchedMesh& mesh_out) // throws IndigoException on failure
{
	try
	{
		FileInStream file(src_path);

		// Read header
		BatchedMeshHeader header;
		file.readData(&header, sizeof(header));

		if(header.magic_number != MAGIC_NUMBER)
			throw Indigo::Exception("Invalid magic number.");

		if(header.format_version < FORMAT_VERSION)
			throw Indigo::Exception("Unsupported format version " + toString(header.format_version) + ".");

		
		// Skip past rest of header
		if(header.header_size > 10000 || header.header_size > file.fileSize())
			throw Indigo::Exception("Header size too large.");
		file.setReadIndex(header.header_size);


		// Read vert attributes
		if(header.num_vert_attributes == 0)
			throw Indigo::Exception("Zero vert attributes.");
		if(header.num_vert_attributes > MAX_NUM_VERT_ATTRIBUTES)
			throw Indigo::Exception("Too many vert attributes.");
		
		mesh_out.vert_attributes.resize(header.num_vert_attributes);
		for(size_t i=0; i<mesh_out.vert_attributes.size(); ++i)
		{
			const uint32 type = file.readUInt32();
			if(type > MAX_VERT_ATTRIBUTE_TYPE_VALUE)
				throw Indigo::Exception("Invalid vert attribute type value.");
			mesh_out.vert_attributes[i].type = (VertAttributeType)type;

			const uint32 component_type = file.readUInt32();
			if(type > MAX_COMPONENT_TYPE_VALUE)
				throw Indigo::Exception("Invalid vert attribute component type value.");
			mesh_out.vert_attributes[i].component_type = (ComponentType)component_type;
		}

		// Read batches
		if(header.num_batches > MAX_NUM_BATCHES)
			throw Indigo::Exception("Too many batches.");

		mesh_out.batches.resize(header.num_batches);
		file.readData(mesh_out.batches.data(), mesh_out.batches.size() * sizeof(IndicesBatch));


		// Check header index type
		if(header.index_type > MAX_COMPONENT_TYPE_VALUE)
			throw Indigo::Exception("Invalid index type value.");

		mesh_out.index_type = (ComponentType)header.index_type;

		// Check total index data size is a multiple of each index size.
		if(header.index_data_size_B % componentTypeSize(mesh_out.index_type) != 0)
			throw Indigo::Exception("Invalid index_data_size_B.");

		mesh_out.index_data.resize(header.index_data_size_B); // TODO: size check? 32-bit limit of index_data_size_B may be enough.

		// Check total vert data size is a multiple of each vertex size.  Note that vertexSize() should be > 0 since we have set mesh_out.vert_attributes and checked there is at least one attribute.
		if(header.vertex_data_size_B % mesh_out.vertexSize() != 0)
			throw Indigo::Exception("Invalid vertex_data_size_B.");

		mesh_out.vertex_data.resize(header.vertex_data_size_B); // TODO: size check? 32-bit limit of vertex_data_size_B may be enough.

		mesh_out.aabb_os.min_ = Vec4f(header.aabb_min.x, header.aabb_min.y, header.aabb_min.z, 1.f);
		mesh_out.aabb_os.max_ = Vec4f(header.aabb_max.x, header.aabb_max.y, header.aabb_max.z, 1.f);
		// TODO: require AABB values finite?

		const bool compression = (header.flags & FLAG_USE_COMPRESSION) != 0;
		if(compression)
		{
			js::Vector<uint8, 16> plaintext(myMax(header.index_data_size_B, header.vertex_data_size_B)); // Make sure large enough so we don't need to resize later.

			Timer timer;
			timer.pause();
			{
				const uint64 index_data_compressed_size = file.readUInt64();
				if((index_data_compressed_size >= file.fileSize()) || (file.getReadIndex() + index_data_compressed_size > file.fileSize())) // Check index_data_compressed_size is valid, while taking care with wraparound
					throw Indigo::Exception("index_data_compressed_size was invalid.");

				// Decompress index data into plaintext buffer.
				timer.unpause();
				const size_t res = ZSTD_decompress(plaintext.begin(), header.index_data_size_B, file.currentReadPtr(), index_data_compressed_size);
				if(ZSTD_isError(res))
					throw Indigo::Exception("Decompression of index buffer failed: " + toString(res));
				if(res < (size_t)header.index_data_size_B)
					throw Indigo::Exception("Decompression of index buffer failed: not enough bytes in result");
				timer.pause();

				// Unfilter indices, place in mesh_out.index_data.
				const size_t num_indices = header.index_data_size_B / componentTypeSize((ComponentType)header.index_type);
				if(header.index_type == ComponentType_UInt8)
				{
					int32 last_index = 0;
					const int8* filtered_index_data_int8 = (const int8*)plaintext.data();
					uint8* index_data = (uint8*)mesh_out.index_data.data();
					for(size_t i=0; i<num_indices; ++i)
					{
						int8 index = (int8)last_index + filtered_index_data_int8[i];
						index_data[i] = index;
						last_index = index;
					}
				}
				else if(header.index_type == ComponentType_UInt16)
				{
					int32 last_index = 0;
					const int16* filtered_index_data_int16 = (const int16*)plaintext.data(); // Note that we know plaintext.data() is 16-byte aligned.
					uint16* index_data = (uint16*)mesh_out.index_data.data();
					for(size_t i=0; i<num_indices; ++i)
					{
						int16 index = (int16)last_index + filtered_index_data_int16[i];
						index_data[i] = index;
						last_index = index;
					}
				}
				else if(header.index_type == ComponentType_UInt32)
				{
					int32 last_index = 0;
					const int32* filtered_index_data_int32 = (const int32*)plaintext.data(); // Note that we know plaintext.data() is 16-byte aligned.
					uint32* index_data = (uint32*)mesh_out.index_data.data();
					for(size_t i=0; i<num_indices; ++i)
					{
						int32 index = (int32)last_index + filtered_index_data_int32[i];
						index_data[i] = index;
						last_index = index;
					}
				}
				else
					throw Indigo::Exception("Invalid index component type " + toString((int)header.index_type));

				file.setReadIndex(file.getReadIndex() + index_data_compressed_size);
			}

		
			// Decompress and de-filter vertex data.
			{
				const uint64 vertex_data_compressed_size = file.readUInt64();
				if((vertex_data_compressed_size >= file.fileSize()) || (file.getReadIndex() + vertex_data_compressed_size > file.fileSize())) // Check vertex_data_compressed_size is valid, while taking care with wraparound
					throw Indigo::Exception("vertex_data_compressed_size was invalid.");

				// Decompress data into plaintext buffer.
				timer.unpause();
				const size_t res = ZSTD_decompress(plaintext.begin(), header.vertex_data_size_B, file.currentReadPtr(), vertex_data_compressed_size);
				if(ZSTD_isError(res))
					throw Indigo::Exception("Decompression of index buffer failed: " + toString(res));
				if(res < (size_t)header.vertex_data_size_B)
					throw Indigo::Exception("Decompression of index buffer failed: not enough bytes in result");
				timer.pause();
				const double elapsed = timer.elapsed();
				// conPrint("Decompression took   " + doubleToStringNSigFigs(elapsed, 4) + " (" + doubleToStringNSigFigs(((double)((size_t)header.index_data_size_B + header.vertex_data_size_B) / (1024ull*1024ull)) / elapsed, 4) + "MB/s)");

				/*
				Read de-interleaved vertex data, and interleave it.
			
				p0 p1 p2 p3 ... pN n0 n1 n2 n3 ... nN c0 c1 c2 c3 ... cN
				=>
				p0 n0 c0 p1 n1 c1 p2 n2 c2 p3 n3 c3 ... pN nN cN
				*/
				const size_t vert_size = mesh_out.vertexSize(); // in bytes
				assert(vert_size % 4 == 0);
				const size_t num_verts = mesh_out.vertex_data.size() / vert_size;

				const uint8* src_ptr = plaintext.data();
				size_t attr_offset = 0;
				for(size_t b=0; b<mesh_out.vert_attributes.size(); ++b)
				{
					const size_t attr_size = vertAttributeSize(mesh_out.vert_attributes[b]);
					assert(attr_size % 4 == 0);
					uint8* dst_ptr = mesh_out.vertex_data.data() + attr_offset;

					for(size_t i=0; i<num_verts; ++i) // For each vertex
					{
						// Copy data for this attribute, for this vertex, to filtered_data
						assert(src_ptr + attr_size <= plaintext.data() + plaintext.size());
						assert(dst_ptr + attr_size <= mesh_out.vertex_data.data() + mesh_out.vertex_data.size());

						copyUInt32s(dst_ptr, src_ptr, attr_size);

						src_ptr += attr_size;
						dst_ptr += vert_size;
					}

					attr_offset += attr_size;
				}

				file.setReadIndex(file.getReadIndex() + vertex_data_compressed_size);
			}
		}
		else // else if !compression:
		{
			file.readData(mesh_out.index_data.data(),  mesh_out.index_data.dataSizeBytes());
			file.readData(mesh_out.vertex_data.data(), mesh_out.vertex_data.dataSizeBytes());
		}

		assert(file.getReadIndex() == file.fileSize());
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Bad allocation while reading from file.");
	}
}


const BatchedMesh::VertAttribute* BatchedMesh::findAttribute(VertAttributeType type, size_t& offset_out) const // returns NULL if not present.
{
	offset_out = 0;
	for(size_t b=0; b<vert_attributes.size(); ++b)
	{
		if(vert_attributes[b].type == type)
			return &vert_attributes[b];

		offset_out += vertAttributeSize(vert_attributes[b]);
	}
	return NULL;
}


size_t BatchedMesh::numMaterialsReferenced() const
{
	size_t max_i = 0;
	for(size_t b=0; b<batches.size(); ++b)
		max_i = myMax(max_i, (size_t)batches[b].material_index);
	return max_i + 1;
}


#if BUILD_TESTS


#include "FormatDecoderGLTF.h"
#include "../dll/IndigoStringUtils.h"
#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"


static void testWritingAndReadingMesh(const BatchedMesh& batched_mesh)
{
	try
	{
		const std::string temp_path = PlatformUtils::getTempDirPath() + "/temp678.bmesh";

		// Write without compressionm, read back from disk, and check unchanged in round trip.
		{
			BatchedMesh::WriteOptions write_options;
			write_options.use_compression = false;
			batched_mesh.writeToFile(temp_path, write_options);

			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);

			testAssert(batched_mesh == batched_mesh2);
		}

		// Write with compressionm, read back from disk, and check unchanged in round trip.
		{
			BatchedMesh::WriteOptions write_options;
			write_options.use_compression = true;
			batched_mesh.writeToFile(temp_path, write_options);

			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);

			testAssert(batched_mesh.index_data.size() == batched_mesh2.index_data.size());
			if(batched_mesh.index_data != batched_mesh2.index_data)
			{
				for(size_t i=0; i<batched_mesh.index_data.size(); ++i)
				{
					conPrint("");
					conPrint("batched_mesh .index_data[" + toString(i) + "]: " + toString(batched_mesh.index_data[i]));
					conPrint("batched_mesh2.index_data[" + toString(i) + "]: " + toString(batched_mesh2.index_data[i]));
				}
			}
			testAssert(batched_mesh.index_data == batched_mesh2.index_data);
			testAssert(batched_mesh.vertex_data == batched_mesh2.vertex_data);

			testAssert(batched_mesh == batched_mesh2);
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


static void perfTestWithMesh(const std::string& path)
{
	conPrint("");
	conPrint("===================================================");
	conPrint("Perf test with " + path);
	conPrint("===================================================");
	Indigo::Mesh indigo_mesh;
	if(hasExtension(path, "glb"))
	{
		GLTFMaterials mats;
		FormatDecoderGLTF::loadGLBFile(path, indigo_mesh, 1.f, mats);
	}
	else if(hasExtension(path, "gltf"))
	{
		GLTFMaterials mats;
		FormatDecoderGLTF::streamModel(path, indigo_mesh, 1.f, mats);
	}
	else if(hasExtension(path, "igmesh"))
	{
		Indigo::Mesh::readFromFile(toIndigoString(path), indigo_mesh);
	}
		

	BatchedMesh batched_mesh;
	batched_mesh.buildFromIndigoMesh(indigo_mesh);

	const std::string temp_path = PlatformUtils::getTempDirPath() + "/temp6789.bmesh";

	BatchedMesh::WriteOptions write_options;
	write_options.use_compression = true;

	const int compression_levels[] ={ 1, 3, 6 };// , 9, 20};

	for(int i=0; i<staticArrayNumElems(compression_levels); ++i)
	{
		write_options.compression_level = compression_levels[i];

		conPrint("\nWriting with compression level " + toString(write_options.compression_level));
		conPrint("----------------------------------------------");
		batched_mesh.writeToFile(temp_path, write_options);

		// Load from disk to get decompression speed.
		{
			Timer timer;
			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);
			conPrint("readFromFile() time: " + timer.elapsedStringNSigFigs(4));
		}
	}
}


void BatchedMesh::test()
{
	conPrint("BatchedMesh::test()");

	try
	{
		{
			Indigo::Mesh m;
			m.num_uv_mappings = 1;
			m.used_materials = Indigo::Vector<Indigo::String>(1, Indigo::String("mat1"));

			m.vert_positions = Indigo::Vector<Indigo::Vec3f>(8, Indigo::Vec3f(1.f, 2.f, 3.f));
			m.vert_normals = Indigo::Vector<Indigo::Vec3f>(8, Indigo::Vec3f(0.f, 1.f, 0.f));

			m.uv_pairs = Indigo::Vector<Indigo::Vec2f>(3, Indigo::Vec2f(5.f, 1.f));

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 0; m.triangles.back().vertex_indices[1] = 1; m.triangles.back().vertex_indices[2] = 2;
			m.triangles.back().uv_indices[0] = 0; m.triangles.back().uv_indices[1] = 0; m.triangles.back().uv_indices[2] = 0;
			m.triangles.back().tri_mat_index = 0;

			m.triangles.push_back(Indigo::Triangle());
			m.triangles.back().vertex_indices[0] = 5; m.triangles.back().vertex_indices[1] = 2; m.triangles.back().vertex_indices[2] = 7;
			m.triangles.back().uv_indices[0] = 0; m.triangles.back().uv_indices[1] = 0; m.triangles.back().uv_indices[2] = 0;
			m.triangles.back().tri_mat_index = 1;

			m.quads.push_back(Indigo::Quad());
			m.quads.back().vertex_indices[0] = 1; m.quads.back().vertex_indices[1] = 2; m.quads.back().vertex_indices[2] = 2, m.quads.back().vertex_indices[2] = 3;
			m.quads.back().uv_indices[0] = 0; m.quads.back().uv_indices[1] = 0; m.quads.back().uv_indices[2] = 0, m.quads.back().uv_indices[3] = 0;
			m.quads.back().mat_index = 2;

			m.quads.push_back(Indigo::Quad());
			m.quads.back().vertex_indices[0] = 5; m.quads.back().vertex_indices[1] = 6; m.quads.back().vertex_indices[2] = 7, m.quads.back().vertex_indices[2] = 4;
			m.quads.back().uv_indices[0] = 0; m.quads.back().uv_indices[1] = 0; m.quads.back().uv_indices[2] = 0, m.quads.back().uv_indices[3] = 0;
			m.quads.back().mat_index = 3;


			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(m);

			testWritingAndReadingMesh(batched_mesh);
		}


		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getIndigoTestReposDir()) + "/testscenes/mesh_2107654449_802486.igmesh", indigo_mesh);
			testAssert(indigo_mesh.triangles.size() == 7656);

			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(indigo_mesh);

			testWritingAndReadingMesh(batched_mesh);
		}

		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(TestUtils::getIndigoTestReposDir()) + "/testscenes/quad_mesh_500x500_verts.igmesh", indigo_mesh);
			testAssert(indigo_mesh.quads.size() == 249001);

			BatchedMesh batched_mesh;
			batched_mesh.buildFromIndigoMesh(indigo_mesh);

			testWritingAndReadingMesh(batched_mesh);
		}
		
		
		// Perf test - test compression and decompression speed at varying compression levels
		if(false)
		{
			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/testfiles/gltf/2CylinderEngine.glb");

			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/testfiles/gltf/duck/Duck.gltf");

			perfTestWithMesh(TestUtils::getIndigoTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh");
		}

		/*
		Some results, CPU = Intel(R) Core(TM) i7-8700K CPU:

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/2CylinderEngine.glb
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     830798 B
		Compression ratio:   3.3852561031682793
		Compression took     0.007963000040035695 s (420.87852534918136 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     748540 B
		Compression ratio:   3.757266144761803
		Compression took     0.01277719996869564 s (226.54076412086405 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     721103 B
		Compression ratio:   3.9002250718690674
		Compression took     0.030561399995349348 s (91.6447029163987 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     710084 B
		Compression ratio:   3.9607483058342394
		Compression took     0.04172229999676347 s (66.01593639085978 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     620024 B
		Compression ratio:   4.536056668774112
		Compression took     0.4796337999869138 s (5.6213213240073445 MB/s)

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/duck/Duck.gltf
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     64346 B
		Compression ratio:   1.2875392409784603
		Compression took     0.001195400021970272 s (248.85044571488913 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     63666 B
		Compression ratio:   1.3012911129959477
		Compression took     0.0013697000104002655 s (122.42022596370255 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61667 B
		Compression ratio:   1.3434738190604376
		Compression took     0.0025105999666266143 s (44.31296226204313 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61568 B
		Compression ratio:   1.3456340956340955
		Compression took     0.0031196000054478645 s (41.82415407470109 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     60575 B
		Compression ratio:   1.3676929426330995
		Compression took     0.012172200018540025 s (7.119429927248082 MB/s)

		===================================================
		Perf test with O:\indigo\trunk/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     47793085 B
		Compression ratio:   2.8499493598289374
		Compression took     0.21159790002275258 s (618.2025166677035 MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     46020837 B
		Compression ratio:   2.9597000158862823
		Compression took     0.4927970999851823 s (264.5570254046427 MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45541753 B
		Compression ratio:   2.9908350695240036
		Compression took     2.107264499994926 s (61.670013889674586 MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45542444 B
		Compression ratio:   2.9907896906015847
		Compression took     2.1111218000296503 s (61.59432004020065 MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     40178301 B
		Compression ratio:   3.390085409534863
		Compression took     12.154803499986883 s (10.687562677819832 MB/s)
		BatchedMesh::test() done.





		With separate compression of the index and vertex data:




		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/2CylinderEngine.glb
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     829078 B
		Compression ratio:   3.3922791341707295
		Compression took     0.006650500057730824 s (403.3042116132777 MB/s)
		Decompression took 0.003736 (718.0MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     748083 B
		Compression ratio:   3.7595614390381815
		Compression took     0.011418199981562793 s (234.90345999791134 MB/s)
		Decompression took 0.003739 (717.3MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     720478 B
		Compression ratio:   3.903608437731617
		Compression took     0.028009999950882047 s (95.75775392076446 MB/s)
		Decompression took 0.003587 (747.9MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     709443 B
		Compression ratio:   3.964326943813668
		Compression took     0.038837999978568405 s (69.06057686022106 MB/s)
		Decompression took 0.003411 (786.3MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   2812464 B
		Compressed size:     620023 B
		Compression ratio:   4.536063984723147
		Compression took     0.4449350000359118 s (6.028239366201136 MB/s)
		Decompression took 0.004185 (640.9MB/s)

		===================================================
		Perf test with O:\indigo\trunk/testfiles/gltf/duck/Duck.gltf
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61888 B
		Compression ratio:   1.3386763185108583
		Compression took     0.0003358999965712428 s (235.21884659759843 MB/s)
		Decompression took 0.0001917 (412.2MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     61590 B
		Compression ratio:   1.3451534339990259
		Compression took     0.0005754000158049166 s (137.31318664477152 MB/s)
		Decompression took 0.0001941 (407.1MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59877 B
		Compression ratio:   1.383636454732201
		Compression took     0.0016341999871656299 s (48.347821800354204 MB/s)
		Decompression took 0.0001986 (397.8MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59744 B
		Compression ratio:   1.3867166577396894
		Compression took     0.0016995000187307596 s (46.490149393838884 MB/s)
		Decompression took 0.0002542 (310.8MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   82848 B
		Compressed size:     59582 B
		Compression ratio:   1.3904870598502903
		Compression took     0.010535200068261474 s (7.499621198808736 MB/s)
		Decompression took 0.0002182 (362.1MB/s)

		===================================================
		Perf test with O:\indigo\trunk/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh
		===================================================

		Writing with compression level 1
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     47807468 B
		Compression ratio:   2.8490919452165926
		Compression took     0.20913650002330542 s (621.1156311991194 MB/s)
		Decompression took 0.1487 (873.5MB/s)

		Writing with compression level 3
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45860861 B
		Compression ratio:   2.9700243089635845
		Compression took     0.48414219997357577 s (268.3053640559318 MB/s)
		Decompression took 0.1516 (856.8MB/s)

		Writing with compression level 6
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45540911 B
		Compression ratio:   2.9908903666859015
		Compression took     1.7893327999627218 s (72.59574586765315 MB/s)
		Decompression took 0.1620 (801.8MB/s)

		Writing with compression level 9
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     45541879 B
		Compression ratio:   2.9908267948276794
		Compression took     2.1417220999719575 s (60.65116908512585 MB/s)
		Decompression took 0.1543 (841.7MB/s)

		Writing with compression level 20
		----------------------------------------------

		Uncompressed size:   136207872 B
		Compressed size:     40175387 B
		Compression ratio:   3.3903312991110703
		Compression took     12.293474600010086 s (10.566414577262268 MB/s)
		Decompression took 0.1857 (699.5MB/s)

		*/
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	catch(Indigo::IndigoException& e)
	{
		failTest(toStdString(e.what()));
	}

	conPrint("BatchedMesh::test() done.");
}


#endif // BUILD_TESTS
