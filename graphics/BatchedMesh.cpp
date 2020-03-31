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
					std::memcpy(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f)); // Copy vert position

					if(mesh_has_shading_normals)
					{
						const uint32 n = BMeshPackNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
						std::memcpy(&vert_data[cur_size + normal_offset], &n, 4);
					}

					if(mesh_has_uvs)
					{
						if(use_half_uvs)
						{
							half half_uv[2];
							half_uv[0] = half(uv.x);
							half_uv[1] = half(uv.y);
							std::memcpy(&vert_data[cur_size + uv_offset], half_uv, 4);
						}
						else
							std::memcpy(&vert_data[cur_size + uv_offset], &uv.x, sizeof(Indigo::Vec2f));
					}

					if(mesh_has_vert_cols)
						std::memcpy(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
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
					std::memcpy(&vert_data[cur_size], &vert_positions[pos_i].x, sizeof(Indigo::Vec3f));
					if(mesh_has_shading_normals)
					{
						const uint32 n = BMeshPackNormal(vert_normals[pos_i]); // Pack normal into GL_INT_2_10_10_10_REV format.
						std::memcpy(&vert_data[cur_size + normal_offset], &n, 4);
					}

					if(mesh_has_uvs)
					{
						if(use_half_uvs)
						{
							half half_uv[2];
							half_uv[0] = half(uv.x);
							half_uv[1] = half(uv.y);
							std::memcpy(&vert_data[cur_size + uv_offset], half_uv, 4);
						}
						else
							std::memcpy(&vert_data[cur_size + uv_offset], &uv_pairs[uv_i].x, sizeof(Indigo::Vec2f));
					}

					if(mesh_has_vert_cols)
						std::memcpy(&vert_data[cur_size + vert_col_offset], &vert_colours[pos_i].x, sizeof(Indigo::Vec3f));
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


//static const uint32 MAX_VECTOR_LENGTH = std::numeric_limits<uint32>::max() / 2;
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


void BatchedMesh::writeToFile(const std::string& dest_path, bool use_compression) const // throws Indigo::Exception on failure
{
	FileOutStream file(dest_path);

	//Timer write_timer;

	BatchedMeshHeader header;
	header.magic_number = MAGIC_NUMBER;
	header.format_version = FORMAT_VERSION;
	header.header_size = sizeof(BatchedMeshHeader);
	header.flags = use_compression ? FLAG_USE_COMPRESSION : 0;

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
	if(use_compression)
	{
		// Copy/format the mesh data into a single buffer

		js::Vector<uint8, 16> filtered_data(index_data.size() + vertex_data.size());

		// Build filtered index data.
		uint32 last_index = 0;
		const size_t num_indices = index_data.size() / componentTypeSize(index_type);
		if(index_type == ComponentType_UInt8)
		{
			const uint8* const index_data_uint8    = (const uint8*)index_data.data();
			      uint8* const filtered_index_data = (uint8*)filtered_data.data();
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
			      uint16* const filtered_index_data = (uint16*)filtered_data.data();
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
			      uint32* const filtered_index_data = (uint32*)filtered_data.data();
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

		// Separate vertex data into separate arrays
		size_t write_i = index_data.size();
		const size_t vert_size = vertexSize(); // in bytes
		const size_t num_verts = vertex_data.size() / vert_size;

		size_t attr_offset = 0;
		for(size_t b=0; b<vert_attributes.size(); ++b)
		{
			const VertAttribute& attr = vert_attributes[b];

			const size_t attr_size = vertAttributeSize(attr);

			for(size_t i=0; i<num_verts; ++i) // For each vertex
			{
				// Copy data for this attribute, for this vertex, to filtered_data
				assert(write_i + attr_size <= filtered_data.size());
				std::memcpy(filtered_data.data() + write_i, vertex_data.data() + i * vert_size + attr_offset, attr_size);

				write_i += attr_size;
			}

			attr_offset += attr_size;
		}

		// Compress the buffer with zstandard
		const size_t compressed_bound = ZSTD_compressBound(filtered_data.size());
		js::Vector<uint8, 16> compressed_data(compressed_bound);
		Timer timer;
		const size_t compressed_size = ZSTD_compress(compressed_data.data(), compressed_data.size(), filtered_data.data(), filtered_data.size(),
			1 // compression level
		);

		const double compression_speed = filtered_data.size() / timer.elapsed();
		conPrint("");
		conPrint("Uncompressed size:   " + toString(filtered_data.size()) + " B");
		conPrint("Compressed size:     " + toString(compressed_size) + " B");
		conPrint("Compression ratio:   " + toString((double)filtered_data.size() / compressed_size));
		conPrint("Compression took     " + timer.elapsedString() + " (" + toString(compression_speed / (1024*1024)) + " MB/s)");

		// Write compressed size as uint64
		const uint64 compressed_size_64 = compressed_size;
		file.writeUInt64(compressed_size_64);

		// Now write compressed data to disk
		file.writeData(compressed_data.data(), compressed_size);
	}
	else
	{
		file.writeData(index_data.data(),  index_data.dataSizeBytes());
		file.writeData(vertex_data.data(), vertex_data.dataSizeBytes());
	}

	//conPrint("Total time to write to disk: " + write_timer.elapsedString());
}


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

		const bool compression = (header.flags & FLAG_USE_COMPRESSION) != 0;

		// Skip past rest of header
		if(header.header_size > 10000 || header.header_size > file.fileSize())
			throw Indigo::Exception("Header size too large.");
		file.setReadIndex(header.header_size);



		// Read vert attributes
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
		mesh_out.batches.resize(header.num_batches);
		file.readData(mesh_out.batches.data(), mesh_out.batches.size() * sizeof(IndicesBatch));


		// Check header index type
		if(header.index_type > MAX_COMPONENT_TYPE_VALUE)
			throw Indigo::Exception("Invalid index type value.");

		mesh_out.index_type = (ComponentType)header.index_type;
		mesh_out.index_data.resize(header.index_data_size_B); // TODO: sanity check
		mesh_out.vertex_data.resize(header.vertex_data_size_B); // TODO: sanity check

		mesh_out.aabb_os.min_ = Vec4f(header.aabb_min.x, header.aabb_min.y, header.aabb_min.z, 1.f);
		mesh_out.aabb_os.max_ = Vec4f(header.aabb_max.x, header.aabb_max.y, header.aabb_max.z, 1.f);

		if(compression)
		{
			// Read compressed size
			const uint64 compressed_size = file.readUInt64();

			const uint64 decompressed_size = ZSTD_getDecompressedSize((const uint8*)file.fileData() + file.getReadIndex(), compressed_size);

			if(decompressed_size != (uint64)header.index_data_size_B + (uint64)header.vertex_data_size_B)
				throw Indigo::Exception("decompressed size was invalid.");

			// Decompress data into buffer.
			js::Vector<uint8, 16> plaintext(decompressed_size);
			//Timer timer;
			ZSTD_decompress(plaintext.begin(), decompressed_size, (const uint8*)file.fileData() + file.getReadIndex(), compressed_size);


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
				const int16* filtered_index_data_int16 = (const int16*)plaintext.data();
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
				const int32* filtered_index_data_int32 = (const int32*)plaintext.data();
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

			
			/*
			Read de-interleaved vertex data, and interleave it.
			Separate vertex data into separate arrays:
			
			p0 p1 p2 p3 ... pN n0 n1 n2 n3 ... nN c0 c1 c2 c3 ... cN
			
			=>

			p0 n0 c0 p1 n1 c1 p2 n2 c2 p3 n3 c3 ... pN nN cN

			*/
			
			const size_t vert_size = mesh_out.vertexSize(); // in bytes
			const size_t num_verts = mesh_out.vertex_data.size() / vert_size;

			size_t read_i = mesh_out.index_data.size(); // vert data comes after index data in plaintext buffer.
			size_t attr_offset = 0;
			for(size_t b=0; b<mesh_out.vert_attributes.size(); ++b)
			{
				const VertAttribute& attr = mesh_out.vert_attributes[b];
				const size_t attr_size = vertAttributeSize(attr);
				size_t write_i = attr_offset;

				for(size_t i=0; i<num_verts; ++i) // For each vertex
				{
					// Copy data for this attribute, for this vertex, to filtered_data
					assert(write_i + attr_size <= mesh_out.vertex_data.size());
					std::memcpy(mesh_out.vertex_data.data() + write_i, plaintext.data() + read_i, attr_size);

					read_i += attr_size;
					write_i += vert_size;
				}

				attr_offset += attr_size;
			}
		}
		else // else if !compression:
		{
			file.readData(mesh_out.index_data.data(), mesh_out.index_data.dataSizeBytes());
			file.readData(mesh_out.vertex_data.data(), mesh_out.vertex_data.dataSizeBytes());
		}
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
			batched_mesh.writeToFile(temp_path, /*use_compression=*/false);

			BatchedMesh batched_mesh2;
			BatchedMesh::readFromFile(temp_path, batched_mesh2);

			testAssert(batched_mesh == batched_mesh2);
		}

		// Write with compressionm, read back from disk, and check unchanged in round trip.
		{
			batched_mesh.writeToFile(temp_path, /*use_compression=*/true);

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
