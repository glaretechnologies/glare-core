/*=====================================================================
BatchedMesh.h
-------------
Copyright Glare Technologies Limited 2020
=====================================================================*/
#pragma once


#include <physics/jscol_aabbox.h>
#include <ThreadSafeRefCounted.h>
#include <Platform.h>
#include <Reference.h>
#include <Vector.h>
#include <vector>
#include <string>


namespace Indigo { class Mesh; }


/*=====================================================================
BatchedMesh
-----------
Triangle mesh optimised for OpenGL rendering.
Triangle vertex indices are sorted by material index, and has infomation 
about the batches of indices corresponding to triangles sharing a common material index.
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
	/// @throws Indigo::Exception on failure.
	struct WriteOptions
	{
		WriteOptions() : use_compression(true), compression_level(3) {}
		bool use_compression;
		int compression_level; // Zstandard compression level.  Zstandard defualt compression level is 3.
	};
	void writeToFile(const std::string& dest_path, const WriteOptions& write_options = WriteOptions()) const;

	/// Read a BatchedMesh object from disk.
	/// @param src_path			Path on disk to read from.
	/// @param mesh_out			Mesh object to read to.
	/// @throws Indigo::Exception on failure.
	static void readFromFile(const std::string& src_path, BatchedMesh& mesh_out);

	
	void buildFromIndigoMesh(const Indigo::Mesh& mesh);
	void buildIndigoMesh(Indigo::Mesh& mesh_out) const;

	bool operator == (const BatchedMesh& other) const;
	
	static void test();


	enum ComponentType
	{
		ComponentType_Float			= 0,
		ComponentType_Half			= 1,
		ComponentType_UInt8			= 2,
		ComponentType_UInt16		= 3,
		ComponentType_UInt32		= 4,
		ComponentType_PackedNormal	= 5 // GL_INT_2_10_10_10_REV
	};
	static const uint32 MAX_COMPONENT_TYPE_VALUE = 5;

	enum VertAttributeType
	{
		VertAttribute_Position	= 0,
		VertAttribute_Normal	= 1,
		VertAttribute_Colour	= 2,
		VertAttribute_UV_0		= 3,
		VertAttribute_UV_1		= 4
	};
	static const uint32 MAX_VERT_ATTRIBUTE_TYPE_VALUE = 4;

	struct VertAttribute
	{
		VertAttributeType type;
		ComponentType component_type;
		size_t offset_B; // Offset of attribute in vertex data, in bytes.

		bool operator == (const VertAttribute& other) const { return type == other.type && component_type == other.component_type && offset_B == other.offset_B; }
	};

	struct IndicesBatch
	{
		uint32 indices_start;
		uint32 num_indices;
		uint32 material_index;

		bool operator == (const IndicesBatch& other) const { return indices_start == other.indices_start && num_indices == other.num_indices && material_index == other.material_index; }
	};

	inline static size_t componentTypeSize(ComponentType t);
	inline static size_t vertAttributeTypeNumComponents(VertAttributeType t);
	inline static size_t vertAttributeSize(const VertAttribute& attr); // In bytes.  Guaranteed to be a multiple of 4.
	inline size_t vertexSize() const; // In bytes.  Guaranteed to be a multiple of 4.
	inline size_t numVerts() const;
	inline size_t numIndices() const; // Equal to num triangles * 3.

	// Find the attribute identified by 'type'.  Returns NULL if not present.
	const VertAttribute* findAttribute(VertAttributeType type) const;

	size_t numMaterialsReferenced() const;



	std::vector<VertAttribute> vert_attributes;

	std::vector<IndicesBatch> batches;

	ComponentType index_type; // Must be one of ComponentType_UInt8, ComponentType_UInt16, ComponentType_UInt32.
	// If ComponentType_UInt8, index values must be < 128.  This is so the difference between any two indices is in the range [-127, 127], which means the difference can be stored in a signed 8-bit value.
	// Likewise for ComponentType_UInt16, index values must be < 32768.
	js::Vector<uint8, 16> index_data;

	js::Vector<uint8, 16> vertex_data;

	js::AABBox aabb_os; // An AABB that contains the vertex positions.
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
	};
	assert(0);
	return 1;
}


size_t BatchedMesh::vertAttributeTypeNumComponents(VertAttributeType t)
{
	switch(t)
	{
	case VertAttribute_Position: return 3;
	case VertAttribute_Normal:   return 3;
	case VertAttribute_Colour:   return 3;
	case VertAttribute_UV_0:     return 2;
	case VertAttribute_UV_1:     return 2;
	};
	assert(0);
	return 1;
}


size_t BatchedMesh::vertAttributeSize(const VertAttribute& attr)
{
	if(attr.component_type == ComponentType_PackedNormal) // Special case, has 3 components packed into 4 bytes.
		return 4;
	else
		return vertAttributeTypeNumComponents(attr.type) * componentTypeSize(attr.component_type);
}


size_t BatchedMesh::vertexSize() const // in bytes
{
	size_t sum = 0;
	for(size_t i=0; i<vert_attributes.size(); ++i)
		sum += vertAttributeSize(vert_attributes[i]);
	return sum;
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
