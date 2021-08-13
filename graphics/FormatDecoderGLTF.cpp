/*=====================================================================
FormatDecoderGLTF.cpp
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "FormatDecoderGLTF.h"


#include "../dll/include/IndigoMesh.h"
#include "../utils/Exception.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Parser.h"
#include "../utils/JSONParser.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/IncludeXXHash.h"
#include "../maths/Quat.h"
#include "../graphics/Colour4f.h"
#include "../graphics/BatchedMesh.h"
#include <assert.h>
#include <vector>
#include <map>
#include <fstream>


// Kinda like assert(), but checks when NDEBUG enabled, and throws glare::Exception on failure.
static void _doRuntimeCheck(bool b, const char* message)
{
	assert(b);
	if(!b)
		throw glare::Exception(std::string(message));
}

#define doRuntimeCheck(v) _doRuntimeCheck((v), (#v))


// Throws an exception if b is false.
static void checkProperty(bool b, const char* on_false_message)
{
	if(!b)
		throw glare::Exception(std::string(on_false_message));
}


struct GLTFBuffer : public RefCounted
{
	GLTFBuffer() : file(NULL), binary_data(NULL) {}
	~GLTFBuffer() { delete file; }
	
	MemMappedFile* file;
	const uint8* binary_data;
	size_t data_size;
	std::string uri;
};
typedef Reference<GLTFBuffer> GLTFBufferRef;


// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-bufferview
struct GLTFBufferView : public RefCounted
{
	size_t buffer;
	size_t byte_length;
	size_t byte_offset;
	size_t byte_stride; // The stride, in bytes, between vertex attributes. When this is not defined, data is tightly packed.
	std::string name;
	size_t target;
};
typedef Reference<GLTFBufferView> GLTFBufferViewRef;


struct GLTFImage : public RefCounted
{
	std::string uri;
	size_t buffer_view;
	std::string mime_type;
};
typedef Reference<GLTFImage> GLTFImageRef;


struct GLTFTexture : public RefCounted
{
	size_t sampler;
	size_t source;
};
typedef Reference<GLTFTexture> GLTFTextureRef;


struct GLTFPrimitive : public RefCounted
{
	std::map<std::string, size_t> attributes;
	size_t indices;
	size_t material;
	size_t mode;
};
typedef Reference<GLTFPrimitive> GLTFPrimitiveRef;


struct GLTFMesh: public RefCounted
{
	std::vector<GLTFPrimitiveRef> primitives;
};
typedef Reference<GLTFMesh> GLTFMeshRef;


struct GLTFAccessor : public RefCounted
{
	size_t buffer_view;
	size_t byte_offset;
	size_t component_type;
	size_t count;
	// min
	// max
	std::string type;
};
typedef Reference<GLTFAccessor> GLTFAccessorRef;


struct GLTFNode : public RefCounted
{
	GLARE_ALIGNED_16_NEW_DELETE

	GLTFNode() : mesh(std::numeric_limits<size_t>::max()) {}

	std::vector<size_t> children;
	std::string name;
	
	Matrix4f matrix;
	Quatf rotation;
	size_t mesh;
	Vec3f scale;
	Vec3f translation;

	Matrix4f node_transform; // Computed matrix
};
typedef Reference<GLTFNode> GLTFNodeRef;


struct GLTFScene : public RefCounted
{
	std::string name;
	std::vector<size_t> nodes;
};
typedef Reference<GLTFScene> GLTFSceneRef;


struct GLTFTextureObject
{
	GLTFTextureObject() : index(std::numeric_limits<size_t>::max()) {}

	bool valid() const { return index != std::numeric_limits<size_t>::max(); }

	size_t index;
	size_t texCoord;
};


struct GLTFMaterial : public RefCounted
{
	GLARE_ALIGNED_16_NEW_DELETE

	GLTFMaterial() : KHR_materials_pbrSpecularGlossiness_present(false), pbrMetallicRoughness_present(false) {}

	std::string name;
	
	//----------- From KHR_materials_pbrSpecularGlossiness extension:------------
	bool KHR_materials_pbrSpecularGlossiness_present;
	Colour4f diffuseFactor;
	GLTFTextureObject diffuseTexture;
	Colour3f specularFactor;
	float glossinessFactor;
	GLTFTextureObject specularGlossinessTexture;
	//----------- End from KHR_materials_pbrSpecularGlossiness extension:------------

	//----------- From pbrMetallicRoughness:------------
	bool pbrMetallicRoughness_present;
	Colour4f baseColorFactor;
	GLTFTextureObject baseColorTexture;
	float metallicFactor;
	float roughnessFactor;
	//----------- End from pbrMetallicRoughness:------------
};
typedef Reference<GLTFMaterial> GLTFMaterialRef;


// Attributes present on any of the meshes.
struct AttributesPresent
{
	AttributesPresent() : normal_present(false), vert_col_present(false), texcoord_0_present(false), joints_present(false), weights_present(false) {}

	bool normal_present;
	bool vert_col_present;
	bool texcoord_0_present;
	bool joints_present;
	bool weights_present;
};


struct GLTFChannel
{
	int sampler; // sampler index
	int target_node;

	enum Path
	{
		Path_translation,
		Path_rotation,
		Path_scale,
		Path_weights
	};

	Path target_path;
};


struct GLTFSampler
{
	int input; // Accessor index - "a set of floating point scalar values representing linear time in seconds"
	enum Interpolation
	{
		Interpolation_linear,
		Interpolation_step,
		Interpolation_cubic_spline,
	};
	Interpolation interpolation; // interpolation method
	int output; // Accessor index?
};


struct GLTFAnimation : public RefCounted
{
	GLTFAnimation() {}

	std::string name;
	std::vector<GLTFChannel> channels;
	std::vector<GLTFSampler> samplers;
};
typedef Reference<GLTFAnimation> GLTFAnimationRef;


struct GLTFSkin : public RefCounted
{
	GLTFSkin() {}

	std::string name;
	int inverse_bind_matrices; // accessor index
	std::vector<int> joints; // Array of node indices
	int skeleton; // "The skeleton property (if present) points to the node that is the common root of a joints hierarchy or to a direct or indirect parent node of the common root."
};
typedef Reference<GLTFSkin> GLTFSkinRef;


struct GLTFData
{
	GLTFData() : scene(0) {}

	std::vector<GLTFBufferRef> buffers;
	std::vector<GLTFBufferViewRef> buffer_views;
	std::vector<GLTFAccessorRef> accessors;
	std::vector<GLTFImageRef> images;
	std::vector<GLTFTextureRef> textures;
	std::vector<GLTFMaterialRef> materials;
	std::vector<GLTFMeshRef> meshes;
	std::vector<GLTFNodeRef> nodes;
	std::vector<GLTFAnimationRef> animations;
	std::vector<GLTFSkinRef> skins;
	std::vector<GLTFSceneRef> scenes;
	size_t scene;

	AttributesPresent attr_present; // Attributes present on any of the meshes.
};


static void checkNodeType(const JSONNode& node, JSONNode::Type type)
{
	if(node.type != type)
		throw glare::Exception("Expected type " + JSONNode::typeString(type) + ", got type " + JSONNode::typeString(node.type) + ".");
}


static GLTFAccessor& getAccessor(GLTFData& data, size_t accessor_index)
{
	if(accessor_index >= data.accessors.size())
		throw glare::Exception("accessor_index out of bounds.");
	return *data.accessors[accessor_index];
}


static GLTFAccessor& getAccessorForAttribute(GLTFData& data, const GLTFPrimitive& primitive, const std::string& attr_name)
{
	auto res = primitive.attributes.find(attr_name);
	if(res == primitive.attributes.end())
		throw glare::Exception("Expected " + attr_name + " attribute.");

	const size_t attribute_val = res->second;
	if(attribute_val >= data.accessors.size())
		throw glare::Exception("attribute " + attr_name + " out of bounds.");

	return *data.accessors[attribute_val];
}


static GLTFBufferView& getBufferView(GLTFData& data, size_t buffer_view_index)
{
	if(buffer_view_index >= data.buffer_views.size())
		throw glare::Exception("buffer_view_index out of bounds.");
	return *data.buffer_views[buffer_view_index];
}


static GLTFBuffer& getBuffer(GLTFData& data, size_t buffer_index)
{
	if(buffer_index >= data.buffers.size())
		throw glare::Exception("buffer_index out of bounds.");
	return *data.buffers[buffer_index];
}


static GLTFTexture& getTexture(GLTFData& data, size_t texture_index)
{
	if(texture_index >= data.textures.size())
		throw glare::Exception("texture_index out of bounds.");
	return *data.textures[texture_index];
}


static GLTFImage& getImage(GLTFData& data, size_t image_index)
{
	if(image_index >= data.textures.size())
		throw glare::Exception("image_index out of bounds.");
	return *data.images[image_index];
}


static GLTFMesh& getMesh(GLTFData& data, size_t mesh_index)
{
	if(mesh_index >= data.meshes.size())
		throw glare::Exception("mesh_index out of bounds.");
	return *data.meshes[mesh_index];
}


static Colour3f parseColour3ChildArrayWithDefault(const JSONParser& parser, const JSONNode& node, const std::string& name, const Colour3f& default_val)
{
	if(node.type != JSONNode::Type_Object)
		throw glare::Exception("Expected type object.");

	for(size_t i=0; i<node.name_val_pairs.size(); ++i)
		if(node.name_val_pairs[i].name == name)
		{
			const JSONNode& array_node = parser.nodes[node.name_val_pairs[i].value_node_index];
			checkNodeType(array_node, JSONNode::Type_Array);

			if(array_node.child_indices.size() != 3)
				throw glare::Exception("Expected 3 elements in array.");

			float v[3];
			for(size_t q=0; q<3; ++q)
				v[q] = (float)parser.nodes[array_node.child_indices[q]].getDoubleValue();

			return Colour3f(v[0], v[1], v[2]);
		}

	return default_val;
}


static Colour4f parseColour4ChildArrayWithDefault(const JSONParser& parser, const JSONNode& node, const std::string& name, const Colour4f& default_val)
{
	if(node.type != JSONNode::Type_Object)
		throw glare::Exception("Expected type object.");

	for(size_t i=0; i<node.name_val_pairs.size(); ++i)
		if(node.name_val_pairs[i].name == name)
		{
			const JSONNode& array_node = parser.nodes[node.name_val_pairs[i].value_node_index];
			checkNodeType(array_node, JSONNode::Type_Array);

			if(array_node.child_indices.size() != 4)
				throw glare::Exception("Expected 4 elements in array.");
			
			float v[4];
			for(size_t q=0; q<4; ++q)
				v[q] = (float)parser.nodes[array_node.child_indices[q]].getDoubleValue();
			
			return Colour4f(v[0], v[1], v[2], v[3]);
		}

	return default_val;
}


static GLTFTextureObject parseTextureIfPresent(const JSONParser& parser, const JSONNode& node, const std::string& name)
{
	if(node.hasChild(name))
	{
		const JSONNode& tex_node = node.getChildObject(parser, name);

		GLTFTextureObject tex;
		tex.index		= tex_node.getChildUIntValueWithDefaultVal(parser, "index", std::numeric_limits<size_t>::max());
		tex.texCoord	= tex_node.getChildUIntValueWithDefaultVal(parser, "texCoord", 0);
		return tex;
	}
	else
	{
		return GLTFTextureObject();
	}
}


static const size_t GLTF_COMPONENT_TYPE_BYTE			= 5120;
static const size_t GLTF_COMPONENT_TYPE_UNSIGNED_BYTE	= 5121;
static const size_t GLTF_COMPONENT_TYPE_SHORT			= 5122;
static const size_t GLTF_COMPONENT_TYPE_UNSIGNED_SHORT	= 5123;
static const size_t GLTF_COMPONENT_TYPE_UNSIGNED_INT	= 5125;
static const size_t GLTF_COMPONENT_TYPE_FLOAT			= 5126;


//static const int GLTF_MODE_POINTS			= 0;
//static const int GLTF_MODE_LINES			= 1;
//static const int GLTF_MODE_LINE_LOOP		= 2;
//static const int GLTF_MODE_LINE_STRIP		= 3;
static const int GLTF_MODE_TRIANGLES		= 4;
//static const int GLTF_MODE_TRIANGLE_STRIP	= 5;
//static const int GLTF_MODE_TRIANGLE_FAN	= 6;


static std::string componentTypeString(size_t t)
{
	switch(t)
	{
		case GLTF_COMPONENT_TYPE_BYTE:			return "BYTE";
		case GLTF_COMPONENT_TYPE_UNSIGNED_BYTE:	return "UNSIGNED_BYTE";
		case GLTF_COMPONENT_TYPE_SHORT:			return "SHORT";
		case GLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return "UNSIGNED_SHORT";
		case GLTF_COMPONENT_TYPE_UNSIGNED_INT:	return "UNSIGNED_INT";
		case GLTF_COMPONENT_TYPE_FLOAT:			return "FLOAT";
		default: return "Unknown";
	};
}


static size_t componentTypeByteSize(size_t t)
{
	switch(t)
	{
	case GLTF_COMPONENT_TYPE_BYTE:			return 1;
	case GLTF_COMPONENT_TYPE_UNSIGNED_BYTE:	return 1;
	case GLTF_COMPONENT_TYPE_SHORT:			return 2;
	case GLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return 2;
	case GLTF_COMPONENT_TYPE_UNSIGNED_INT:	return 4;
	case GLTF_COMPONENT_TYPE_FLOAT:			return 4;
	default: return 1;
	};
}


static size_t typeNumComponents(const std::string& type)
{
	if(::stringEqual(type.c_str(), "SCALAR"))
		return 1;
	if(::stringEqual(type.c_str(), "VEC2"))
		return 2;
	if(::stringEqual(type.c_str(), "VEC3"))
		return 3;
	if(::stringEqual(type.c_str(), "VEC4"))
		return 4;
	if(::stringEqual(type.c_str(), "MAT2"))
		return 4;
	if(::stringEqual(type.c_str(), "MAT3"))
		return 9;
	if(::stringEqual(type.c_str(), "MAT4"))
		return 16;

	throw glare::Exception("Unknown type '" + type + "'");
}


// Just ignore primitives that do not have mode GLTF_MODE_TRIANGLES for now, instead of throwing an error.
static inline bool shouldLoadPrimitive(const GLTFPrimitive& primitive)
{
	return primitive.mode == GLTF_MODE_TRIANGLES;
}


static void processNodeToGetMeshCapacity(GLTFData& data, GLTFNode& node, size_t& total_num_indices, size_t& total_num_verts)
{
	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		GLTFMesh& mesh = getMesh(data, node.mesh);

		// Loop over primitive batches and get number of triangles and verts, add to total_num_indices and total_num_verts
		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTFPrimitive& primitive = *mesh.primitives[i];

			if(shouldLoadPrimitive(primitive))
			{
				const size_t num_vert_pos = getAccessorForAttribute(data, primitive, "POSITION").count;
				
				if(primitive.indices == std::numeric_limits<size_t>::max()) // If we don't have an indices accessor, we will use one index per vertex.
					total_num_indices += num_vert_pos;
				else
					total_num_indices += getAccessor(data, primitive.indices).count;
				
				total_num_verts += num_vert_pos;
			}
		}
	}

	// Process children
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw glare::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNodeToGetMeshCapacity(data, child, total_num_indices, total_num_verts);
	}
}


struct BufferViewInfo
{
	BufferViewInfo() : pos_offset(-1), normal_offset(-1), colour_offset(-1), uv0_offset(-1), count(0) {}
	int pos_offset;
	int normal_offset;
	int colour_offset;
	int uv0_offset;

	int count;
};


#if 0
static void processNodeToGetBufferViewInfo(GLTFData& data, GLTFNode& node, std::vector<BufferViewInfo>& buffer_view_info)
{
	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		GLTFMesh& mesh = getMesh(data, node.mesh);

		// Loop over primitive batches and get number of triangles and verts, add to total_num_tris and total_num_verts
		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTPrimitive& primitive = *mesh.primitives[i];

			if(primitive.mode != GLTF_MODE_TRIANGLES)
				throw glare::Exception("Only GLTF_MODE_TRIANGLES handled currently.");

			if(primitive.indices == std::numeric_limits<size_t>::max())
				throw glare::Exception("Primitve did not have indices..");


			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "POSITION");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);

				const size_t offset = accessor.byte_offset; // Offset relative to start of buffer view
				const size_t stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec3f);
				const size_t offset_mod_stride = offset % stride;
				conPrint("POSITION offset: " + toString(offset_mod_stride));

				if((buffer_view_info[accessor.buffer_view].pos_offset != -1) &&
					(buffer_view_info[accessor.buffer_view].pos_offset != (int)offset_mod_stride))
					throw glare::Exception("Inconsistent buffer view position offset");

				buffer_view_info[accessor.buffer_view].pos_offset = (int)offset_mod_stride;
				buffer_view_info[accessor.buffer_view].count = (int)accessor.count;
			}

			if(primitive.attributes.count("NORMAL"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "NORMAL");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);

				const size_t offset = accessor.byte_offset; // Offset relative to start of buffer view
				const size_t stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec3f);
				const size_t offset_mod_stride = offset % stride;
				conPrint("NORMAL offset: " + toString(offset_mod_stride));
				if((buffer_view_info[accessor.buffer_view].normal_offset != -1) &&
					(buffer_view_info[accessor.buffer_view].normal_offset != (int)offset_mod_stride))
					throw glare::Exception("Inconsistent buffer view normal offset");
			}

			if(primitive.attributes.count("COLOR_0"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "COLOR_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);

				const size_t offset = accessor.byte_offset; // Offset relative to start of buffer view
				const size_t stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec3f);
				const size_t offset_mod_stride = offset % stride;
				conPrint("COLOR_0 offset: " + toString(offset_mod_stride));
				if((buffer_view_info[accessor.buffer_view].colour_offset != -1) &&
					(buffer_view_info[accessor.buffer_view].colour_offset != (int)offset_mod_stride))
					throw glare::Exception("Inconsistent buffer view COLOR_0 offset");
			}

			if(primitive.attributes.count("TEXCOORD_0"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "TEXCOORD_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);

				const size_t offset = accessor.byte_offset; // Offset relative to start of buffer view
				const size_t stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec2f);
				const size_t offset_mod_stride = offset % stride;
				conPrint("TEXCOORD_0 offset: " + toString(offset_mod_stride));
				if((buffer_view_info[accessor.buffer_view].uv0_offset != -1) &&
					(buffer_view_info[accessor.buffer_view].uv0_offset != (int)offset_mod_stride))
					throw glare::Exception("Inconsistent buffer view TEXCOORD_0 offset");
			}
		}
	}

	// Process children
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw glare::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNodeToGetBufferViewInfo(data, child, buffer_view_info);
	}
}
#endif

// Copy some data from a buffer via an accessor, to a destination vertex_data buffer.
// Converts types and applies a scale as well.
template <typename SrcType, typename DestType, int N>
static void copyData(size_t accessor_count, const uint8* const offset_base, const size_t byte_stride, js::Vector<uint8, 16>& vertex_data, const size_t vert_write_i, 
	const size_t dest_vert_stride_B, const size_t dest_attr_offset_B, const DestType scale)
{
	// Bounds check destination addresses (should be done already, but check again)
	if(dest_attr_offset_B + sizeof(DestType) * N > dest_vert_stride_B)
		throw glare::Exception("Internal error: desination buffer overflow");

	if((vert_write_i + accessor_count) * dest_vert_stride_B > vertex_data.size())
		throw glare::Exception("Internal error: desination buffer overflow");

	uint8* const offset_dest = vertex_data.data() + vert_write_i * dest_vert_stride_B + dest_attr_offset_B;

	for(size_t z=0; z<accessor_count; ++z)
	{
		// Copy to v[]
		SrcType v[N];
		std::memcpy(v, offset_base + byte_stride * z, sizeof(SrcType) * N);

		// Write to dest
		DestType* dest = (DestType*)(offset_dest + z * dest_vert_stride_B);
		for(int c=0; c<N; ++c)
			dest[c] = (DestType)v[c] * scale;
	}
}


// vert_byte_stride - vertex byte stride, in bytes.  This value may have come from the buffer view (user controlled data)
// offset_B = Offset in bytes from start of buffer to the data we are accessing.  user controlled data.
static void checkAccessorBounds(const size_t vert_byte_stride, const size_t offset_B, const size_t value_size_B, const GLTFAccessor& accessor, const GLTFBuffer& buffer)
{
	if(accessor.count > 1000000000)
		throw glare::Exception("accessor_count is too large: " + toString(accessor.count));

	if(value_size_B > vert_byte_stride)
		throw glare::Exception("Invalid stride for buffer view: " + toString(vert_byte_stride) + " B");

	if(accessor.count > 0)
	{
		if(offset_B + vert_byte_stride * (accessor.count - 1) + value_size_B > buffer.data_size)
			throw glare::Exception("accessor tried to read out of bounds.");
	}
}


// vert_byte_stride - vertex byte stride, in bytes.  This value may have come from the buffer view (user controlled data)
// offset_B = Offset in bytes from start of buffer to the data we are accessing.  user controlled data.
static void checkAccessorBounds(const size_t vert_byte_stride, const size_t offset_B, const size_t value_size_B, const GLTFAccessor& accessor, const GLTFBuffer& buffer, int expected_num_components)
{
	checkAccessorBounds(vert_byte_stride, offset_B, value_size_B, accessor, buffer);

	if(typeNumComponents(accessor.type) != expected_num_components)
		throw glare::Exception("Invalid num components (type) for accessor.");
}


static void processNode(GLTFData& data, GLTFNode& node, const Matrix4f& parent_transform, BatchedMesh& mesh_out, js::Vector<uint32, 16>& uint32_indices_out, size_t& vert_write_i)
{
	const bool statically_apply_transform = data.skins.empty();

	const Matrix4f trans = Matrix4f::translationMatrix(node.translation.x, node.translation.y, node.translation.z);
	const Matrix4f rot = node.rotation.toMatrix();
	const Matrix4f scale = Matrix4f::scaleMatrix(node.scale.x, node.scale.y, node.scale.z);
	const Matrix4f node_transform = parent_transform * node.matrix * trans * rot * scale; // Matrix and T,R,S transforms should be mutually exclusive in GLTF files.  Just multiply them together however.

	node.node_transform = node_transform;

	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		GLTFMesh& mesh = getMesh(data, node.mesh);

		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTFPrimitive& primitive = *mesh.primitives[i];

			if(!shouldLoadPrimitive(primitive))
				continue;

			
			const GLTFAccessor& pos_accessor = getAccessorForAttribute(data, primitive, "POSITION");
			const size_t vert_pos_count = pos_accessor.count;

			//--------------------------------------- Read indices ---------------------------------------
			const size_t indices_write_i = uint32_indices_out.size();

			size_t primitive_num_indices;
			if(primitive.indices == std::numeric_limits<size_t>::max())
			{
				// Write one index per vertex.
				BatchedMesh::IndicesBatch batch;
				batch.indices_start = (uint32)indices_write_i;
				batch.material_index = (uint32)primitive.material;
				batch.num_indices = (uint32)vert_pos_count;
				mesh_out.batches.push_back(batch);

				uint32_indices_out.resize(uint32_indices_out.size() + vert_pos_count);

				for(size_t z=0; z<vert_pos_count; ++z)
					uint32_indices_out[indices_write_i + z] = (uint32)(z + vert_write_i);
				primitive_num_indices = vert_pos_count;
			}
			else
			{
				const GLTFAccessor& index_accessor = getAccessor(data, primitive.indices);
				const GLTFBufferView& index_buf_view = getBufferView(data, index_accessor.buffer_view);
				const GLTFBuffer& buffer = getBuffer(data, index_buf_view.buffer);

				const size_t offset_B = index_accessor.byte_offset + index_buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(index_accessor.component_type);
				const size_t byte_stride = (index_buf_view.byte_stride != 0) ? index_buf_view.byte_stride : value_size_B;

				if(offset_B + index_accessor.count * value_size_B > buffer.data_size)
					throw glare::Exception("Out of bounds while trying to read indices");

				BatchedMesh::IndicesBatch batch;
				batch.indices_start = (uint32)indices_write_i;
				batch.material_index = (uint32)primitive.material;
				batch.num_indices = (uint32)index_accessor.count;
				mesh_out.batches.push_back(batch);

				uint32_indices_out.resize(uint32_indices_out.size() + index_accessor.count);

				if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					for(size_t z=0; z<index_accessor.count; ++z)
						uint32_indices_out[indices_write_i + z] = *((const uint8*)(offset_base + z * byte_stride)) + (uint32)vert_write_i;
				}
				else if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					for(size_t z=0; z<index_accessor.count; ++z)
					{
						uint16 v;
						std::memcpy(&v, offset_base + z * byte_stride, sizeof(uint16));
						uint32_indices_out[indices_write_i + z] = (uint32)v + (uint32)vert_write_i;
					}
				}
				else if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					for(size_t z=0; z<index_accessor.count; ++z)
					{
						uint32 v;
						std::memcpy(&v, offset_base + z * byte_stride, sizeof(uint32));
						uint32_indices_out[indices_write_i + z] = v + (uint32)vert_write_i;
						//uint32_indices_out[indices_write_i + z] = *((const uint32*)(offset_base + z * byte_stride)); // NOTE: can only do this with alignment check
					}
				}
				else
					throw glare::Exception("Invalid index accessor component type: " + componentTypeString(index_accessor.component_type));

				primitive_num_indices = index_accessor.count;
			}

			//--------------------------------------- Process vertex positions ---------------------------------------
			const BatchedMesh::VertAttribute& pos_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Position);
			{
				const Matrix4f transform = statically_apply_transform ? node_transform : Matrix4f::identity();
				
				const GLTFBufferView& buf_view = getBufferView(data, pos_accessor.buffer_view);
				const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = pos_accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(pos_accessor.component_type) * typeNumComponents(pos_accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, pos_accessor, buffer, /*expected_num_components=*/3);

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = pos_attr.offset_B;

				doRuntimeCheck(pos_attr.component_type == BatchedMesh::ComponentType_Float); // We store positions in float format.

				// POSITION must be FLOAT
				if(pos_accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<pos_accessor.count; ++z)
					{
						// NOTE: alignment is dodgy here, assuming offset_base is 4-byte aligned.
						const Vec4f p_os(
							((const float*)(offset_base + byte_stride * z))[0],
							((const float*)(offset_base + byte_stride * z))[1],
							((const float*)(offset_base + byte_stride * z))[2],
							1
						);

						const Vec4f p_ws = transform * p_os;

						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[0] = p_ws[0];
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[1] = p_ws[1];
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[2] = p_ws[2];
					}
				}
				else
					throw glare::Exception("Invalid POSITION component type");

				
			}

			//--------------------------------------- Process vertex normals ---------------------------------------
			if(primitive.attributes.count("NORMAL"))
			{
				Matrix4f normal_transform;
				if(statically_apply_transform)
					node_transform.getUpperLeftInverseTranspose(normal_transform);
				else
					normal_transform = Matrix4f::identity();

				const Matrix4f transform = statically_apply_transform ? node_transform : Matrix4f::identity();

				const BatchedMesh::VertAttribute& normals_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Normal);

				const GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "NORMAL");
				const GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer, /*expected_num_components=*/3);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = normals_attr.offset_B;

				doRuntimeCheck(normals_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store normals in packed format.

				// NORMAL must be FLOAT
				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<accessor.count; ++z)
					{
						// NOTE: alignment is dodgy here, assuming offset_base is 4-byte aligned.
						const Vec4f n_os(
							((const float*)(offset_base + byte_stride * z))[0],
							((const float*)(offset_base + byte_stride * z))[1],
							((const float*)(offset_base + byte_stride * z))[2],
							0
						);

						const Vec4f n_ws = normalise(normal_transform * n_os);

						const uint32 packed = batchedMeshPackNormal(n_ws);

						*(uint32*)(&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B]) = packed;
					}
				}
				else
					throw glare::Exception("Invalid NORMAL component type");
			}
			else
			{
				if(data.attr_present.normal_present)
				{
					// Pad with normals.  This is a hack, needed because we only have one vertex layout per mesh.

					const BatchedMesh::VertAttribute& normals_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Normal);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = normals_attr.offset_B;

					// Initialise with a default normal value
					doRuntimeCheck(normals_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store normals in packed format.
					for(size_t z=0; z<vert_pos_count; ++z)
					{
						const Vec4f n_ws(0,0,1,0);
						const uint32 packed = batchedMeshPackNormal(n_ws);
						*(uint32*)(&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B]) = packed;
					}

					for(size_t z=0; z<primitive_num_indices/3; z++) // For each tri, splat geometric normal to the vert normal for each vert
					{
						uint32 v0i = uint32_indices_out[indices_write_i + z * 3 + 0];
						uint32 v1i = uint32_indices_out[indices_write_i + z * 3 + 1];
						uint32 v2i = uint32_indices_out[indices_write_i + z * 3 + 2];

						const Vec4f v0(
							((float*)&mesh_out.vertex_data[v0i * dest_vert_stride_B + pos_attr.offset_B])[0],
							((float*)&mesh_out.vertex_data[v0i * dest_vert_stride_B + pos_attr.offset_B])[1],
							((float*)&mesh_out.vertex_data[v0i * dest_vert_stride_B + pos_attr.offset_B])[2],
							1);

						const Vec4f v1(
							((float*)&mesh_out.vertex_data[v1i * dest_vert_stride_B + pos_attr.offset_B])[0],
							((float*)&mesh_out.vertex_data[v1i * dest_vert_stride_B + pos_attr.offset_B])[1],
							((float*)&mesh_out.vertex_data[v1i * dest_vert_stride_B + pos_attr.offset_B])[2],
							1);

						const Vec4f v2(
							((float*)&mesh_out.vertex_data[v2i * dest_vert_stride_B + pos_attr.offset_B])[0],
							((float*)&mesh_out.vertex_data[v2i * dest_vert_stride_B + pos_attr.offset_B])[1],
							((float*)&mesh_out.vertex_data[v2i * dest_vert_stride_B + pos_attr.offset_B])[2],
							1);

						const Vec4f normal = normalise(crossProduct(v1 - v0, v2 - v0));
						const uint32 packed = batchedMeshPackNormal(normal);
						*(uint32*)(&mesh_out.vertex_data[v0i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
						*(uint32*)(&mesh_out.vertex_data[v1i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
						*(uint32*)(&mesh_out.vertex_data[v2i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
					}
				}
			}

			//--------------------------------------- Process vertex colours ---------------------------------------
			if(primitive.attributes.count("COLOR_0"))
			{
				const BatchedMesh::VertAttribute& colour_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Colour);

				const GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "COLOR_0");
				const GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				// It can be VEC3 or VEC4
				const size_t num_components = typeNumComponents(accessor.type);
				if(!((num_components == 3) || (num_components == 4)))
					throw glare::Exception("Invalid num components (type) for accessor.");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = colour_attr.offset_B;

				doRuntimeCheck(colour_attr.component_type == BatchedMesh::ComponentType_Float); // We store colours in float format for now.

				// COLOR_0 must be FLOAT, UNSIGNED_BYTE, or UNSIGNED_SHORT.
				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					if(num_components == 3)
						copyData<float, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f);
					else
						copyData<float, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					if(num_components == 3)
						copyData<uint8, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 255);
					else
						copyData<uint8, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 255);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					if(num_components == 3)
						copyData<uint16, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 65535);
					else
						copyData<uint16, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 65535);
				}
				else
					throw glare::Exception("Invalid COLOR_0 component type");
			}
			else
			{
				if(data.attr_present.vert_col_present)
				{
					// Pad with colours.  This is a hack, needed because we only have one vertex layout per mesh.
					const BatchedMesh::VertAttribute& colour_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Colour);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = colour_attr.offset_B;

					doRuntimeCheck(colour_attr.component_type == BatchedMesh::ComponentType_Float); // We store colours in float format for now.
					for(size_t z=0; z<vert_pos_count; ++z)
					{
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[0] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[1] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[2] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[3] = 1.f;
					}
				}
			}

			// Process uvs
			if(primitive.attributes.count("TEXCOORD_0"))
			{
				const BatchedMesh::VertAttribute& texcoord_0_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_UV_0);

				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "TEXCOORD_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer, /*expected_num_components=*/2);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = texcoord_0_attr.offset_B;

				doRuntimeCheck(texcoord_0_attr.component_type == BatchedMesh::ComponentType_Float); // We store texcoords in float format for now.

				// TEXCOORD_0 must be FLOAT, UNSIGNED_BYTE, or UNSIGNED_SHORT.
				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					copyData<float, float, 2>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					copyData<uint8, float, 2>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 255);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					copyData<uint16, float, 2>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 65535);
				}
				else
					throw glare::Exception("Invalid TEXCOORD_0 component type");
			}
			else
			{
				if(data.attr_present.texcoord_0_present)
				{
					// Pad with UV zeroes.  This is a bit of a hack, needed because we only have one vertex layout per mesh.
					const BatchedMesh::VertAttribute& texcoord_0_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_UV_0);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = texcoord_0_attr.offset_B;

					doRuntimeCheck(texcoord_0_attr.component_type == BatchedMesh::ComponentType_Float); // We store uvs in float format for now.
					for(size_t z=0; z<vert_pos_count; ++z)
					{
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[0] = 0.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[1] = 0.f;
					}
				}
			}

			//--------------------------------------- Process vertex joint indices ---------------------------------------
			if(primitive.attributes.count("JOINTS_0"))
			{
				const BatchedMesh::VertAttribute& joint_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Joints);

				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "JOINTS_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer, /*expected_num_components=*/4);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = joint_attr.offset_B;

				doRuntimeCheck(joint_attr.component_type == BatchedMesh::ComponentType_UInt16); // We will always store uint16 joint indices for now.

				// JOINTS_0 must be UNSIGNED_BYTE or UNSIGNED_SHORT
				if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					copyData<uint8, uint16, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					copyData<uint16, uint16, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1);
				}
				else
					throw glare::Exception("Unhandled component type for JOINTS_0");
			}
			else
			{
				if(data.attr_present.joints_present)
				{
					// Pad with zeroes.  This is a bit of a hack, needed because we only have one vertex layout per mesh.
					const BatchedMesh::VertAttribute& joint_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Joints);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = joint_attr.offset_B;

					doRuntimeCheck(joint_attr.component_type == BatchedMesh::ComponentType_UInt16); // We will always store uint16 joint indices for now.
					for(size_t z=0; z<vert_pos_count; ++z)
						for(int c=0; c<4; ++c)
							((uint16*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[c] = 0;
				}
			}


			//--------------------------------------- Process vertex weights (skinning joint weights) ---------------------------------------
			if(primitive.attributes.count("WEIGHTS_0"))
			{
				const BatchedMesh::VertAttribute& weights_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Weights);

				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "WEIGHTS_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer, /*expected_num_components=*/4);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = weights_attr.offset_B;

				doRuntimeCheck(weights_attr.component_type == BatchedMesh::ComponentType_Float); // We store weights as floats

				// WEIGHTS_0 must be FLOAT, UNSIGNED_BYTE (normalised) or UNSIGNED_SHORT (normalised)
				if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					copyData<uint8, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 255);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					copyData<uint16, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 65535);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					copyData<float, float, 4>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f);
				}
				else
					throw glare::Exception("unhandled accessor.component_type for weights attr");
			}
			else
			{
				if(data.attr_present.weights_present)
				{
					// Pad with zeroes.  This is a bit of a hack, needed because we only have one vertex layout per mesh.
					const BatchedMesh::VertAttribute& weights_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Weights);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = weights_attr.offset_B;

					doRuntimeCheck(weights_attr.component_type == BatchedMesh::ComponentType_Float); // We store uvs in float format for now.
					for(size_t z=0; z<vert_pos_count; ++z)
						for(int c=0; c<4; ++c)
							((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[c] = 0.f;
				}
			}

			vert_write_i += vert_pos_count;

		} // End for each primitive batch
	} // End if(node.mesh != std::numeric_limits<size_t>::max())


	// Process child nodes
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw glare::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNode(data, child, node_transform, mesh_out, uint32_indices_out, vert_write_i);
	}
}


static void processImage(GLTFData& data, GLTFImage& image, const std::string& gltf_folder)
{
	if(image.uri.empty())
	{
		// If the Image URI is empty, this image refers to the data in a GLB file.  Save it to disk separately so our image loaders can load it.
		
		GLTFBufferView& buffer_view = getBufferView(data, image.buffer_view);
		GLTFBuffer& buffer = getBuffer(data, buffer_view.buffer);

		// Work out extension to use - see https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
		std::string extension;
		if(image.mime_type == "image/bmp")
			extension = "bmp";
		else if(image.mime_type == "image/gif")
			extension = "gif";
		else if(image.mime_type == "image/jpeg")
			extension = "jpg";
		else if(image.mime_type == "image/png")
			extension = "png";
		else if(image.mime_type == "image/svg+xml")
			extension = "svg";
		else if(image.mime_type == "image/tiff")
			extension = "tif";
		else if(image.mime_type == "image/webp")
			extension = "webp";
		else
			throw glare::Exception("Unknown MIME type for image.");

		// Check length
		if(buffer_view.byte_length == 0)
			throw glare::Exception("Image buffer view too small.");
		if(buffer_view.byte_offset + buffer_view.byte_length > buffer.data_size)
			throw glare::Exception("Image buffer view too large.");

		// Compute a hash over the data to get a semi-unique filename.
		const uint64 hash = XXH64((const char*)buffer.binary_data + buffer_view.byte_offset, buffer_view.byte_length, /*seed=*/1);

		const std::string path = PlatformUtils::getTempDirPath() + "/GLB_image_" + toString(hash) + "." + extension;

		try
		{
			FileUtils::writeEntireFile(path, (const char*)buffer.binary_data + buffer_view.byte_offset, buffer_view.byte_length);
		}
		catch(FileUtils::FileUtilsExcep& e)
		{
			throw glare::Exception("Error while writing temp image file: " + e.what());
		}

		// Update GLTF image to use URI on disk
		image.uri = path;
	}
}


static void processMaterial(GLTFData& data, GLTFMaterial& mat, const std::string& gltf_folder, GLTFResultMaterial& mat_out)
{
	if(mat.pbrMetallicRoughness_present)
	{
		mat_out.diffuse = Colour3f(mat.baseColorFactor.x[0], mat.baseColorFactor.x[1], mat.baseColorFactor.x[2]);
		if(mat.baseColorTexture.valid())
		{
			GLTFTexture& texture = getTexture(data, mat.baseColorTexture.index);
			GLTFImage& image = getImage(data, texture.source);
			const std::string path = FileUtils::join(gltf_folder, image.uri);
			mat_out.diffuse_map.path = path;
		}
		mat_out.roughness = mat.roughnessFactor;
		mat_out.metallic = mat.metallicFactor;
	}

	if(mat.KHR_materials_pbrSpecularGlossiness_present)
	{
		mat_out.diffuse = Colour3f(mat.diffuseFactor.x[0], mat.diffuseFactor.x[1], mat.diffuseFactor.x[2]);
		mat_out.alpha = mat.diffuseFactor.x[3];
		if(mat.diffuseTexture.valid())
		{
			GLTFTexture& texture = getTexture(data, mat.diffuseTexture.index);
			GLTFImage& image = getImage(data, texture.source);
			const std::string path = FileUtils::join(gltf_folder, image.uri);
			mat_out.diffuse_map.path = path;
		}

		/*
		From https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness
		alpha = (1 - g)^2
		our roughness is defined as alpha = r^3
		so
		r^3 = (1 - g)^2
		r = (1 - g)^(2/3)
		*/
		mat_out.roughness = pow(1.0f - mat.glossinessFactor, 0.6666666f);
	}
}


static void processAnimation(GLTFData& data, const GLTFAnimation& anim, const std::string& gltf_folder, AnimationDatum& anim_data_out)
{
	conPrint("Processing anim " + anim.name + "...");

	anim_data_out.name = anim.name;


	// Do a pass to get that largest input and output accessor indices.
	int largest_input_accessor = -1;
	int largest_output_accessor = -1;

	for(size_t c=0; c<anim.channels.size(); ++c)
	{
		const GLTFChannel& channel = anim.channels[c];

		// Get sampler
		checkProperty(channel.sampler >= 0 && channel.sampler < (int)anim.samplers.size(), "invalid sampler index");
		const GLTFSampler& sampler = anim.samplers[channel.sampler];

		largest_input_accessor  = myMax(largest_input_accessor,  sampler.input);
		largest_output_accessor = myMax(largest_output_accessor, sampler.output);
	}

	// sanity check largest_input_accessor etc.
	checkProperty(largest_input_accessor  >= -1 && largest_input_accessor < 10000,  "invalid animation input accessor");
	checkProperty(largest_output_accessor >= -1 && largest_output_accessor < 10000, "invalid animation output accessor");

	anim_data_out.keyframe_times.resize(largest_input_accessor  + 1);
	anim_data_out.output_data   .resize(largest_output_accessor + 1);


	//--------------------------- Set default translations, scales, rotations with the non-animated static values. ---------------------------

	anim_data_out.per_anim_node_data.resize(data.nodes.size());
	for(size_t n=0; n<data.nodes.size(); ++n)
	{
		anim_data_out.per_anim_node_data[n].translation_input_accessor = -1;
		anim_data_out.per_anim_node_data[n].translation_output_accessor = -1;
		anim_data_out.per_anim_node_data[n].rotation_input_accessor = -1;
		anim_data_out.per_anim_node_data[n].rotation_output_accessor = -1;
		anim_data_out.per_anim_node_data[n].scale_input_accessor = -1;
		anim_data_out.per_anim_node_data[n].scale_output_accessor = -1;
	}
	

	for(size_t c=0; c<anim.channels.size(); ++c)
	{
		const GLTFChannel& channel = anim.channels[c];
		const GLTFSampler& sampler = anim.samplers[channel.sampler]; // Has been bound-checked earlier

		checkProperty(sampler.interpolation == GLTFSampler::Interpolation_linear || sampler.interpolation == GLTFSampler::Interpolation_step, "Only linear and step interpolation types supported currently for animations.");

		//---------------- read keyframe time values from the input accessor -----------------
		std::vector<float>& time_vals = anim_data_out.keyframe_times[sampler.input];
		if(time_vals.empty()) // If we have not already read the data for this input accessor:
		{
			const GLTFAccessor& input_accessor = getAccessor(data, sampler.input);

			// input should be "a set of floating point scalar values representing linear time in seconds"
			checkProperty(input_accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT, "anim input component_type was not float");
			checkProperty(input_accessor.type == "SCALAR", "anim input type was not SCALAR");

			const GLTFBufferView& buf_view = getBufferView(data, input_accessor.buffer_view);
			const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);
			const size_t offset_B = input_accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
			const uint8* offset_base = buffer.binary_data + offset_B;
			const size_t value_size_B = componentTypeByteSize(input_accessor.component_type) * typeNumComponents(input_accessor.type);
			const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

			checkAccessorBounds(byte_stride, offset_B, value_size_B, input_accessor, buffer, /*expected_num_components=*/1);

			time_vals.resize(input_accessor.count);
			for(size_t i=0; i<input_accessor.count; ++i)
				std::memcpy(&time_vals[i], offset_base + byte_stride * i, sizeof(float));
		}

		const GLTFAccessor& output_accessor = getAccessor(data, sampler.output);
		
		// For linear and step interpolation types, we have this constraint:
		// "The number output of elements must equal the number of input elements."
		// For cubic-spline there are tangents to handle as well.
		checkProperty(time_vals.size() == output_accessor.count, "Animation: The number output of elements must equal the number of input elements.");

		js::Vector<Vec4f, 16>& output_data = anim_data_out.output_data[sampler.output];

		if(output_data.empty()) // If we have not already read the data for this output accessor:
		{
			// NOTE: GLTF translations and scales must be FLOAT.  But rotation can be a bunch of stuff, BYTE etc..  TODO: handle.
			if(output_accessor.component_type != GLTF_COMPONENT_TYPE_FLOAT)
				throw glare::Exception("anim output component_type was not float");

			const GLTFBufferView& buf_view = getBufferView(data, output_accessor.buffer_view);
			const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);
			const size_t offset_B = output_accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
			const uint8* offset_base = buffer.binary_data + offset_B;
			const size_t value_size_B = componentTypeByteSize(output_accessor.component_type) * typeNumComponents(output_accessor.type);
			const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

			checkAccessorBounds(byte_stride, offset_B, value_size_B, output_accessor, buffer);

			output_data.resize(output_accessor.count, Vec4f(0.f));

			if(channel.target_path == GLTFChannel::Path_translation)
			{
				anim_data_out.per_anim_node_data[channel.target_node].translation_input_accessor  = sampler.input;
				anim_data_out.per_anim_node_data[channel.target_node].translation_output_accessor = sampler.output;

				checkProperty(typeNumComponents(output_accessor.type) == 3, "invalid number of components for animated translation");

				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float) * 3);
			}
			else if(channel.target_path == GLTFChannel::Path_rotation)
			{
				anim_data_out.per_anim_node_data[channel.target_node].rotation_input_accessor  = sampler.input;
				anim_data_out.per_anim_node_data[channel.target_node].rotation_output_accessor = sampler.output;

				checkProperty(typeNumComponents(output_accessor.type) == 4, "invalid number of components for animated rotation");

				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float) * 4);
			}
			else if(channel.target_path == GLTFChannel::Path_scale)
			{
				anim_data_out.per_anim_node_data[channel.target_node].scale_input_accessor  = sampler.input;
				anim_data_out.per_anim_node_data[channel.target_node].scale_output_accessor = sampler.output;

				checkProperty(typeNumComponents(output_accessor.type) == 3, "invalid number of components for animated scale");

				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float) * 3);
			}
		}
	}

	conPrint("done.");
}


static void processSkin(GLTFData& data, const GLTFSkin& skin, const std::string& gltf_folder, AnimationData& anim_data)
{
	conPrint("Processing skin " + skin.name + "...");

	// "Each skin is defined by the inverseBindMatrices property (which points to an accessor with IBM data), used to bring coordinates being skinned into the same space as each joint"
	js::Vector<Matrix4f, 16> ibms(skin.joints.size());

	// Read inverse bind matrices
	if(skin.inverse_bind_matrices >= 0) // inverseBindMatrices are optional, default is identity matrix.
	{
		const GLTFAccessor& accessor = getAccessor(data, skin.inverse_bind_matrices);

		checkProperty(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT, "anim inverse_bind_matrices component_type was not float");
		checkProperty(accessor.type == "MAT4", "anim inverse_bind_matrices type was not MAT4");

		const GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
		const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);
		const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
		const uint8* offset_base = buffer.binary_data + offset_B;
		const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
		const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

		checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer);

		checkProperty(accessor.count == ibms.size(), "accessor.count had invalid size"); // skin.joints: "The array length must be the same as the count property of the inverseBindMatrices accessor (when defined)."

		for(int i=0; i<accessor.count; ++i)
			std::memcpy(&ibms[i].e, offset_base + byte_stride * i, sizeof(float) * 16);
	}
	else
	{
		for(size_t i=0; i<ibms.size(); ++i)
			ibms[i] = Matrix4f::identity();
	}

	// Assign bind matrices to nodes
	for(size_t i=0; i<skin.joints.size(); ++i)
	{
		const int node_i = skin.joints[i];
		checkProperty(node_i >= 0 && node_i < (int)data.nodes.size(), "node_index was invalid");
		anim_data.nodes[node_i].inverse_bind_matrix = ibms[i];
	}

	anim_data.joint_nodes = skin.joints;

	// Skeleton is "The index of the node used as a skeleton root", so we want to use the transform of that node.
	if(skin.skeleton != -1)
		anim_data.skeleton_root_transform = data.nodes[skin.skeleton]->node_transform;
	else
		anim_data.skeleton_root_transform = Matrix4f::identity();

	conPrint("done.");
}


struct GLBHeader
{
	uint32 magic;
	uint32 version;
	uint32 length;
};


struct GLBChunkHeader
{
	uint32 chunk_length;
	uint32 chunk_type;
};


static const uint32 CHUNK_TYPE_JSON = 0x4E4F534A;
static const uint32 CHUNK_TYPE_BIN  = 0x004E4942;


Reference<BatchedMesh> FormatDecoderGLTF::loadGLBFile(const std::string& pathname, GLTFLoadedData& data_out) // throws glare::Exception on failure
{
	MemMappedFile file(pathname);

	if(file.fileSize() < sizeof(GLBHeader) + sizeof(GLBChunkHeader))
		throw glare::Exception("File too small.");

	// Read header
	GLBHeader header;
	std::memcpy(&header, file.fileData(), sizeof(GLBHeader));

	// Read JSON chunk header
	GLBChunkHeader json_header;
	std::memcpy(&json_header, (const uint8*)file.fileData() + 12, sizeof(GLBChunkHeader));
	if(json_header.chunk_type != CHUNK_TYPE_JSON)
		throw glare::Exception("Expected JSON chunk type");

	// Check json_header length
	if(12 + sizeof(GLBChunkHeader) + json_header.chunk_length > file.fileSize())
		throw glare::Exception("JSON Chunk too large.");

	// Read binary buffer chunk header
	const size_t bin_buf_chunk_header_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(20 + (size_t)json_header.chunk_length, 4);
	GLBChunkHeader bin_buf_header;
	std::memcpy(&bin_buf_header, (const uint8*)file.fileData() + bin_buf_chunk_header_offset, sizeof(GLBChunkHeader));
	if(bin_buf_header.chunk_type != CHUNK_TYPE_BIN)
		throw glare::Exception("Expected BIN chunk type");

	// Check bin_buf_header length
	if(bin_buf_chunk_header_offset + sizeof(GLBChunkHeader) + bin_buf_header.chunk_length > file.fileSize())
		throw glare::Exception("Bin buf Chunk too large.");

	// Make a buffer object for it
	GLTFBufferRef buffer = new GLTFBuffer();
	buffer->binary_data = (const uint8*)file.fileData() + bin_buf_chunk_header_offset + 8;
	buffer->data_size = bin_buf_header.chunk_length;

	if(true)
	{
		// Save JSON to disk for debugging
		const std::string json((const char*)file.fileData() + 20, json_header.chunk_length);
		conPrint(PlatformUtils::getCurrentWorkingDirPath());
		FileUtils::writeEntireFileTextMode(pathname + ".json", json);
	}

	// Parse JSON chunk
	JSONParser parser;
	parser.parseBuffer((const char*)file.fileData() + 20, json_header.chunk_length);

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	return loadGivenJSON(parser, gltf_base_dir, buffer, data_out);
}


Reference<BatchedMesh> FormatDecoderGLTF::loadGLTFFile(const std::string& pathname, GLTFLoadedData& data_out)
{
	JSONParser parser;
	parser.parseFile(pathname);

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	return loadGivenJSON(parser, gltf_base_dir, /*glb_bin_buffer=*/NULL, data_out);
}


Reference<BatchedMesh> FormatDecoderGLTF::loadGivenJSON(JSONParser& parser, const std::string gltf_base_dir, const GLTFBufferRef& glb_bin_buffer,
	GLTFLoadedData& data_out) // throws glare::Exception on failure
{
	const JSONNode& root = parser.nodes[0];
	checkNodeType(root, JSONNode::Type_Object);

	GLTFData data;

	// Load buffers
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "buffers")
		{
			const JSONNode& buffers_node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(buffers_node_array, JSONNode::Type_Array);

			for(size_t z=0; z<buffers_node_array.child_indices.size(); ++z)
			{
				const JSONNode& buffer_node = parser.nodes[buffers_node_array.child_indices[z]];

				GLTFBufferRef buffer = new GLTFBuffer();
				if(buffer_node.hasChild("uri"))
					buffer->uri = buffer_node.getChildStringValue(parser, "uri");
				else
				{
					if(glb_bin_buffer.isNull())
						throw glare::Exception("buffer with undefined uri, not in a GLB file.");
					buffer = glb_bin_buffer;
				}
				data.buffers.push_back(buffer);
			}
		}

	// Load buffer views
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "bufferViews")
		{
			const JSONNode& view_node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(view_node_array, JSONNode::Type_Array);

			for(size_t z=0; z<view_node_array.child_indices.size(); ++z)
			{
				const JSONNode& view_node = parser.nodes[view_node_array.child_indices[z]];

				GLTFBufferViewRef view = new GLTFBufferView();
				view->buffer =		view_node.getChildUIntValue(parser, "buffer");
				view->byte_length = view_node.getChildUIntValue(parser, "byteLength");
				view->byte_offset = view_node.getChildUIntValueWithDefaultVal(parser, "byteOffset", 0);
				view->byte_stride = view_node.getChildUIntValueWithDefaultVal(parser, "byteStride", 0);
				view->name =		view_node.getChildStringValueWithDefaultVal(parser, "name", "");
				view->target =		view_node.getChildUIntValueWithDefaultVal(parser, "target", 0);
				data.buffer_views.push_back(view);
			}
		}


	// Load accessors
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "accessors")
		{
			const JSONNode& accessor_node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(accessor_node_array, JSONNode::Type_Array);

			for(size_t z=0; z<accessor_node_array.child_indices.size(); ++z)
			{
				const JSONNode& accessor_node = parser.nodes[accessor_node_array.child_indices[z]];

				GLTFAccessorRef accessor = new GLTFAccessor();
				accessor->buffer_view		= accessor_node.getChildUIntValueWithDefaultVal(parser, "bufferView", 0);
				accessor->byte_offset		= accessor_node.getChildUIntValueWithDefaultVal(parser, "byteOffset", 0);
				accessor->component_type	= accessor_node.getChildUIntValue(parser, "componentType");
				accessor->count				= accessor_node.getChildUIntValue(parser, "count");
				accessor->type				= accessor_node.getChildStringValue(parser, "type");
				data.accessors.push_back(accessor);
			}
		}


	// Load images
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "images")
		{
			const JSONNode& image_node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(image_node_array, JSONNode::Type_Array);

			for(size_t z=0; z<image_node_array.child_indices.size(); ++z)
			{
				const JSONNode& image_node = parser.nodes[image_node_array.child_indices[z]];
				GLTFImageRef image = new GLTFImage();
				if(image_node.hasChild("uri"))
				{
					image->uri = image_node.getChildStringValue(parser, "uri");
				}
				else // Else image is embedded in GLB file:
				{
					image->buffer_view = image_node.getChildUIntValue(parser, "bufferView");
					image->mime_type   = image_node.getChildStringValue(parser, "mimeType");
				}
				data.images.push_back(image);
			}
		}


	// Load textures
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "textures")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& texture_node = parser.nodes[node_array.child_indices[z]];
				GLTFTextureRef tex = new GLTFTexture();
				tex->sampler = texture_node.getChildUIntValueWithDefaultVal(parser, "sampler", std::numeric_limits<size_t>::max());
				tex->source = texture_node.getChildUIntValueWithDefaultVal(parser, "source", 0); // NOTE: not required for some reason.
				data.textures.push_back(tex);
			}
		}

	// Load materials
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "materials")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& mat_node = parser.nodes[node_array.child_indices[z]];

				GLTFMaterialRef mat = new GLTFMaterial();

				if(mat_node.hasChild("name")) mat->name = mat_node.getChildStringValue(parser, "name");

				if(mat_node.hasChild("extensions"))
				{
					const JSONNode& extensions_node = mat_node.getChildObject(parser, "extensions");
					
					if(extensions_node.hasChild("KHR_materials_pbrSpecularGlossiness"))
					{
						mat->KHR_materials_pbrSpecularGlossiness_present = true;

						const JSONNode& pbr_node = extensions_node.getChildObject(parser, "KHR_materials_pbrSpecularGlossiness");

						mat->diffuseFactor = parseColour4ChildArrayWithDefault(parser, pbr_node, "diffuseFactor", Colour4f(1, 1, 1, 1));
						mat->diffuseTexture = parseTextureIfPresent(parser, pbr_node, "diffuseTexture");
						mat->specularFactor = parseColour3ChildArrayWithDefault(parser, pbr_node, "specularFactor", Colour3f(1, 1, 1));
						mat->glossinessFactor	= (float)pbr_node.getChildDoubleValueWithDefaultVal(parser, "glossinessFactor", 1.0);
						mat->specularGlossinessTexture = parseTextureIfPresent(parser, pbr_node, "specularGlossinessTexture");
					}
				}

				// pbrMetallicRoughness
				if(mat_node.hasChild("pbrMetallicRoughness"))
				{
					const JSONNode& pbr_node = mat_node.getChildObject(parser, "pbrMetallicRoughness");

					mat->pbrMetallicRoughness_present = true;
					mat->baseColorTexture = parseTextureIfPresent(parser, pbr_node, "baseColorTexture");
					mat->baseColorFactor = parseColour4ChildArrayWithDefault(parser, pbr_node, "baseColorFactor", Colour4f(1, 1, 1, 1));
					mat->roughnessFactor	= (float)pbr_node.getChildDoubleValueWithDefaultVal(parser, "roughnessFactor", 1.0);
					mat->metallicFactor		= (float)pbr_node.getChildDoubleValueWithDefaultVal(parser, "metallicFactor", 1.0);
				}
				else
				{
					mat->pbrMetallicRoughness_present = false;
					mat->roughnessFactor = 0.5;
					mat->metallicFactor = 0.0;
				}

				data.materials.push_back(mat);
			}
		}


	// Load meshes	
	bool default_mat_used = false;
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "meshes")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& mesh_node = parser.nodes[node_array.child_indices[z]];

				GLTFMeshRef gltf_mesh = new GLTFMesh();
				
				const JSONNode& primitives_node_array = mesh_node.getChildArray(parser, "primitives");

				for(size_t q=0; q<primitives_node_array.child_indices.size(); ++q)
				{
					const JSONNode& primitive_node = parser.nodes[primitives_node_array.child_indices[q]];

					GLTFPrimitiveRef primitive = new GLTFPrimitive();

					const JSONNode& attributes_ob = primitive_node.getChildObject(parser, "attributes");
					for(size_t t=0; t<attributes_ob.name_val_pairs.size(); ++t)
						primitive->attributes[attributes_ob.name_val_pairs[t].name] =
							parser.nodes[attributes_ob.name_val_pairs[t].value_node_index].getUIntValue();

					primitive->indices	= primitive_node.getChildUIntValueWithDefaultVal(parser, "indices", std::numeric_limits<size_t>::max()); // optional
					primitive->material = primitive_node.getChildUIntValueWithDefaultVal(parser, "material", data.materials.size()); // Optional. Default mat index is index of default material.
					primitive->mode		= primitive_node.getChildUIntValueWithDefaultVal(parser, "mode", 4); // Optional, default = 4.

					// See if this primitive uses the default material
					default_mat_used = default_mat_used || (primitive->material == data.materials.size());

					gltf_mesh->primitives.push_back(primitive);
				}

				data.meshes.push_back(gltf_mesh);
			}
		}

	// Append default material if needed
	if(default_mat_used)
	{
		GLTFMaterialRef mat = new GLTFMaterial();
		mat->name = "default";
		mat->KHR_materials_pbrSpecularGlossiness_present = false;
		mat->pbrMetallicRoughness_present = true;
		mat->baseColorFactor = Colour4f(0.5f, 0.5f, 0.5f, 1);
		mat->metallicFactor = 0.0; 
		mat->roughnessFactor = 0.5;

		data.materials.push_back(mat);
	}


	// Load nodes	
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "nodes")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& node_node = parser.nodes[node_array.child_indices[z]];
				GLTFNodeRef node = new GLTFNode();
				if(node_node.hasChild("name")) node->name = node_node.getChildStringValue(parser, "name");
				if(node_node.hasChild("mesh")) node->mesh = node_node.getChildUIntValue(parser, "mesh");

				// parse children array
				if(node_node.hasChild("children"))
				{
					const JSONNode& children_array_node = node_node.getChildArray(parser, "children");
					for(size_t q=0; q<children_array_node.child_indices.size(); ++q)
					{
						node->children.push_back(parser.nodes[children_array_node.child_indices[q]].getUIntValue());
					}
				}

				if(node_node.hasChild("rotation"))
				{
					const JSONNode& rotation_node = node_node.getChildArray(parser, "rotation");
					if(rotation_node.child_indices.size() != 4)
						throw glare::Exception("Expected 4 elements in rotation array.");
					float v[4];
					for(size_t q=0; q<4; ++q)
						v[q] = (float)parser.nodes[rotation_node.child_indices[q]].getDoubleValue();
					node->rotation = Quatf(v[0], v[1], v[2], v[3]);
				}
				else
					node->rotation = Quatf::identity();

				if(node_node.hasChild( "scale"))
				{
					const JSONNode& scale_node = node_node.getChildArray(parser, "scale");
					if(scale_node.child_indices.size() != 3)
						throw glare::Exception("Expected 3 elements in scale array.");
					for(uint32 q=0; q<3; ++q)
						node->scale[q] = (float)parser.nodes[scale_node.child_indices[q]].getDoubleValue();
				}
				else
					node->scale = Vec3f(1.f);

				if(node_node.hasChild("translation"))
				{
					const JSONNode& translation_node = node_node.getChildArray(parser, "translation");
					if(translation_node.child_indices.size() != 3)
						throw glare::Exception("Expected 3 elements in translation array.");
					for(uint32 q=0; q<3; ++q)
						node->translation[q] = (float)parser.nodes[translation_node.child_indices[q]].getDoubleValue();
				}
				else
					node->translation = Vec3f(0.f);

				if(node_node.hasChild("matrix"))
				{
					const JSONNode& matrix_node = node_node.getChildArray(parser, "matrix");
					if(matrix_node.child_indices.size() != 16)
						throw glare::Exception("Expected 16 elements in matrix array.");

					// GLTF stores matrices in column-major order, like Matrix4f.
					for(size_t q=0; q<16; ++q)
						node->matrix.e[q] = (float)parser.nodes[matrix_node.child_indices[q]].getDoubleValue();
				}
				else
					node->matrix = Matrix4f::identity();

				data.nodes.push_back(node);
			}
		}

	// Load animations
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "animations")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);
	
			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& anim_node = parser.nodes[node_array.child_indices[z]];

				Reference<GLTFAnimation> gltf_anim = new GLTFAnimation();

				gltf_anim->name = anim_node.getChildStringValueWithDefaultVal(parser, "name", "");

				// parse channels array
				const JSONNode& channels_array_node = anim_node.getChildArray(parser, "channels");
				for(size_t q=0; q<channels_array_node.child_indices.size(); ++q)
				{
					const JSONNode& channel_node = parser.nodes[channels_array_node.child_indices[q]];

					GLTFChannel channel;
					channel.sampler = channel_node.getChildIntValue(parser, "sampler");

					const JSONNode& target_node = channel_node.getChildObject(parser, "target");

					channel.target_node = target_node.getChildIntValueWithDefaultVal(parser, "node", -1); // NOTE: node is optional.  "When node isn't defined, channel should be ignored"
					if(channel.target_node != -1)
					{
						const std::string target_path = target_node.getChildStringValue(parser, "path");

						if(target_path == "translation")
							channel.target_path = GLTFChannel::Path_translation;
						else if(target_path == "rotation")
							channel.target_path = GLTFChannel::Path_rotation;
						else if(target_path == "scale")
							channel.target_path = GLTFChannel::Path_scale;
						else if(target_path == "weights")
							channel.target_path = GLTFChannel::Path_weights;
						else
							throw glare::Exception("invalid channel target path: " + target_path);

						gltf_anim->channels.push_back(channel);
					}
				}

				// parse samplers
				const JSONNode& samplers_array_node = anim_node.getChildArray(parser, "samplers");
				for(size_t q=0; q<samplers_array_node.child_indices.size(); ++q)
				{
					const JSONNode& sampler_node = parser.nodes[samplers_array_node.child_indices[q]];

					GLTFSampler sampler;
					sampler.input  = (int)sampler_node.getChildUIntValue(parser, "input");
					sampler.output = (int)sampler_node.getChildUIntValue(parser, "output");

					const std::string interpolation = sampler_node.getChildStringValueWithDefaultVal(parser, "interpolation", "LINEAR");

					if(interpolation == "LINEAR")
						sampler.interpolation = GLTFSampler::Interpolation_linear;
					else if(interpolation == "STEP")
						sampler.interpolation = GLTFSampler::Interpolation_step;
					else if(interpolation == "CUBICSPLINE")
						sampler.interpolation = GLTFSampler::Interpolation_cubic_spline;
					else
						throw glare::Exception("invalid interpolation: " + interpolation);

					gltf_anim->samplers.push_back(sampler);
				}

				data.animations.push_back(gltf_anim);
			}
		}

	// Load skins
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "skins")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& skin_node = parser.nodes[node_array.child_indices[z]];

				Reference<GLTFSkin> gltf_skin = new GLTFSkin();

				if(skin_node.hasChild("name")) gltf_skin->name = skin_node.getChildStringValue(parser, "name");

				gltf_skin->inverse_bind_matrices = skin_node.getChildIntValueWithDefaultVal(parser, "inverseBindMatrices", -1); // Optional, default is identity matrices
				gltf_skin->skeleton              = skin_node.getChildIntValueWithDefaultVal(parser, "skeleton",            -1); // Optional.  NOTE: not used currently

				// parse joints array
				const JSONNode& joints_array_node = skin_node.getChildArray(parser, "joints");
				for(size_t q=0; q<joints_array_node.child_indices.size(); ++q)
				{
					const int val = parser.nodes[joints_array_node.child_indices[q]].getIntValue();
					gltf_skin->joints.push_back(val);
				}

				data.skins.push_back(gltf_skin);
			}
		}



	// Load scenes
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "scenes")
		{
			const JSONNode& node_array = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(node_array, JSONNode::Type_Array);

			for(size_t z=0; z<node_array.child_indices.size(); ++z)
			{
				const JSONNode& scene_node = parser.nodes[node_array.child_indices[z]];

				GLTFSceneRef scene = new GLTFScene();

				if(scene_node.hasChild("name")) scene->name = scene_node.getChildStringValue(parser, "name");

				// parse nodes array
				if(scene_node.hasChild("nodes"))
				{
					const JSONNode& nodes_array_node = scene_node.getChildArray(parser, "nodes");
					for(size_t q=0; q<nodes_array_node.child_indices.size(); ++q)
					{
						scene->nodes.push_back(parser.nodes[nodes_array_node.child_indices[q]].getUIntValue());
					}
				}

				data.scenes.push_back(scene);
			}
		}

	// Load 'scene'
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "scene")
			data.scene = parser.nodes[root.name_val_pairs[i].value_node_index].getUIntValue();



	//======================== Process GLTF data ================================

	// Print out buffer views
	/*for(size_t i=0; i<data.buffer_views.size(); ++i)
	{
		conPrint("Buffer view " + toString(i));
		printVar(data.buffer_views[i]->buffer);
		printVar(data.buffer_views[i]->byte_offset);
		printVar(data.buffer_views[i]->byte_length);
		printVar(data.buffer_views[i]->byte_stride);
	}

	// Print out accessors
	for(size_t i=0; i<data.accessors.size(); ++i)
	{
		conPrint("Accessor " + toString(i));
		printVar(data.accessors[i]->buffer_view);
		printVar(data.accessors[i]->byte_offset);
		printVar(data.accessors[i]->count);
	}*/


	// Load all unloaded buffers
	for(size_t i=0; i<data.buffers.size(); ++i)
	{
		if(data.buffers[i]->binary_data == NULL)
		{
			const std::string path = gltf_base_dir + "/" + data.buffers[i]->uri;
			data.buffers[i]->file = new MemMappedFile(path);
			data.buffers[i]->binary_data = (const uint8*)data.buffers[i]->file->fileData();
			data.buffers[i]->data_size = data.buffers[i]->file->fileSize();
		}
	}


	// Process meshes to get the set of attributes used
	for(size_t i=0; i<data.meshes.size(); ++i)
	{
		for(size_t z=0; z<data.meshes[i]->primitives.size(); ++z)
		{
			if(data.meshes[i]->primitives[z]->attributes.count("NORMAL"))
				data.attr_present.normal_present = true;
			if(data.meshes[i]->primitives[z]->attributes.count("COLOR_0"))
				data.attr_present.vert_col_present = true;
			if(data.meshes[i]->primitives[z]->attributes.count("TEXCOORD_0"))
				data.attr_present.texcoord_0_present = true;
			if(data.meshes[i]->primitives[z]->attributes.count("JOINTS_0"))
				data.attr_present.joints_present = true;
			if(data.meshes[i]->primitives[z]->attributes.count("WEIGHTS_0"))
				data.attr_present.weights_present = true;
		}
	}



	// Process scene nodes

	// Get the scene to use
	if(data.scene >= data.scenes.size())
		throw glare::Exception("scene index out of bounds.");
	GLTFScene& scene_node = *data.scenes[data.scene];


	/*std::vector<BufferViewInfo> buffer_view_info(data.buffer_views.size());

	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		processNodeToGetBufferViewInfo(data, root_node, buffer_view_info);
	}*/


	// Process meshes referenced by nodes to get the total number of vertices and triangles, so we can reserve space for them.
	size_t total_num_indices = 0;
	size_t total_num_verts = 0;
	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		processNodeToGetMeshCapacity(data, root_node, total_num_indices, total_num_verts);
	}


	Reference<BatchedMesh> batched_mesh = new BatchedMesh();

	// Build mesh attributes
	batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Position, BatchedMesh::ComponentType_Float, /*offset_B=*/0));

	if(data.attr_present.normal_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Normal, BatchedMesh::ComponentType_PackedNormal, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.vert_col_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Colour, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.texcoord_0_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_UV_0, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.joints_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Joints, BatchedMesh::ComponentType_UInt16, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.weights_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Weights, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));


	batched_mesh->vertex_data.resize(batched_mesh->vertexSize() * total_num_verts);

	// Reserve the space in the Indigo Mesh
	//mesh.triangles.reserve(total_num_tris);
	//mesh.vert_positions.reserve(total_num_verts);
	//if(data.attr_present.normal_present)     mesh.vert_normals.reserve(total_num_verts);
	//if(data.attr_present.vert_col_present)   mesh.vert_colours.reserve(total_num_verts);
	//if(data.attr_present.texcoord_0_present) mesh.uv_pairs.reserve(total_num_verts);

	js::Vector<uint32, 16> uint32_indices;//(total_num_indices);
	uint32_indices.reserve(total_num_indices);
	size_t vert_write_i = 0;


	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		Matrix4f current_transform = Matrix4f::identity();

		processNode(data, root_node, current_transform, *batched_mesh, uint32_indices, vert_write_i);
	}

	assert(uint32_indices.size() == total_num_indices);
	assert(vert_write_i == total_num_verts);


	batched_mesh->setIndexDataFromIndices(uint32_indices, total_num_verts);

	// Compute AABB
	// TEMP
	{
		js::AABBox aabb = js::AABBox::emptyAABBox();
		const size_t vert_size = batched_mesh->vertexSize();
		const BatchedMesh::VertAttribute& pos_attr = batched_mesh->getAttribute(BatchedMesh::VertAttribute_Position);
		doRuntimeCheck(pos_attr.component_type == BatchedMesh::ComponentType_Float);
		for(int v=0; v<total_num_verts; ++v)
		{
			Vec4f vert_pos = Vec4f(
				((float*)&batched_mesh->vertex_data[v * vert_size + pos_attr.offset_B])[0],
				((float*)&batched_mesh->vertex_data[v * vert_size + pos_attr.offset_B])[1],
				((float*)&batched_mesh->vertex_data[v * vert_size + pos_attr.offset_B])[2],
				1);
			aabb.enlargeToHoldPoint(vert_pos);
		}
		batched_mesh->aabb_os = aabb;
	}



	// Check we reserved the right amount of space.
	//assert(mesh.vert_positions.size()     == total_num_verts);
	//assert(mesh.vert_positions.capacity() == total_num_verts);
	//assert(mesh.vert_normals.size()       == mesh.vert_normals.capacity());
	//assert(mesh.vert_colours.size()       == mesh.vert_colours.capacity());
	//assert(mesh.uv_pairs.size()           == mesh.uv_pairs.capacity());
	//
	//assert(mesh.triangles.size()          == total_num_tris);
	//assert(mesh.triangles.capacity()      == total_num_tris);


	// Process images - for any image embedded in the GLB file data, save onto disk in a temp location
	for(size_t i=0; i<data.images.size(); ++i)
	{
		processImage(data, *data.images[i], gltf_base_dir);
	}

	// Process materials
	data_out.materials.materials.resize(data.materials.size());
	for(size_t i=0; i<data.materials.size(); ++i)
	{
		processMaterial(data, *data.materials[i], gltf_base_dir, data_out.materials.materials[i]);
	}

	//-------------------- Process animations ---------------------------
	// Write out nodes

	// Set parent indices in data_out.nodes
	data_out.anim_data.nodes.resize(data.nodes.size());
	for(size_t i=0; i<data.nodes.size(); ++i)
	{
		data_out.anim_data.nodes[i].parent_index = -1;

		data_out.anim_data.nodes[i].trans = data.nodes[i]->translation.toVec4fVector();
		data_out.anim_data.nodes[i].rot   = data.nodes[i]->rotation;
		data_out.anim_data.nodes[i].scale = data.nodes[i]->scale.toVec4fVector();
	}

	for(size_t i=0; i<data.nodes.size(); ++i)
	{
		GLTFNode& node = *data.nodes[i];

		conPrint("Node " + toString(i));
		for(size_t z=0; z<node.children.size(); ++z)
		{
			const size_t child_index = node.children[z];
			conPrint("\tchild " + toString(child_index));
			data_out.anim_data.nodes[child_index].parent_index = (int)i;
		}
	}
	
	// Build sorted_nodes - Node indices sorted such that children always come after parents.
	// We will do this by doing a depth first traversal over the nodes.
	std::vector<size_t> node_stack;

	// Initialise node_stack with roots
	for(size_t i=0; i<data.nodes.size(); ++i)
		if(data_out.anim_data.nodes[i].parent_index == -1)
			node_stack.push_back(i);

	while(!node_stack.empty())
	{
		const int node_i = (int)node_stack.back();
		node_stack.pop_back();

		data_out.anim_data.sorted_nodes.push_back(node_i);

		// Add child nodes of the current node
		const GLTFNode& node = *data.nodes[node_i];
		for(size_t z=0; z<node.children.size(); ++z)
		{
			const size_t child_index = node.children[z];
			node_stack.push_back(child_index);
		}
	}



	data_out.anim_data.animations.resize(data.animations.size());
	for(size_t i=0; i<data.animations.size(); ++i)
	{
		data_out.anim_data.animations[i] = new AnimationDatum();
		processAnimation(data, *data.animations[i], gltf_base_dir, *data_out.anim_data.animations[i]);
	}


	for(size_t i=0; i<data.skins.size(); ++i)
	{
		processSkin(data, *data.skins[i], gltf_base_dir, data_out.anim_data);
	}

	//mesh.endOfModel();
	return batched_mesh;
}


struct GLTFChunkInfo
{
	GLTFChunkInfo(){}

	uint32 indices_start;
	size_t num_indices;
	uint32 mat_index;
};


static const std::string formatDouble(double x)
{
	std::string s = doubleLiteralString(x);
	if(hasLastChar(s, '.'))
		s += "0";
	return s;
}


/*
NOTE: copied from Escaping.cpp in website_core repo.

JSON RFC:
http://tools.ietf.org/html/rfc7159

"All Unicode characters may be placed within the
quotation marks, except for the characters that must be escaped:
quotation mark, reverse solidus, and the control characters (U+0000
through U+001F)."
*/
static const std::string JSONEscape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '"')
			result += "\\\"";
		else if(s[i] == '\\') // a single backslash needs to be escaped as two backslashes.
			result += "\\\\";
		else if(s[i] >= 0 && s[i] <= 0x1F) // control characters (U+0000 through U+001F).
			result += "\\u" + leftPad(toString(s[i]), '0', 4);
		else
			result.push_back(s[i]);
	}
	return result;
}


void FormatDecoderGLTF::writeToDisk(const Indigo::Mesh& mesh, const std::string& path, const GLTFWriteOptions& options, const GLTFLoadedData& gltf_data)
{
	std::ofstream file(path);

	file <<
		"{\n"
		"\"asset\": {\n"
		"	\"generator\": \"Indigo Renderer\",\n"
		"	\"version\": \"2.0\"\n"
		"},\n"
		"\"nodes\": [\n"
		"	{\n"
		"		\"mesh\": 0\n"
		"	}\n"
		"],\n"
		"\"scenes\": [\n"
		"	{\n"
		"		\"name\": \"singleScene\",\n"
		"			\"nodes\": [\n"
		"				0\n"
		"			]\n"
		"	}\n"
		"],\n"
		"\"scene\": 0,\n";

	

	const size_t tri_index_data_size = mesh.triangles.size() * 3 * sizeof(uint32);

	const bool write_normals = !mesh.vert_normals.empty() && options.write_vert_normals;

	const size_t vert_stride =
		                              sizeof(Indigo::Vec3f) + // vert positions
		(write_normals              ? sizeof(Indigo::Vec3f) : 0) +
		(!mesh.vert_colours.empty() ? sizeof(Indigo::Vec3f) : 0) +
		(!mesh.uv_pairs.empty()     ? sizeof(Indigo::Vec2f) : 0);

	const size_t total_vert_data_size = mesh.vert_positions.size() * vert_stride;


	// Build and then write data binary (.bin) file.
	std::vector<unsigned char> data;
	data.resize(tri_index_data_size + total_vert_data_size);

	for(size_t i=0; i<mesh.triangles.size(); ++i)
		std::memcpy(data.data() + sizeof(uint32) * 3 * i, mesh.triangles[i].vertex_indices, sizeof(uint32) * 3); // Copy vert indices for tri.

	for(size_t i=0; i<mesh.vert_positions.size(); ++i)
	{
		float* write_pos = (float*)(data.data() + tri_index_data_size + vert_stride * i);
		std::memcpy(write_pos, &mesh.vert_positions[i], sizeof(Indigo::Vec3f)); // Copy vert position
		write_pos += 3;

		if(write_normals)
		{
			std::memcpy(write_pos, &mesh.vert_normals[i], sizeof(Indigo::Vec3f)); // Copy vert normal
			write_pos += 3;
		}

		if(!mesh.vert_colours.empty())
		{
			std::memcpy(write_pos, &mesh.vert_colours[i], sizeof(Indigo::Vec3f)); // Copy vert colour
			write_pos += 3;
		}

		if(!mesh.uv_pairs.empty())
		{
			std::memcpy(write_pos, &mesh.uv_pairs[i], sizeof(Indigo::Vec2f)); // Copy vert UV
			write_pos += 2;
		}
	}

	const std::string bin_filename = ::eatExtension(FileUtils::getFilename(path)) + "bin";
	const std::string bin_path = ::eatExtension(path) + "bin";
	FileUtils::writeEntireFile(bin_path, data);

	// Write buffer element
	// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#binary-data-storage
	file << 
	"\"buffers\": [\n"
	"	{\n"
	"		\"byteLength\": " + toString(data.size()) + ",\n"
	"		\"uri\": \"" + bin_filename + "\"\n"
	"	}\n"
	"],\n";

	
	// Assume all triangles are sorted by material index currently.
	// Build a temp PrimitiveChunk array
	std::vector<GLTFChunkInfo> chunks;
	uint32 last_mat_index = std::numeric_limits<uint32>::max();
	size_t last_chunk_start = 0; // index into triangles
	for(size_t i=0; i<mesh.triangles.size(); ++i)
	{
		if(mesh.triangles[i].tri_mat_index != last_mat_index)
		{
			if(i > last_chunk_start)
			{
				chunks.push_back(GLTFChunkInfo());
				chunks.back().indices_start = (int)last_chunk_start * 3;
				chunks.back().num_indices = (i - last_chunk_start) * 3;
				chunks.back().mat_index = mesh.triangles[last_chunk_start].tri_mat_index;
			}
			last_chunk_start = i;
			last_mat_index = mesh.triangles[i].tri_mat_index;
		}
	}
	if(last_chunk_start < mesh.triangles.size())
	{
		chunks.push_back(GLTFChunkInfo());
		chunks.back().indices_start = (int)last_chunk_start * 3;
		chunks.back().num_indices = (mesh.triangles.size() - last_chunk_start) * 3;
		chunks.back().mat_index = mesh.triangles[last_chunk_start].tri_mat_index;
	}

	// Write bufferViews
	file << "\"bufferViews\": [\n";

	file << 
	"	{\n" // Buffer view for triangle vert indices (for all chunks)
	"		\"buffer\": 0,\n"
	"		\"byteLength\": " + toString(tri_index_data_size) + ",\n"
	"		\"byteOffset\": 0,\n"
	"		\"target\": 34963\n"
	"	},\n";
	

	// Write buffer views - 1 for each chunk.  Not stricly neccessary buy needed for how the loader works currently, to avoid duplicated verts.
	//for(size_t i=0; i<chunks.size(); ++i)
	//{
	//	file <<
	//		"	{\n" // Buffer view for vertex data
	//		"		\"buffer\": 0,\n"
	//		"		\"byteLength\": " + toString(total_vert_data_size) + ",\n"
	//		"		\"byteOffset\": " + toString(tri_index_data_size) + ",\n"
	//		"		\"byteStride\": " + toString(vert_stride) + ",\n"
	//		"		\"target\": 34962\n"
	//		"	}\n"
	//		"],\n";
	//}


	file << 
	"	{\n" // Buffer view for vertex data
	"		\"buffer\": 0,\n"
	"		\"byteLength\": " + toString(total_vert_data_size) + ",\n"
	"		\"byteOffset\": " + toString(tri_index_data_size) + ",\n"
	"		\"byteStride\": " + toString(vert_stride) + ",\n"
	"		\"target\": 34962\n"
	"	}\n"
	"],\n";
	
	// Write accessors
	file << "\"accessors\": [\n";

	for(size_t i=0; i<chunks.size(); ++i)
	{
		// Make accessors for chunk indices
		file << 
		"	" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
		"		\"bufferView\": 0,\n" // buffer view 0 is the index buffer view
		"		\"byteOffset\": " + toString(chunks[i].indices_start * sizeof(uint32)) + ",\n"
		"		\"componentType\": 5125,\n" // 5125 = UNSIGNED_INT
		"		\"count\": " + toString(chunks[i].num_indices) + ",\n"
		"		\"type\": \"SCALAR\"\n"
		"	}\n";
	}

	int next_accessor = (int)chunks.size();

	// Write accessor for vertex position
	const int position_accessor = next_accessor++;
	file << 
	"	,{\n"
	"		\"bufferView\": 1,\n"
	"		\"byteOffset\": 0,\n"
	"		\"componentType\": 5126,\n" // 5126 = GLTF_COMPONENT_TYPE_FLOAT
	"		\"count\": " + toString(mesh.vert_positions.size()) + ",\n"
	"		\"max\": [\n"
	"			" + formatDouble(mesh.aabb_os.bound[1].x) + ",\n"
	"			" + formatDouble(mesh.aabb_os.bound[1].y) + ",\n"
	"			" + formatDouble(mesh.aabb_os.bound[1].z) + "\n"
	"		],\n"
	"		\"min\": [\n"
	"			" + formatDouble(mesh.aabb_os.bound[0].x) + ",\n"
	"			" + formatDouble(mesh.aabb_os.bound[0].y) + ",\n"
	"			" + formatDouble(mesh.aabb_os.bound[0].z) + "\n"
	"		],\n"
	"		\"type\": \"VEC3\"\n"
	"	}\n";

		
	size_t byte_offset = sizeof(Indigo::Vec3f);
	int vert_normal_accessor = -1;
	if(write_normals)
	{
		//  Write accessor for vertex normals
		file << 
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(byte_offset) + ",\n"
		"		\"componentType\": 5126,\n" // 5126 = GLTF_COMPONENT_TYPE_FLOAT
		"		\"count\": " + toString(mesh.vert_normals.size()) + ",\n"
		"		\"type\": \"VEC3\"\n"
		"	}\n";

		vert_normal_accessor = next_accessor++;
		byte_offset += sizeof(Indigo::Vec3f);
	}

	int vert_colour_accessor = -1;
	if(!mesh.vert_colours.empty())
	{
		//  Write accessor for vertex colours
		file <<
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(byte_offset) + ",\n"
		"		\"componentType\": 5126,\n" // 5126 = GLTF_COMPONENT_TYPE_FLOAT
		"		\"count\": " + toString(mesh.vert_colours.size()) + ",\n"
		"		\"type\": \"VEC3\"\n"
		"	}\n";
		vert_colour_accessor = next_accessor++;
		byte_offset += sizeof(Indigo::Vec3f);
	}

	int uv_accessor = -1;
	if(!mesh.uv_pairs.empty())
	{
		//  Write accessor for vertex UVs
		file << 
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(byte_offset) + ",\n"
		"		\"componentType\": 5126,\n" // 5126 = GLTF_COMPONENT_TYPE_FLOAT
		"		\"count\": " + toString(mesh.uv_pairs.size()) + ",\n"
		"		\"type\": \"VEC2\"\n"
		"	}\n";
		uv_accessor = next_accessor++;
		byte_offset += sizeof(Indigo::Vec2f);
	}

	// Finish accessors array
	file << "],\n";

	// Write meshes element.
	file << "\"meshes\": [\n"
	"	{\n"
	"		\"primitives\": [\n";

	for(size_t i=0; i<chunks.size(); ++i)
	{
		file << "			" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"				\"attributes\": {\n"
			"					\"POSITION\": " + toString(position_accessor);

		if(vert_normal_accessor >= 0)
			file << ",\n					\"NORMAL\": " + toString(vert_normal_accessor);

		if(vert_colour_accessor >= 0)
			file << ",\n					\"COLOR_0\": " + toString(vert_colour_accessor);

		if(uv_accessor >= 0)
			file << ",\n					\"TEXCOORD_0\": " + toString(uv_accessor);

		file << "\n";

		file <<
		"				},\n"
		"				\"indices\": " + toString(i) + ",\n" // First accessors are the index accessors
		"				\"material\": " + toString(chunks[i].mat_index) + ",\n"
		"				\"mode\": 4\n" // 4 = GLTF_MODE_TRIANGLES
		"			}\n";
	}

	// End primitives and meshes array
	file << 
	"		]\n" // End primitives array
	"	}\n" // End mesh hash
	"],\n"; // End meshes array


	// Write materials
	file << "\"materials\": [\n";

	std::vector<GLTFResultMap> textures;

	for(size_t i=0; i<gltf_data.materials.materials.size(); ++i)
	{
		file <<
			"	" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"		\"pbrMetallicRoughness\": {\n";

		if(!gltf_data.materials.materials[i].diffuse_map.path.empty())
		{
			file <<
				"			\"baseColorTexture\": {\n"
				"				\"index\": " + toString(textures.size()) + "\n"
				"			},\n";

			textures.push_back(gltf_data.materials.materials[i].diffuse_map);
		}

		file <<
			"			\"metallicFactor\": 0.0\n"
			"		},\n" //  end of pbrMetallicRoughness hash
			"		\"emissiveFactor\": [\n"
			"			0.0,\n"
			"			0.0,\n"
			"			0.0\n"
			"		]\n"
			"	}\n"; // end of material hash
	}

	file << "],\n"; // end materials array

	// Write textures
	if(!textures.empty())
	{
		file << "\"textures\": [\n";

		for(size_t i=0; i<textures.size(); ++i)
		{
			file <<  
				"	" + ((i > 0) ? std::string(",") : std::string("")) + "{\n"
				"		\"sampler\": 0,\n"
				"		\"source\": " + toString(i) + "\n"
				"	}\n";
		}
		file << "],\n"; // end textures array

		// Write images
		file << "\"images\": [\n";

		std::string current_dir = FileUtils::getDirectory(path);
		if(current_dir.empty())
			current_dir = ".";

		for(size_t i=0; i<textures.size(); ++i)
		{
			//const std::string relative_path = FileUtils::getRelativePath(current_dir, textures[i].path);
			const std::string use_path = textures[i].path;// FileUtils::getFilename(textures[i].path);

			file << 
				"	" + ((i > 0) ? std::string(",") : std::string("")) + "{\n"
				"		\"uri\": \"" + JSONEscape(use_path) + "\"\n"
				"	}\n";
		}
		file << "],\n"; // end images array
	}

	// Write samplers
	file <<
		"\"samplers\": [\n"
		"	{\n"
		"		\"magFilter\": 9729,\n" // TODO: work out best values for these
		"		\"minFilter\": 9986,\n"
		"		\"wrapS\": 10497,\n"
		"		\"wrapT\": 10497\n"
		"	}\n"
		"]\n";

	file << "}\n";
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"


static void testWriting(const Reference<BatchedMesh>& mesh, const GLTFLoadedData& data)
{
	//try
	//{
	//	const std::string path = PlatformUtils::getTempDirPath() + "/mesh_" + toString(mesh.checksum()) + ".gltf";
	//
	//	// Write it back to disk
	//	GLTFWriteOptions options;
	//	//options.write_vert_normals = false;
	//	//FormatDecoderGLTF::writeToDisk(mesh, path, options, mats);
	//
	//	options.write_vert_normals = true;
	//	FormatDecoderGLTF::writeToDisk(mesh, path, options, data);
	//
	//	// Load again
	//	//Indigo::Mesh mesh2;
	//	GLTFLoadedData data2;
	//	Reference<BatchedMesh> mesh2 FormatDecoderGLTF::loadGLTFFile(path, data2);
	//
	//	// Check num vertices etc.. is the same
	//	testAssert(mesh2.num_materials_referenced	== mesh.num_materials_referenced);
	//	testAssert(mesh2.vert_positions.size()		== mesh.vert_positions.size());
	//	testAssert(mesh2.vert_normals.size()		== mesh.vert_normals.size());
	//	testAssert(mesh2.vert_colours.size()		== mesh.vert_colours.size());
	//	testAssert(mesh2.uv_pairs.size()			== mesh.uv_pairs.size());
	//	testAssert(mesh2.triangles.size()			== mesh.triangles.size());
	//}
	//catch(glare::Exception& e)
	//{
	//	failTest(e.what());
	//}
}


void FormatDecoderGLTF::test()
{
	conPrint("FormatDecoderGLTF::test()");

	try
	{
		{
			// Has vertex colours, also uses unsigned bytes for indices.
			conPrint("---------------------------------VertexColorTest.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/VertexColorTest.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 2);
			testAssert(mesh->numVerts() == 72);
			testAssert(mesh->numIndices() == 36 * 3);

			testWriting(mesh, data);
		}

		{
			conPrint("---------------------------------Box.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/Box.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 24);
			testAssert(mesh->numIndices() == 12 * 3);
			//testAssert(mesh.vert_normals.size() == 24);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 0);

			testWriting(mesh, data);
		}
	
		{
			conPrint("---------------------------------BoxInterleaved.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxInterleaved.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 24);
			testAssert(mesh->numIndices() == 12 * 3);
			//testAssert(mesh.vert_normals.size() == 24);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 0);

			testWriting(mesh, data);
		}

		{
			conPrint("---------------------------------BoxTextured.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxTextured.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 24);
			testAssert(mesh->numIndices() == 12 * 3);
			//testAssert(mesh.vert_normals.size() == 24);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 24);

			testWriting(mesh, data);
		}
	
		{
			conPrint("---------------------------------BoxVertexColors.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxVertexColors.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 24);
			testAssert(mesh->numIndices() == 12 * 3);
			//testAssert(mesh.vert_normals.size() == 24);
			//testAssert(mesh.vert_colours.size() == 24);
			//testAssert(mesh.uv_pairs.size() == 24);

			testWriting(mesh, data);
		}
	
		{
			conPrint("---------------------------------2CylinderEngine.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/2CylinderEngine.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 34);
			testAssert(mesh->numVerts() == 84657);
			testAssert(mesh->numIndices() == 121496 * 3);
			//testAssert(mesh.vert_normals.size() == 84657);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 0);

			testWriting(mesh, data);
		}

		{
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/RiggedFigure.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 370);
			testAssert(mesh->numIndices() == 256 * 3);
			//testAssert(mesh.vert_normals.size() == 370);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 0);

			testWriting(mesh, data);
		}
	
		{
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/CesiumMan.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 3273);
			testAssert(mesh->numIndices() == 4672 * 3);
			//testAssert(mesh.vert_normals.size() == 3273);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 3273);

			testWriting(mesh, data);
		}
	
		{
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/MetalRoughSpheresNoTextures.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 98 + 1); // Seems to use a default material
			testAssert(mesh->numVerts() == 528291);
			testAssert(mesh->numIndices() == 1040409 * 3);
			//testAssert(mesh.vert_normals.size() == 528291);
			//testAssert(mesh.vert_colours.size() == 0);
			//testAssert(mesh.uv_pairs.size() == 0);

			testWriting(mesh, data);
		}

		{
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/RiggedSimple.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 160);
			testAssert(mesh->numIndices() == 188 * 3);

			testWriting(mesh, data);
		}

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	try
	{
		// Read a GLTF file from disk
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/gltf/duck/Duck.gltf";
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLTFFile(path, data);

		testAssert(mesh->numMaterialsReferenced() == 1);
		testAssert(mesh->numVerts() == 2399);
		testAssert(mesh->numIndices() == 4212 * 3);

		//testAssert(mesh.num_materials_referenced == 1);
		//testAssert(mesh.vert_positions.size() == 2399);
		//testAssert(mesh.vert_normals.size() == 2399);
		//testAssert(mesh.vert_colours.size() == 0);
		//testAssert(mesh.uv_pairs.size() == 2399);
		//testAssert(mesh.triangles.size() == 4212);


		/*
		// Write it back to disk
		GLTFWriteOptions options;
		options.write_vert_normals = false;
		writeToDisk(mesh, PlatformUtils::getTempDirPath() + "/duck_new.gltf", options, data);

		options.write_vert_normals = true;
		writeToDisk(mesh, PlatformUtils::getTempDirPath() + "/duck_new.gltf", options, data);


		// Load again
		Indigo::Mesh mesh2;
		GLTFLoadedData data2;
		streamModel(path, mesh2, data2);

		testAssert(mesh2.num_materials_referenced == 1);
		testAssert(mesh2.vert_positions.size() == 2399);
		testAssert(mesh2.vert_normals.size() == 2399);
		testAssert(mesh2.vert_colours.size() == 0);
		testAssert(mesh2.uv_pairs.size() == 2399);
		testAssert(mesh2.triangles.size() == 4212);
		*/
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	conPrint("FormatDecoderGLTF::test() done.");
}


#endif // BUILD_TESTS
