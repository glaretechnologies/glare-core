/*=====================================================================
BatchedMesh.h
-------------
Copyright Glare Technologies Limited 2020
=====================================================================*/
#pragma once


#include "AnimationData.h"
#include <physics/jscol_aabbox.h>
#include <utils/ThreadSafeRefCounted.h>
#include <utils/Platform.h>
#include <utils/Reference.h>
#include <utils/Vector.h>
#include <utils/AllocatorVector.h>
#include <vector>
#include <string>


namespace Indigo { class Mesh; }
class OutStream;


/*=====================================================================
BatchedMesh
-----------
Triangle mesh optimised for OpenGL rendering.
Triangle vertex indices are sorted by material index, and has information 
about the batches of indices corresponding to triangles sharing a common material index.

Tests are in BatchedMeshTests.
=====================================================================*/
class BatchedMesh : public ThreadSafeRefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	BatchedMesh();
	~BatchedMesh();

	/// Write a BatchedMesh object to disk.
	/// @param dest_path		Path on disk.
	/// @param use_compression	True if compression should be used.
	/// @throws glare::Exception on failure.
	struct WriteOptions
	{
		WriteOptions() : write_mesh_version_2(false), use_compression(true), use_meshopt(false), compression_level(3), pos_mantissa_bits(16), uv_mantissa_bits(10), meshopt_vertex_version(1)  {}
		
		bool write_mesh_version_2; // Write an older batched mesh version for backwards compatibility.  Default is false.
		bool use_compression;
		bool use_meshopt;
		int compression_level; // Zstandard compression level.  Zstandard default compression level is 3.
		int pos_mantissa_bits; // For meshopt filtering.  Should be >= 1 and <= 24.  Only used in the write_mesh_version_2 case.
		int uv_mantissa_bits;  // For meshopt filtering.  Should be >= 1 and <= 24.  Only used in the write_mesh_version_2 case.
		int meshopt_vertex_version; // Can be 0 or 1.  Default is 1.
	};
	void writeToFile(const std::string& dest_path, const WriteOptions& write_options = WriteOptions()) const;

	void writeToOutStream(OutStream& out_stream, const WriteOptions& write_options) const;

	/// Read a BatchedMesh object from disk.
	/// Memory allocator param can be null.
	/// @param src_path			Path on disk to read from.
	/// @param mem_allocator	Memory allocator.  Can be null.
	/// @param mesh_out			Mesh object to read to.
	/// @throws glare::Exception on failure.
	static Reference<BatchedMesh> readFromFile(const std::string& src_path, glare::Allocator* mem_allocator);

	static Reference<BatchedMesh> readFromData(const void* data, size_t data_len, glare::Allocator* mem_allocator);

	// Check vertex, joint indices are in bounds etc.
	// Throws glare::Exception on invalid mesh.
	// May also change vertex joint weights so they sum to 1.
	void checkValidAndSanitiseMesh();

	// Reorders vertex index batches so that batches sharing the same material are grouped together, then merges them.
	void optimise();

	void doMeshOptimizerOptimisations();

	struct QuantiseOptions
	{
		QuantiseOptions() : pos_bits(14), uv_bits(10) {}
		
		int pos_bits; // Number of bits to use for the position coordinates.  must be <= 16.
		int uv_bits;  // Must be <= 16
	};
	[[nodiscard]] Reference<BatchedMesh> buildQuantisedMesh(const QuantiseOptions& quantise_options) const;

	// Builds a BatchedMesh from an Indigo::Mesh.
	// Any quads are converted to triangles.
	// Merges any vertices with the same position and UVs.
	[[nodiscard]] static Reference<BatchedMesh> buildFromIndigoMesh(const Indigo::Mesh& mesh);
	
	// Build an Indigo::Mesh from this BatchedMesh.
	Reference<Indigo::Mesh> buildIndigoMesh() const;
	void buildIndigoMesh(Indigo::Mesh& mesh_out) const;


	bool operator == (const BatchedMesh& other) const; // Just used in tests
private:
	void operator = (const BatchedMesh& other); // Just used in tests
public:

	enum ComponentType
	{
		ComponentType_Float			= 0,
		ComponentType_Half			= 1,
		ComponentType_UInt8			= 2,
		ComponentType_UInt16		= 3,
		ComponentType_UInt32		= 4,
		ComponentType_PackedNormal	= 5, // GL_INT_2_10_10_10_REV
		ComponentType_Oct16			= 6  // Octahedral encoding in 16 bits (oct16).  Stored as snorm8 * 2. (See 'A Survey of Efficient Representations for Independent Unit Vectors')
	};
	static const uint32 MAX_COMPONENT_TYPE_VALUE = 6;

	enum VertAttributeType
	{
		VertAttribute_Position	= 0,
		VertAttribute_Normal	= 1,
		VertAttribute_Colour	= 2,
		VertAttribute_UV_0		= 3,
		VertAttribute_UV_1		= 4,
		VertAttribute_Joints	= 5, // Indices of joint nodes for skinning
		VertAttribute_Weights	= 6, // weights for skinning
		VertAttribute_Tangent	= 7,
		VertAttribute_MatIndex	= 8, // Index of original material for combined meshes.
	};
	static const uint32 MAX_VERT_ATTRIBUTE_TYPE_VALUE = 8;

	struct VertAttribute
	{
		VertAttribute() {}
		VertAttribute(VertAttributeType type_, ComponentType component_type_, size_t offset_B_) : type(type_), component_type(component_type_), offset_B(offset_B_) {}

		VertAttributeType type;
		ComponentType component_type;
		size_t offset_B; // Offset of attribute in vertex data, in bytes.  Not serialised.

		bool operator == (const VertAttribute& other) const { return type == other.type && component_type == other.component_type && offset_B == other.offset_B; }
	};

	struct IndicesBatch
	{
		uint32 indices_start;
		uint32 num_indices;
		uint32 material_index;

		bool operator == (const IndicesBatch& other) const { return indices_start == other.indices_start && num_indices == other.num_indices && material_index == other.material_index; }
	};

	inline static size_t componentTypeSize(ComponentType t); // In bytes.
	static std::string componentTypeString(ComponentType t);
	inline static size_t vertAttributeTypeNumComponents(VertAttributeType t);
	inline static size_t vertAttributeSize(const VertAttribute& attr); // In bytes.
	size_t vertexSize() const; // In bytes.  Guaranteed to be a multiple of 4.
	inline size_t numVerts() const;
	inline size_t numIndices() const; // Equal to num triangles * 3.
	inline uint32 getIndexAsUInt32(size_t i) const; // Slow, for debugging

	js::AABBox computeAABB() const;

	// Find the attribute identified by 'attr_type'.  Returns NULL if not present.
	const VertAttribute* findAttribute(VertAttributeType attr_type) const;

	// Find the attribute identified by 'attr_type'.  Throws glare::Exception if attribute not present.
	const VertAttribute& getAttribute(VertAttributeType attr_type) const;

	size_t numMaterialsReferenced() const;

	size_t getTotalMemUsage() const;

	// Sets index_data and index_type.
	// The index type will depend on num_verts. (will be ComponentType_UInt16 for num_verts < 32768) 
	void setIndexDataFromIndices(const js::Vector<uint32, 16>& uint32_indices, size_t num_verts);

	void toUInt32Indices(js::Vector<uint32, 16>& uint32_indices_out) const;

	Vec4f getVertexPosition(uint32 vert_index) const; // for debugging

	std::vector<VertAttribute> vert_attributes;

	std::vector<IndicesBatch> batches;

	ComponentType index_type; // Must be one of ComponentType_UInt8, ComponentType_UInt16, ComponentType_UInt32.
	// If ComponentType_UInt8, index values must be < 128.  This is so the difference between any two indices is in the range [-127, 127], which means the difference can be stored in a signed 8-bit value.
	// Likewise for ComponentType_UInt16, index values must be < 32768.
	glare::AllocatorVector<uint8, 16> index_data;

	glare::AllocatorVector<uint8, 16> vertex_data;

	js::AABBox aabb_os; // An AABB that contains the vertex positions.

	float uv0_scale; // dequantisation uv0 scale
	float uv1_scale; // dequantisation uv1 scale

	AnimationData animation_data;
};


typedef Reference<BatchedMesh> BatchedMeshRef;


size_t BatchedMesh::componentTypeSize(ComponentType t)
{
	switch(t)
	{
	case ComponentType_Float:			return 4;
	case ComponentType_Half:			return 2;
	case ComponentType_UInt8:			return 1;
	case ComponentType_UInt16:			return 2;
	case ComponentType_UInt32:			return 4;
	case ComponentType_PackedNormal:	return 4; // GL_INT_2_10_10_10_REV
	case ComponentType_Oct16:			return 2;
	};
	assert(0);
	return 1;
}


size_t BatchedMesh::vertAttributeTypeNumComponents(VertAttributeType t)
{
	switch(t)
	{
	case VertAttribute_Position:  return 3;
	case VertAttribute_Normal:    return 3;
	case VertAttribute_Colour:    return 3;
	case VertAttribute_UV_0:      return 2;
	case VertAttribute_UV_1:      return 2;
	case VertAttribute_Joints:    return 4; // 4 joints per vert, following GLTF
	case VertAttribute_Weights:   return 4; // 4 weights per vert, following GLTF
	case VertAttribute_Tangent:   return 4; // x,y,z,w
	case VertAttribute_MatIndex:  return 1;
	};
	assert(0);
	return 1;
}


size_t BatchedMesh::vertAttributeSize(const VertAttribute& attr)
{
	if(attr.component_type == ComponentType_PackedNormal) // Special case, has 3 components packed into 4 bytes.
		return 4;
	else if(attr.component_type == ComponentType_Oct16) // Special case, packed into 2 bytes.
		return 2;
	else
		return vertAttributeTypeNumComponents(attr.type) * componentTypeSize(attr.component_type);
}


size_t BatchedMesh::numVerts() const // in bytes
{
	const size_t vert_size = vertexSize();
	if(vert_size == 0)
		return 0;
	else
	{
		assert(vertex_data.size() % vert_size == 0);
		return vertex_data.size() / vert_size;
	}
}


size_t BatchedMesh::numIndices() const // in bytes
{
	const size_t index_size = componentTypeSize(this->index_type);
	assert(index_data.size() % index_size == 0);
	return index_data.size() / index_size;
}


uint32 BatchedMesh::getIndexAsUInt32(size_t i) const
{
	switch(index_type)
	{
	case ComponentType_UInt8:
		return (uint32)index_data[i];
	case ComponentType_UInt16:
		return (uint32)(((uint16*)index_data.data())[i]);
	//case ComponentType_UInt32:
	default:
		return ((uint32*)index_data.data())[i];
	}
}


// See Section 10.3.8 'Packed Vertex Data Formats' from https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf
inline static uint32 batchedMeshPackNormal(const Vec4f& normal)
{
	const Vec4f scaled_n = normal * 511.f; // Map from [-1, 1] to [-511, 511]
	int x = (int)scaled_n[0]; 
	int y = (int)scaled_n[1];
	int z = (int)scaled_n[2];
	// ANDing with 1023 isolates the bottom 10 bits.
	return (x & 1023) | ((y & 1023) << 10) | ((z & 1023) << 20);
}


inline static uint32 batchedMeshPackNormalWithW(const Vec4f& normal)
{
	const Vec4f scaled_n = normal * 511.f; // Map from [-1, 1] to [-511, 511]
	int x = (int)scaled_n[0]; 
	int y = (int)scaled_n[1];
	int z = (int)scaled_n[2];
	int w = (int)normal[3];
	// ANDing with 1023 isolates the bottom 10 bits.
	return (x & 1023) | ((y & 1023) << 10) | ((z & 1023) << 20) | (w << 30);
}


// Treat the rightmost 10 bits of x as a signed number, sign extend.
inline int convertToSigned(uint32 x)
{
	// We will sign-extend using the right shift operator, which has implementation-defined behaviour, but should sign-extend when using two's complement numbers.
	return ((int)x << 22) >> 22;
}


inline static const Vec4f batchedMeshUnpackNormal(const uint32 packed_normal)
{
	const uint32 x_bits = (packed_normal >> 0 ) & 1023;
	const uint32 y_bits = (packed_normal >> 10) & 1023;
	const uint32 z_bits = (packed_normal >> 20) & 1023;

	const int x = convertToSigned(x_bits);
	const int y = convertToSigned(y_bits);
	const int z = convertToSigned(z_bits);

	return Vec4f((float)x, (float)y, (float)z, 0) * (1.f / 511.f);
}


inline static const Vec4f batchedMeshUnpackNormalWithW(const uint32 packed_normal)
{
	const uint32 x_bits = (packed_normal >> 0 ) & 1023;
	const uint32 y_bits = (packed_normal >> 10) & 1023;
	const uint32 z_bits = (packed_normal >> 20) & 1023;
	const uint32 w_bits = (packed_normal >> 30) & 3;

	const int x = convertToSigned(x_bits);
	const int y = convertToSigned(y_bits);
	const int z = convertToSigned(z_bits);
	const int w = (w_bits == 3) ? -1 : (int)w_bits; // If sign bit was set, the value is -1, otherwise it is just the value of bit 30, e.g. 0 or 1.

	Vec4f v = Vec4f((float)x, (float)y, (float)z, 0) * (1.f / 511.f);
	v[3] = (float)w;
	return v;
}
