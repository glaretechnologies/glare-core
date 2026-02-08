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
#include "../utils/BufferViewInStream.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/IncludeXXHash.h"
#include "../utils/FileOutStream.h"
#include "../utils/FileInStream.h"
#include "../utils/Base64.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/Timer.h"
#include "../maths/Quat.h"
#include "../graphics/Colour4f.h"
#include "../graphics/BatchedMesh.h"
#include <assert.h>
#include <vector>
#include <map>
#include <fstream>
#include <set>


struct GLTFBuffer : public RefCounted
{
	GLTFBuffer() : file(NULL), binary_data(NULL) {}
	~GLTFBuffer() { delete file; }
	
	MemMappedFile* file;
	const uint8* binary_data;
	size_t data_size;
	std::string uri;
	std::vector<unsigned char> decoded_base64_data;

	std::vector<uint8> copied_data;
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
	std::string name;
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

	GLTFNode() : mesh(std::numeric_limits<size_t>::max()), skin(std::numeric_limits<size_t>::max()) {}

	std::vector<size_t> children;
	std::string name;
	
	Matrix4f matrix;
	Quatf rotation;
	size_t mesh; // Index of mesh if specified, std::numeric_limits<size_t>::max() otherwise.
	size_t skin; // Index of skin if specified, std::numeric_limits<size_t>::max() otherwise.
	Vec3f scale;
	Vec3f translation;
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

	GLTFMaterial() : KHR_materials_pbrSpecularGlossiness_present(false), doubleSided(false) {}

	std::string name;
	std::string alphaMode;

	bool doubleSided;

	//----------- From KHR_materials_pbrSpecularGlossiness extension:------------
	bool KHR_materials_pbrSpecularGlossiness_present;
	Colour4f diffuseFactor;
	GLTFTextureObject diffuseTexture;
	Colour3f specularFactor;
	float glossinessFactor;
	GLTFTextureObject specularGlossinessTexture;
	//----------- End from KHR_materials_pbrSpecularGlossiness extension:------------

	//----------- From pbrMetallicRoughness:------------
	Colour4f baseColorFactor;
	GLTFTextureObject baseColorTexture;
	GLTFTextureObject metallicRoughnessTexture;
	float metallicFactor;
	float roughnessFactor;
	//----------- End from pbrMetallicRoughness:------------

	GLTFTextureObject emissiveTexture;
	Colour3f emissiveFactor;

	GLTFTextureObject normalTexture;
};
typedef Reference<GLTFMaterial> GLTFMaterialRef;


// Attributes present on any of the meshes.
struct AttributesPresent
{
	AttributesPresent() : normal_present(false), vert_col_present(false), texcoord_0_present(false), joints_present(false), weights_present(false), tangent_present(false) {}

	bool normal_present;
	bool vert_col_present;
	bool texcoord_0_present;
	bool joints_present;
	bool weights_present;
	bool tangent_present;
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
	GLTFVRMExtensionRef vrm_data;
	size_t scene;

	AttributesPresent attr_present; // Attributes present on any of the meshes.
};


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
	if(image_index >= data.images.size())
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


static void processNodeToGetMeshCapacity(GLTFData& data, GLTFNode& node, int depth, size_t& total_num_indices, size_t& total_num_verts)
{
	// Check depth to ensure we don't get stuck in an infinite loop if there are cycles in the children references.
	if(depth > 256)
		throw glare::Exception("Exceeded max depth while computing mesh capacity");

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

		processNodeToGetMeshCapacity(data, child, depth + 1, total_num_indices, total_num_verts);
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
static void copyData(size_t accessor_count, const uint8* const offset_base, const size_t byte_stride, glare::AllocatorVector<uint8, 16>& vertex_data, const size_t vert_write_i, 
	const size_t dest_vert_stride_B, const size_t dest_attr_offset_B, const DestType scale)
{
	// Bounds check destination addresses (should be done already, but check again)
	if(dest_attr_offset_B + sizeof(DestType) * N > dest_vert_stride_B)
		throw glare::Exception("Internal error: destination buffer overflow");

	if((vert_write_i + accessor_count) * dest_vert_stride_B > vertex_data.size())
		throw glare::Exception("Internal error: destination buffer overflow");

	static_assert(sizeof(DestType) <= 4, "sizeof(DestType) <= 4");
	runtimeCheck(dest_vert_stride_B % 4 == 0); // Check alignment
	
	uint8* const offset_dest = vertex_data.data() + vert_write_i * dest_vert_stride_B + dest_attr_offset_B;

	runtimeCheck((uint64)offset_dest % 4 == 0); // Check alignment

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

	if((int)typeNumComponents(accessor.type) != expected_num_components)
		throw glare::Exception("Invalid num components (type) for accessor.");
}


// uint32_indices_out should have sufficient capacity for all indices, but does need to be resized for new indices.
// mesh_out.vertex_data should have sufficient size for all vertex data (has already been resized)
static void processNode(GLTFData& data, GLTFNode& node, size_t node_index, const Matrix4f& parent_transform, BatchedMesh& mesh_out, js::Vector<uint32, 16>& uint32_indices_out, size_t& vert_write_i)
{
	const bool statically_apply_transform = data.skins.empty();

	const Matrix4f trans = Matrix4f::translationMatrix(node.translation.x, node.translation.y, node.translation.z);
	const Matrix4f rot = normalise(node.rotation).toMatrix();
	const Matrix4f scale = Matrix4f::scaleMatrix(node.scale.x, node.scale.y, node.scale.z);
	const Matrix4f node_transform = parent_transform * node.matrix * trans * rot * scale; // Matrix and T,R,S transforms should be mutually exclusive in GLTF files.  Just multiply them together however.

	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		GLTFMesh& mesh = getMesh(data, node.mesh);

		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTFPrimitive& primitive = *mesh.primitives[i];

			if(!shouldLoadPrimitive(primitive))
				continue;


			uint16 use_joint_index = 0;
			if(!data.skins.empty() && (node.skin == std::numeric_limits<size_t>::max())) // If we have a skin, and if this is a mesh node that doesn't use a skin, then this node uses rigid-body animation.
			{
				// Use the skin-based animation system for it - add this node to the list of joints.
				use_joint_index = (uint16)data.skins.back()->joints.size();
				data.skins.back()->joints.push_back((int)node_index);
			}

			
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

				checkAccessorBounds(byte_stride, offset_B, value_size_B, index_accessor, buffer, /*expected_num_components=*/1);

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
			//Timer timer;
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

				runtimeCheck(pos_attr.component_type == BatchedMesh::ComponentType_Float); // We store positions in float format.

				// POSITION attribute must have FLOAT component types (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview)
				checkProperty(pos_accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT, "Invalid POSITION component type");
				
				// Check alignment before we start reading and writing floats
				// The source alignments should be valid for valid GLTF files.
				checkProperty((uint64)(offset_base) % 4 == 0, "source offset_base not multiple of 4");
				checkProperty(byte_stride % 4 == 0, "source byte stride not multiple of 4");
				// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
				runtimeCheck(dest_vert_stride_B % 4 == 0);
				runtimeCheck(dest_attr_offset_B % 4 == 0);

				for(size_t z=0; z<pos_accessor.count; ++z)
				{
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
			//conPrint("Process vertex positions: " + timer.elapsedString());

			//--------------------------------------- Process vertex normals ---------------------------------------
			if(primitive.attributes.count("NORMAL"))
			{
				Matrix4f normal_transform;
				if(statically_apply_transform)
					node_transform.getUpperLeftInverseTranspose(normal_transform);
				else
					normal_transform = Matrix4f::identity();

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

				runtimeCheck(normals_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store normals in packed format.

				// NORMAL attribute must have FLOAT component types (https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview)
				checkProperty(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT, "Invalid POSITION component type");

				// Check alignment before we start reading and writing floats
				// The source alignments should be valid for valid GLTF files.
				checkProperty((uint64)(offset_base) % 4 == 0, "source offset_base not multiple of 4");
				checkProperty(byte_stride % 4 == 0, "source byte stride not multiple of 4");
				// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
				runtimeCheck(dest_vert_stride_B % 4 == 0);
				runtimeCheck(dest_attr_offset_B % 4 == 0);

				for(size_t z=0; z<accessor.count; ++z)
				{
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
			{
				if(data.attr_present.normal_present)
				{
					// Pad with normals.  This is a hack, needed because we only have one vertex layout per mesh.

					const BatchedMesh::VertAttribute& normals_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Normal);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = normals_attr.offset_B;

					// Check alignment before we start reading and writing
					// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
					runtimeCheck(dest_vert_stride_B % 4 == 0);
					runtimeCheck(dest_attr_offset_B % 4 == 0);
					runtimeCheck(pos_attr.offset_B % 4 == 0);

					// Initialise with a default normal value
					runtimeCheck(normals_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store normals in packed format.
					for(size_t z=0; z<vert_pos_count; ++z)
					{
						const Vec4f n_ws(0,0,1,0);
						const uint32 packed = batchedMeshPackNormal(n_ws);
						*(uint32*)(&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B]) = packed;
					}

					const size_t total_num_verts = mesh_out.vertex_data.size() / dest_vert_stride_B;

					for(size_t z=0; z<primitive_num_indices/3; z++) // For each tri, splat geometric normal to the vert normal for each vert
					{
						const uint32 v0i = uint32_indices_out[indices_write_i + z * 3 + 0];
						const uint32 v1i = uint32_indices_out[indices_write_i + z * 3 + 1];
						const uint32 v2i = uint32_indices_out[indices_write_i + z * 3 + 2];

						// Vert indices are not checked yet (are still user-controlled) so check before we use them.
						checkProperty(v0i < total_num_verts, "vert index out of bounds");
						checkProperty(v1i < total_num_verts, "vert index out of bounds");
						checkProperty(v2i < total_num_verts, "vert index out of bounds");

						const size_t v0_offset_B = v0i * dest_vert_stride_B + pos_attr.offset_B;
						const size_t v1_offset_B = v1i * dest_vert_stride_B + pos_attr.offset_B;
						const size_t v2_offset_B = v2i * dest_vert_stride_B + pos_attr.offset_B;

						// This should be redundant, but check anwyay:
						checkProperty(v0_offset_B + sizeof(float)*3 <= mesh_out.vertex_data.size(), "vert index out of bounds");
						checkProperty(v1_offset_B + sizeof(float)*3 <= mesh_out.vertex_data.size(), "vert index out of bounds");
						checkProperty(v2_offset_B + sizeof(float)*3 <= mesh_out.vertex_data.size(), "vert index out of bounds");

						const Vec4f v0(
							((float*)&mesh_out.vertex_data[v0_offset_B])[0],
							((float*)&mesh_out.vertex_data[v0_offset_B])[1],
							((float*)&mesh_out.vertex_data[v0_offset_B])[2],
							1);

						const Vec4f v1(
							((float*)&mesh_out.vertex_data[v1_offset_B])[0],
							((float*)&mesh_out.vertex_data[v1_offset_B])[1],
							((float*)&mesh_out.vertex_data[v1_offset_B])[2],
							1);

						const Vec4f v2(
							((float*)&mesh_out.vertex_data[v2_offset_B])[0],
							((float*)&mesh_out.vertex_data[v2_offset_B])[1],
							((float*)&mesh_out.vertex_data[v2_offset_B])[2],
							1);

						const Vec4f normal = normalise(crossProduct(v1 - v0, v2 - v0));
						const uint32 packed = batchedMeshPackNormal(normal);
						*(uint32*)(&mesh_out.vertex_data[v0i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
						*(uint32*)(&mesh_out.vertex_data[v1i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
						*(uint32*)(&mesh_out.vertex_data[v2i * dest_vert_stride_B + dest_attr_offset_B]) = packed;
					}
				}
			}

			//--------------------------------------- Process vertex tangents ---------------------------------------
			if(primitive.attributes.count("TANGENT"))
			{
				Matrix4f normal_transform;
				if(statically_apply_transform)
					node_transform.getUpperLeftInverseTranspose(normal_transform);
				else
					normal_transform = Matrix4f::identity();

				const BatchedMesh::VertAttribute& tangents_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Tangent);

				const GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "TANGENT");
				const GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t value_size_B = componentTypeByteSize(accessor.component_type) * typeNumComponents(accessor.type);
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

				checkAccessorBounds(byte_stride, offset_B, value_size_B, accessor, buffer, /*expected_num_components=*/4);

				if(accessor.count != vert_pos_count) throw glare::Exception("invalid accessor.count");

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = tangents_attr.offset_B;

				runtimeCheck(tangents_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store normals in packed format.

				// Check alignment before we start reading and writing floats
				// The source alignments should be valid for valid GLTF files.
				checkProperty((uint64)(offset_base) % 4 == 0, "source offset_base not multiple of 4");
				checkProperty(byte_stride % 4 == 0, "source byte stride not multiple of 4");
				// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
				runtimeCheck(dest_vert_stride_B % 4 == 0);
				runtimeCheck(dest_attr_offset_B % 4 == 0);

				// TANGENT must be FLOAT
				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<accessor.count; ++z)
					{
						const Vec4f tangent_os(
							((const float*)(offset_base + byte_stride * z))[0],
							((const float*)(offset_base + byte_stride * z))[1],
							((const float*)(offset_base + byte_stride * z))[2],
							((const float*)(offset_base + byte_stride * z))[3]
						);

						Vec4f tangent_ws = normalise(normal_transform * maskWToZero(tangent_os));
						
						tangent_ws[3] = tangent_os[3]; // Copy w component (sign)

						const uint32 packed_tangent = batchedMeshPackNormalWithW(tangent_ws);
						
						*(uint32*)(&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B]) = packed_tangent;
					}
				}
				else
					throw glare::Exception("Invalid TANGENT component type");
			}
			else
			{
				if(data.attr_present.tangent_present)
				{
					// Pad with tangents.  This is a hack, needed because we only have one vertex layout per mesh.

					const BatchedMesh::VertAttribute& tangents_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Tangent);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = tangents_attr.offset_B;

					// Initialise with a default tangent value
					runtimeCheck(tangents_attr.component_type == BatchedMesh::ComponentType_PackedNormal); // We store tangents in packed format.
					for(size_t z=0; z<vert_pos_count; ++z)
					{
						const Vec4f tangent_ws(0,0,1,1);
						const uint32 packed_tangent = batchedMeshPackNormalWithW(tangent_ws);
						std::memcpy(&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B], &packed_tangent, sizeof(uint32));
					}

					// TODO: compute proper tangents
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

				// Currently BatchedMesh only supports 3-vector colours, so just copy first 3 components in the RGBA attribute case.

				const size_t dest_vert_stride_B = mesh_out.vertexSize();
				const size_t dest_attr_offset_B = colour_attr.offset_B;

				runtimeCheck(colour_attr.component_type == BatchedMesh::ComponentType_Float); // We store colours in float format for now.

				// COLOR_0 must be FLOAT, UNSIGNED_BYTE, or UNSIGNED_SHORT.
				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					copyData<float, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					copyData<uint8, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 255);
				}
				else if(accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					copyData<uint16, float, 3>(accessor.count, offset_base, byte_stride, mesh_out.vertex_data, vert_write_i, dest_vert_stride_B, dest_attr_offset_B, /*scale=*/1.f / 65535);
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

					runtimeCheck(colour_attr.component_type == BatchedMesh::ComponentType_Float); // We store colours in float format for now.

					runtimeCheck((vert_write_i + (vert_pos_count - 1)) * dest_vert_stride_B + dest_attr_offset_B + sizeof(float)*3 <= mesh_out.vertex_data.size());

					// Check alignment before we start reading and writing floats
					// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
					runtimeCheck(dest_vert_stride_B % 4 == 0);
					runtimeCheck(dest_attr_offset_B % 4 == 0);

					for(size_t z=0; z<vert_pos_count; ++z)
					{
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[0] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[1] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[2] = 1.f;
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

				runtimeCheck(texcoord_0_attr.component_type == BatchedMesh::ComponentType_Float); // We store texcoords in float format for now.

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

					runtimeCheck(texcoord_0_attr.component_type == BatchedMesh::ComponentType_Float); // We store uvs in float format for now.
					// Check alignment before we start reading and writing floats
					// Destination alignments should be valid since dest_vert_stride_B should be a multiple of 4 due to the attribute types we choose, and vertex_data is 16-byte aligned.
					runtimeCheck(dest_vert_stride_B % 4 == 0);
					runtimeCheck(dest_attr_offset_B % 4 == 0);

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

				runtimeCheck(joint_attr.component_type == BatchedMesh::ComponentType_UInt16); // We will always store uint16 joint indices for now.

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
					// Pad with use_joint_index
					const BatchedMesh::VertAttribute& joint_attr = mesh_out.getAttribute(BatchedMesh::VertAttribute_Joints);
					const size_t dest_vert_stride_B = mesh_out.vertexSize();
					const size_t dest_attr_offset_B = joint_attr.offset_B;

					// Check alignment
					runtimeCheck(dest_vert_stride_B % 2 == 0);
					runtimeCheck(dest_attr_offset_B % 2 == 0);

					runtimeCheck(joint_attr.component_type == BatchedMesh::ComponentType_UInt16); // We will always store uint16 joint indices for now.
					for(size_t z=0; z<vert_pos_count; ++z)
						for(int c=0; c<4; ++c)
							((uint16*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[c] = use_joint_index;
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

				runtimeCheck(weights_attr.component_type == BatchedMesh::ComponentType_Float); // We store weights as floats

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

					runtimeCheck(weights_attr.component_type == BatchedMesh::ComponentType_Float); // We store uvs in float format for now.
					// Check alignment
					runtimeCheck(dest_vert_stride_B % 4 == 0);
					runtimeCheck(dest_attr_offset_B % 4 == 0);

					for(size_t z=0; z<vert_pos_count; ++z)
					{
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[0] = 1.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[1] = 0.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[2] = 0.f;
						((float*)&mesh_out.vertex_data[(vert_write_i + z) * dest_vert_stride_B + dest_attr_offset_B])[3] = 0.f;
					}
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

		processNode(data, child, node.children[i], node_transform, mesh_out, uint32_indices_out, vert_write_i);
	}
}


static std::string sanitiseString(const std::string& s)
{
	std::string res = s;
	for(size_t i=0; i<s.size(); ++i)
		if(!::isAlphaNumeric(s[i]))
			res[i] = '_';
	return res;
}


// Returns path
static std::string saveImageForMimeType(const std::string& image_name, const std::string& mime_type, const uint8* data, size_t data_size, bool write_images_to_disk)
{
	// Work out extension to use - see https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
	std::string extension;
	if(mime_type == "image/bmp")
		extension = "bmp";
	else if(mime_type == "image/gif")
		extension = "gif";
	else if(mime_type == "image/jpeg")
		extension = "jpg";
	else if(mime_type == "image/png")
		extension = "png";
	else if(mime_type == "image/svg+xml")
		extension = "svg";
	else if(mime_type == "image/tiff")
		extension = "tif";
	else if(mime_type == "image/webp")
		extension = "webp";
	else
		throw glare::Exception("Unknown MIME type for image.");

	// Compute a hash over the data to get a semi-unique filename.
	const uint64 hash = XXH64(data, data_size, /*seed=*/1);

	// Use the sanitised image.name if it is non-empty, otherwise use "GLB_image".  Then append a hash of the file data.
	std::string base_name;
	if(!image_name.empty())
		base_name = sanitiseString(image_name);
	else
		base_name = "GLB_image";

	const std::string path = PlatformUtils::getTempDirPath() + "/" + base_name + "_" + toString(hash) + "." + extension;

	try
	{
		if(write_images_to_disk)
			FileUtils::writeEntireFile(path, (const char*)data, data_size);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw glare::Exception("Error while writing temp image file: " + e.what());
	}

	return path;
}


static void processImage(GLTFData& data, GLTFImage& image, const std::string& gltf_folder, bool write_images_to_disk)
{
	if(image.uri.empty())
	{
		// If the Image URI is empty, this image refers to the data in a GLB file.  Save it to disk separately so our image loaders can load it.
		
		GLTFBufferView& buffer_view = getBufferView(data, image.buffer_view);
		GLTFBuffer& buffer = getBuffer(data, buffer_view.buffer);

		// Check length
		if(buffer_view.byte_length == 0)
			throw glare::Exception("Image buffer view too small.");
		if(buffer_view.byte_length > 1000000000ull)
			throw glare::Exception("Image byte_length too large.");
		if(buffer_view.byte_offset >= buffer.data_size)
			throw glare::Exception("Invalid byte_offset.");
		if(buffer_view.byte_offset + buffer_view.byte_length > buffer.data_size) // NOTE: have to be careful handling unsigned wraparound here
			throw glare::Exception("Image buffer view too large.");

		std::string use_name;
		if(!image.name.empty())
			use_name = image.name;
		else
			use_name = removeDotAndExtension(buffer_view.name); // Texture filenames seem to be stored in the buffer views sometimes.

		const std::string path = saveImageForMimeType(use_name, image.mime_type, (const uint8*)buffer.binary_data + buffer_view.byte_offset, buffer_view.byte_length, write_images_to_disk);

		image.uri = path; // Update GLTF image to use URI on disk
	}
	else // else if !image.uri.empty():
	{
		// If image.uri is non-empty, it either refers to an external file, or is a data URI. (base 64 encoded data)
		
		// Try parsing as a data URI, e.g. "uri":"data:image/png;base64,iVBORw0KGgoAAAANSU...
		Parser parser(image.uri);
		if(parser.parseCString("data:"))  // If this is a data URI (see https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs)
		{
			// parse mime type
			string_view mime_type;
			if(!parser.parseToChar(';', mime_type))
				throw glare::Exception("Failed to parse mime type from image URI");

			// Parse "base64,"
			if(!parser.parseCString(";base64,"))
				throw glare::Exception("Failed to parse base64 encoding type string from image URI");

			const std::string data_base64(image.uri.begin() + parser.currentPos(), image.uri.end());
			std::vector<unsigned char> decoded_data;
			Base64::decode(data_base64, /*data out=*/decoded_data);

			const std::string path = saveImageForMimeType(image.name, toString(mime_type), decoded_data.data(), decoded_data.size(), write_images_to_disk);
			
			image.uri = path; // Update GLTF image to use URI on disk
		}
	}
}


static void processMaterial(GLTFData& data, GLTFMaterial& mat, const std::string& gltf_folder, GLTFResultMaterial& mat_out)
{
	mat_out.colour_factor = Colour3f(mat.baseColorFactor.x[0], mat.baseColorFactor.x[1], mat.baseColorFactor.x[2]);
	mat_out.alpha = mat.baseColorFactor[3];
	if(mat.baseColorTexture.valid())
	{
		GLTFTexture& texture = getTexture(data, mat.baseColorTexture.index);
		GLTFImage& image = getImage(data, texture.source);
		const std::string path = FileUtils::join(gltf_folder, image.uri);
		mat_out.diffuse_map.path = path;
	}
	mat_out.roughness = mat.roughnessFactor;
	mat_out.metallic = mat.metallicFactor;
	if(mat.metallicRoughnessTexture.valid())
	{
		GLTFTexture& texture = getTexture(data, mat.metallicRoughnessTexture.index);
		GLTFImage& image = getImage(data, texture.source);
		const std::string path = FileUtils::join(gltf_folder, image.uri);
		mat_out.metallic_roughness_map.path = path;
	}

	if(mat.KHR_materials_pbrSpecularGlossiness_present)
	{
		mat_out.colour_factor = Colour3f(mat.diffuseFactor.x[0], mat.diffuseFactor.x[1], mat.diffuseFactor.x[2]);
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

	mat_out.emissive_factor = mat.emissiveFactor;
	if(mat.emissiveTexture.valid())
	{
		GLTFTexture& texture = getTexture(data, mat.emissiveTexture.index);
		GLTFImage& image = getImage(data, texture.source);
		const std::string path = FileUtils::join(gltf_folder, image.uri);
		mat_out.emissive_map.path = path;
	}

	if(mat.normalTexture.valid())
	{
		GLTFTexture& texture = getTexture(data, mat.normalTexture.index);
		GLTFImage& image = getImage(data, texture.source);
		const std::string path = FileUtils::join(gltf_folder, image.uri);
		mat_out.normal_map.path = path;
	}
	
	if(mat.alphaMode == "OPAQUE")
		mat_out.alpha = 1; // "The alpha value is ignored and the rendered output is fully opaque."

	mat_out.double_sided = mat.doubleSided;
}


static void processAnimation(GLTFData& data, const GLTFAnimation& anim, const std::string& gltf_folder, const std::vector<int>& new_input_index, const std::vector<int>& new_output_index, 
	AnimationData& anim_data_out, AnimationDatum& anim_datum_out)
{
	// conPrint("Processing anim " + anim.name + "...");

	anim_datum_out.name = anim.name;

	//--------------------------- Set default translations, scales, rotations with the non-animated static values. ---------------------------

	anim_datum_out.raw_per_anim_node_data.resize(data.nodes.size());
	for(size_t n=0; n<data.nodes.size(); ++n)
		anim_datum_out.raw_per_anim_node_data[n].init();
	

	for(size_t c=0; c<anim.channels.size(); ++c)
	{
		const GLTFChannel& channel = anim.channels[c];
		const GLTFSampler& sampler = anim.samplers[channel.sampler]; // Has been bound-checked earlier

		checkProperty(sampler.interpolation == GLTFSampler::Interpolation_linear || sampler.interpolation == GLTFSampler::Interpolation_step, "Only linear and step interpolation types supported currently for animations.");

		const GLTFAccessor& input_accessor  = getAccessor(data, sampler.input);
		const GLTFAccessor& output_accessor = getAccessor(data, sampler.output);

		// For linear and step interpolation types, we have this constraint:
		// "The number output of elements must equal the number of input elements."
		// For cubic-spline there are tangents to handle as well.
		checkProperty(input_accessor.count == output_accessor.count, "Animation: The number output of elements must equal the number of input elements.");

		// channel.target_node is user-controlled and not checked yet.
		checkProperty(channel.target_node >= 0 && channel.target_node < (int)anim_datum_out.raw_per_anim_node_data.size(), "channel.target_node is invalid.");

		if(channel.target_path == GLTFChannel::Path_translation)
		{
			anim_datum_out.raw_per_anim_node_data[channel.target_node].translation_input_accessor  = new_input_index [sampler.input];
			anim_datum_out.raw_per_anim_node_data[channel.target_node].translation_output_accessor = new_output_index[sampler.output];

			checkProperty(typeNumComponents(output_accessor.type) == 3, "invalid number of components for animated translation");
		}
		else if(channel.target_path == GLTFChannel::Path_rotation)
		{
			anim_datum_out.raw_per_anim_node_data[channel.target_node].rotation_input_accessor  = new_input_index [sampler.input];
			anim_datum_out.raw_per_anim_node_data[channel.target_node].rotation_output_accessor = new_output_index[sampler.output];

			checkProperty(typeNumComponents(output_accessor.type) == 4, "invalid number of components for animated rotation");
		}
		else if(channel.target_path == GLTFChannel::Path_scale)
		{
			anim_datum_out.raw_per_anim_node_data[channel.target_node].scale_input_accessor  = new_input_index [sampler.input];
			anim_datum_out.raw_per_anim_node_data[channel.target_node].scale_output_accessor = new_output_index[sampler.output];

			checkProperty(typeNumComponents(output_accessor.type) == 3, "invalid number of components for animated scale");
		}
	}

	// conPrint("done.");
}


static void processSkin(GLTFData& data, const GLTFSkin& skin, const std::string& gltf_folder, AnimationData& anim_data)
{
	// conPrint("Processing skin " + skin.name + "...");

	// "Each skin is defined by the inverseBindMatrices property (which points to an accessor with IBM data), used to bring coordinates being skinned into the same space as each joint"
	js::Vector<Matrix4f, 16> ibms(skin.joints.size(), Matrix4f::identity());

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

		// skin.joints: "The array length must be the same as the count property of the inverseBindMatrices accessor (when defined)."
		// Since we may have added new joints for rigid-body animated meshes, just check <= instead of ==.
		checkProperty(accessor.count <= ibms.size(), "accessor.count had invalid size"); 

		for(size_t i=0; i<accessor.count; ++i)
			std::memcpy(&ibms[i].e, offset_base + byte_stride * i, sizeof(float) * 16);
	}

	// Assign bind matrices to nodes
	for(size_t i=0; i<skin.joints.size(); ++i)
	{
		const int node_i = skin.joints[i];
		checkProperty(node_i >= 0 && node_i < (int)data.nodes.size(), "node_index was invalid");
		anim_data.nodes[node_i].inverse_bind_matrix = ibms[i];
	}

	anim_data.joint_nodes = skin.joints;

	// conPrint("done.");
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

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	return loadGLBFileFromData(file.fileData(), file.fileSize(), gltf_base_dir, /*write_images_to_disk=*/true, data_out);
}


static_assert(sizeof(GLBHeader) == 12, "sizeof(GLBHeader) == 12");
static_assert(sizeof(GLBChunkHeader) == 8, "sizeof(GLBChunkHeader) == 8");


// Takes raw data pointer so we can use for fuzzing.
Reference<BatchedMesh> FormatDecoderGLTF::loadGLBFileFromData(const void* file_data, const size_t file_size, const std::string& gltf_base_dir, bool write_images_to_disk, GLTFLoadedData& data_out)
{
	BufferViewInStream stream(ArrayRef<uint8>((const uint8*)file_data, file_size));

	// Read GLB header
	GLBHeader header;
	stream.readData(&header, sizeof(GLBHeader));

	if(header.magic != 0x46546C67)
		throw glare::Exception("Invalid magic number in header");
	

	// Read JSON chunk header
	GLBChunkHeader json_header;
	stream.readData(&json_header, sizeof(GLBChunkHeader));
	if(json_header.chunk_type != CHUNK_TYPE_JSON)
		throw glare::Exception("Expected JSON chunk type");

	runtimeCheck(stream.getReadIndex() == 20);

	// Check JSON header chunk_length is valid:
	const size_t json_chunk_end = stream.getReadIndex() + (size_t)json_header.chunk_length;
	if(!stream.canReadNBytes((size_t)json_header.chunk_length))
		throw glare::Exception("JSON Chunk too large.");

	
	// Read binary buffer chunk header
	// First determine the offset of the binary buffer chunk header

	// Try with the correct 4-byte alignment of the BIN chunk
	size_t bin_buf_chunk_header_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(json_chunk_end, 4);
	stream.setReadIndex(bin_buf_chunk_header_offset); // NOTE: bin_buf_chunk_header_offset may be invalid, in which case setReadIndex() will throw an exception.
	GLBChunkHeader bin_buf_header;
	stream.readData(&bin_buf_header, sizeof(GLBChunkHeader));
	if(bin_buf_header.chunk_type != CHUNK_TYPE_BIN)
	{
		// Try again without 4-byte alignment, some files may incorrectly not have 4-byte alignment for the BIN chunk.  See for example https://x.com/SubstrataVr/status/1906356996921811188
		bin_buf_chunk_header_offset = json_chunk_end;
		stream.setReadIndex(bin_buf_chunk_header_offset);
		stream.readData(&bin_buf_header, sizeof(GLBChunkHeader));
		if(bin_buf_header.chunk_type != CHUNK_TYPE_BIN)
			throw glare::Exception("Expected BIN chunk type");
	}

	// Check binary buffer chunk_length is valid:
	if(!stream.canReadNBytes(bin_buf_header.chunk_length)) // bin_buf_chunk_header_offset + sizeof(GLBChunkHeader) + bin_buf_header.chunk_length > file_size)
		throw glare::Exception("Bin buf chunk too large.");

	// Make a buffer object for it
	GLTFBufferRef buffer = new GLTFBuffer();

	if((bin_buf_chunk_header_offset % 4) == 0) // if properly 4-byte aligned (should be the case for valid GLB files):
		buffer->binary_data = (const uint8*)file_data + bin_buf_chunk_header_offset + sizeof(GLBChunkHeader);
	else // else if not properly 4-byte aligned:
	{
		// Copy to a temporary buffer with proper alignment.  Do this because a bunch of code processing the vertices etc. requires correct (4-byte) alignment.
		// Note that bin_buf_header.chunk_length has been checked above in the stream.canReadNBytes() call.
		buffer->copied_data.resize(bin_buf_header.chunk_length);
		std::memcpy(buffer->copied_data.data(), (const uint8*)file_data + bin_buf_chunk_header_offset + sizeof(GLBChunkHeader), bin_buf_header.chunk_length);

		buffer->binary_data = buffer->copied_data.data();
	}

	buffer->data_size = bin_buf_header.chunk_length;

	if(false)
	{
		// Save JSON to disk for debugging
		const std::string json((const char*)file_data + 20, json_header.chunk_length);
		FileUtils::writeEntireFileTextMode(gltf_base_dir + "/extracted.json", json);
	}

	// Parse JSON chunk. (chunk length already checked above)
	JSONParser parser;
	parser.parseBuffer((const char*)file_data + 20, json_header.chunk_length);

	return loadGivenJSON(parser, gltf_base_dir, buffer, write_images_to_disk, data_out);
}


Reference<BatchedMesh> FormatDecoderGLTF::loadGLTFFile(const std::string& pathname, GLTFLoadedData& data_out)
{
	MemMappedFile file(pathname);

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	return loadGLTFFileFromData(file.fileData(), file.fileSize(), gltf_base_dir, /*write_images_to_disk=*/true, data_out);
}


Reference<BatchedMesh> FormatDecoderGLTF::loadGLTFFileFromData(const void* data, const size_t datalen, const std::string& gltf_base_dir, bool write_images_to_disk, GLTFLoadedData& data_out)
{
	JSONParser parser;
	parser.parseBuffer((const char*)data, datalen);

	return loadGivenJSON(parser, gltf_base_dir, /*glb_bin_buffer=*/NULL, write_images_to_disk, data_out);
}


Reference<BatchedMesh> FormatDecoderGLTF::loadGivenJSON(JSONParser& parser, const std::string gltf_base_dir, const GLTFBufferRef& glb_bin_buffer, bool write_images_to_disk,
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
				{
					buffer->uri = buffer_node.getChildStringValue(parser, "uri");

					if(hasPrefix(buffer->uri, "data:")) // If this is a data URI (see https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs)
					{
						const std::string expected_prefix_1 = "data:application/octet-stream;base64,";
						const std::string expected_prefix_2 = "data:application/gltf-buffer;base64,";

						std::string data_base64;
						if(hasPrefix(buffer->uri, expected_prefix_1))
							data_base64 = buffer->uri.substr(expected_prefix_1.size());
						else if(hasPrefix(buffer->uri, expected_prefix_2))
							data_base64 = buffer->uri.substr(expected_prefix_2.size());
						else
							throw glare::Exception("Expected prefix " + expected_prefix_1 + " or " + expected_prefix_2 + " in URI");

						Base64::decode(data_base64, /*data out=*/buffer->decoded_base64_data);

						buffer->binary_data = buffer->decoded_base64_data.data();
						buffer->data_size = buffer->decoded_base64_data.size();
					}
				}
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
				image->name = image_node.getChildStringValueWithDefaultVal(parser, "name", "");
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
				mat->alphaMode = mat_node.getChildStringValueWithDefaultVal(parser, "alphaMode", "OPAQUE");
				mat->doubleSided = mat_node.getChildBoolValueWithDefaultVal(parser, "doubleSided", false);

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

					mat->baseColorTexture = parseTextureIfPresent(parser, pbr_node, "baseColorTexture");
					mat->baseColorFactor = parseColour4ChildArrayWithDefault(parser, pbr_node, "baseColorFactor", Colour4f(1, 1, 1, 1));
					mat->roughnessFactor	= (float)pbr_node.getChildDoubleValueWithDefaultVal(parser, "roughnessFactor", 1.0);
					mat->metallicFactor		= (float)pbr_node.getChildDoubleValueWithDefaultVal(parser, "metallicFactor", 1.0);
					mat->metallicRoughnessTexture = parseTextureIfPresent(parser, pbr_node, "metallicRoughnessTexture");
				}
				else
				{
					// "When undefined [pbrMetallicRoughness element], all the default values of pbrMetallicRoughness MUST apply."
					mat->baseColorFactor = Colour4f(1, 1, 1, 1);
					mat->roughnessFactor = 1;
					mat->metallicFactor = 1;
				}

				mat->emissiveTexture = parseTextureIfPresent(parser, mat_node, "emissiveTexture");
				mat->emissiveFactor = parseColour3ChildArrayWithDefault(parser, mat_node, "emissiveFactor", Colour3f(0, 0, 0));

				mat->normalTexture = parseTextureIfPresent(parser, mat_node, "normalTexture");

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
				if(node_node.hasChild("skin")) node->skin = node_node.getChildUIntValue(parser, "skin");

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


	// Load VRM extension data
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "extensions")
		{
			const JSONNode& extensions_node = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(extensions_node, JSONNode::Type_Object);

			for(size_t z=0; z<extensions_node.name_val_pairs.size(); ++z)
			{
				if(extensions_node.name_val_pairs[z].name == "VRM")
				{
					data.vrm_data = new GLTFVRMExtension();

					const JSONNode& VRM_node = parser.nodes[extensions_node.name_val_pairs[z].value_node_index];
					checkNodeType(VRM_node, JSONNode::Type_Object);

					for(size_t q=0; q<VRM_node.name_val_pairs.size(); ++q)
					{
						if(VRM_node.name_val_pairs[q].name == "humanoid")
						{
							const JSONNode& humanoid_node = parser.nodes[VRM_node.name_val_pairs[q].value_node_index];
							checkNodeType(humanoid_node, JSONNode::Type_Object);

							const JSONNode& bones_array = humanoid_node.getChildArray(parser, "humanBones");
							
							for(size_t t=0; t<bones_array.child_indices.size(); ++t)
							{
								const JSONNode& bone_node = parser.nodes[bones_array.child_indices[t]];
								checkNodeType(bone_node, JSONNode::Type_Object);

								const std::string bone_name = bone_node.getChildStringValue(parser, "bone");

								VRMBoneInfo info;
								info.node_index = bone_node.getChildIntValue(parser, "node");

								data.vrm_data->human_bones.insert(std::make_pair(bone_name, info));
							}
						}
					}
				}
			}
		}

	// Load extensionsRequired, throw exception if we encounter an extension we don't support.
	for(size_t i=0; i<root.name_val_pairs.size(); ++i)
		if(root.name_val_pairs[i].name == "extensionsRequired")
		{
			const JSONNode& extensions_node = parser.nodes[root.name_val_pairs[i].value_node_index];
			checkNodeType(extensions_node, JSONNode::Type_Array);

			for(size_t z=0; z<extensions_node.child_indices.size(); ++z)
			{
				const std::string& extension = parser.nodes[extensions_node.child_indices[z]].getStringValue();

				if(extension == "KHR_materials_pbrSpecularGlossiness")
				{}
				else
					throw glare::Exception("Unsupported extension that file requires: '" + extension + "'");
			}
		}



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
			if(data.meshes[i]->primitives[z]->attributes.count("TANGENT"))
				data.attr_present.tangent_present = true;
		}
	}

	//data.attr_present.tangent_present = true; // TEMP HACK


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


	// If there is an animation, but no skinning information, then add a skin.
	// We do this so we can use the skin-based animation system for rigid-body animations.
	if(!data.animations.empty() && data.skins.empty())
	{
		GLTFSkinRef skin = new GLTFSkin();
		skin->name = "Anim skin";
		skin->inverse_bind_matrices = -1;
		skin->skeleton = -1;
		data.skins.push_back(skin);
	}


	// If there is animation data, we will handle rigid body animations using skinning system, so we want joint and weight vertex attributes.
	// If there is a skin that is not just empty (e.g. it has some joints), then we want joint and weight vertex attributes.
	if(!data.animations.empty() || (!data.skins.empty() && !data.skins[0]->joints.empty()))
	{
		data.attr_present.joints_present = true;
		data.attr_present.weights_present = true;
	}


	// Process meshes referenced by nodes to get the total number of vertices and triangles, so we can reserve space for them.
	size_t total_num_indices = 0;
	size_t total_num_verts = 0;
	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		processNodeToGetMeshCapacity(data, root_node, /*depth=*/0, total_num_indices, total_num_verts);
	}


	Reference<BatchedMesh> batched_mesh = new BatchedMesh();

	// Build mesh attributes
	// Note that the batched_mesh->vertexSize() calls below calculate the total size of the vertex given all the attributes previously added, which gives the correct offset.
	batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Position, BatchedMesh::ComponentType_Float, /*offset_B=*/0));

	if(data.attr_present.normal_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Normal, BatchedMesh::ComponentType_PackedNormal, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.tangent_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Tangent, BatchedMesh::ComponentType_PackedNormal, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.vert_col_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Colour, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.texcoord_0_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_UV_0, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.joints_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Joints, BatchedMesh::ComponentType_UInt16, /*offset_B=*/batched_mesh->vertexSize()));

	if(data.attr_present.weights_present)
		batched_mesh->vert_attributes.push_back(BatchedMesh::VertAttribute(BatchedMesh::VertAttribute_Weights, BatchedMesh::ComponentType_Float, /*offset_B=*/batched_mesh->vertexSize()));

	assert(batched_mesh->vertexSize() % 4 == 0); // All attributes we choose above currently have a length of a multiple of 4 bytes.


	batched_mesh->vertex_data.resize(batched_mesh->vertexSize() * total_num_verts);

	js::Vector<uint32, 16> uint32_indices;
	uint32_indices.reserve(total_num_indices);
	size_t vert_write_i = 0;

	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		Matrix4f current_transform = Matrix4f::identity();

		processNode(data, root_node, scene_node.nodes[i], current_transform, *batched_mesh, uint32_indices, vert_write_i);
	}

	assert(uint32_indices.size() == total_num_indices);
	assert(vert_write_i == total_num_verts);


	batched_mesh->setIndexDataFromIndices(uint32_indices, total_num_verts);

	// Compute AABB
	// Assuming we are storing float vert positions here.
	// We could also read the bounds from the GLTF file, but they may be incorrect.
	{
		js::AABBox aabb = js::AABBox::emptyAABBox();
		const size_t vert_size_B = batched_mesh->vertexSize();
		const BatchedMesh::VertAttribute& pos_attr = batched_mesh->getAttribute(BatchedMesh::VertAttribute_Position);
		runtimeCheck(pos_attr.component_type == BatchedMesh::ComponentType_Float);
		runtimeCheck(vert_size_B % 4 == 0); // Check alignment
		runtimeCheck(pos_attr.offset_B % 4 == 0); // Check alignment

		for(size_t v=0; v<total_num_verts; ++v)
		{
			const Vec4f vert_pos(
				((float*)&batched_mesh->vertex_data[v * vert_size_B + pos_attr.offset_B])[0],
				((float*)&batched_mesh->vertex_data[v * vert_size_B + pos_attr.offset_B])[1],
				((float*)&batched_mesh->vertex_data[v * vert_size_B + pos_attr.offset_B])[2],
				1);
			aabb.enlargeToHoldPoint(vert_pos);
		}
		batched_mesh->aabb_os = aabb;
	}


	// Process images - for any image embedded in the GLB file data, save onto disk in a temp location
	for(size_t i=0; i<data.images.size(); ++i)
	{
		processImage(data, *data.images[i], gltf_base_dir, write_images_to_disk);
	}

	// Process materials
	data_out.materials.materials.resize(data.materials.size());
	for(size_t i=0; i<data.materials.size(); ++i)
	{
		processMaterial(data, *data.materials[i], gltf_base_dir, data_out.materials.materials[i]);
	}

	//-------------------- Process animations ---------------------------
	// Write out nodes

	AnimationData& batched_mesh_anim_data = batched_mesh->animation_data;
	batched_mesh_anim_data.nodes.resize(data.nodes.size());
	for(size_t i=0; i<data.nodes.size(); ++i)
	{
		batched_mesh_anim_data.nodes[i].parent_index = -1;
		batched_mesh_anim_data.nodes[i].inverse_bind_matrix = Matrix4f::identity();

		batched_mesh_anim_data.nodes[i].trans = data.nodes[i]->translation.toVec4fVector();
		batched_mesh_anim_data.nodes[i].rot   = data.nodes[i]->rotation;
		batched_mesh_anim_data.nodes[i].scale = data.nodes[i]->scale.toVec4fVector();

		batched_mesh_anim_data.nodes[i].name         = data.nodes[i]->name;
	}

	// Set parent indices in data_out.nodes
	for(size_t i=0; i<data.nodes.size(); ++i)
	{
		const GLTFNode& node = *data.nodes[i];

		for(size_t z=0; z<node.children.size(); ++z)
		{
			const size_t child_index = node.children[z];
			checkProperty(child_index < batched_mesh_anim_data.nodes.size(), "invalid child_index");
			batched_mesh_anim_data.nodes[child_index].parent_index = (int)i;
		}
	}
	
	// Build sorted_nodes - Indices of nodes that are used as joints, sorted such that children always come after parents.
	// We will do this by doing a depth first traversal over the nodes.
	std::vector<size_t> node_stack;

	// Initialise node_stack with roots
	for(size_t i=0; i<data.nodes.size(); ++i)
		if(batched_mesh_anim_data.nodes[i].parent_index == -1)
			node_stack.push_back(i);

	std::set<int> sorted_nodes_set;

	while(!node_stack.empty())
	{
		const int node_i = (int)node_stack.back();
		node_stack.pop_back();

		// Check if we have already processed this node, to avoid getting stuck in cycles.
		if(sorted_nodes_set.count(node_i) != 0) // Found node twice in depth-first traversal
			throw glare::Exception("Nodes are not a strict tree");
		sorted_nodes_set.insert(node_i);

		batched_mesh_anim_data.sorted_nodes.push_back(node_i);

		// Add child nodes of the current node
		const GLTFNode& node = *data.nodes[node_i];
		for(size_t z=0; z<node.children.size(); ++z)
		{
			const size_t child_index = node.children[z];
			node_stack.push_back(child_index);
		}
	}


	// Process animations
	// Remap input and output accessor indices to avoid gaps in keyframe time and value arrays.
	// 
	// Do a pass to get that largest input and output accessor indices.
	int largest_input_accessor = -1;
	int largest_output_accessor = -1;

	for(size_t i=0; i<data.animations.size(); ++i)
	{
		const GLTFAnimation* anim = data.animations[i].ptr();
		for(size_t c=0; c<anim->channels.size(); ++c)
		{
			const GLTFChannel& channel = anim->channels[c];

			// Get sampler
			checkProperty(channel.sampler >= 0 && channel.sampler < (int)anim->samplers.size(), "invalid sampler index");
			const GLTFSampler& sampler = anim->samplers[channel.sampler];

			checkProperty(sampler.input  >= 0 && sampler.input  < (int)data.accessors.size(), "invalid anim sampler accessor index");
			checkProperty(sampler.output >= 0 && sampler.output < (int)data.accessors.size(), "invalid anim sampler accessor index");

			largest_input_accessor  = myMax(largest_input_accessor,  sampler.input);
			largest_output_accessor = myMax(largest_output_accessor, sampler.output);
		}
	}

	// Sanity check largest_input_accessor etc.
	checkProperty(largest_input_accessor  >= -1 && largest_input_accessor  < 1000000, "invalid animation input accessor");
	checkProperty(largest_output_accessor >= -1 && largest_output_accessor < 1000000, "invalid animation output accessor");

	std::vector<int> new_input_index(largest_input_accessor   + 1, -1); // Map from old accessor index to new accessor data index.
	std::vector<int> new_output_index(largest_output_accessor + 1, -1);

	int cur_new_input_index = 0;
	int cur_new_output_index = 0;

	for(size_t i=0; i<data.animations.size(); ++i)
	{
		const GLTFAnimation* anim = data.animations[i].ptr();
		for(size_t c=0; c<anim->channels.size(); ++c)
		{
			const GLTFChannel& channel = anim->channels[c];
			const GLTFSampler& sampler = anim->samplers[channel.sampler];

			if(new_input_index[sampler.input] == -1)
				new_input_index[sampler.input] = cur_new_input_index++;

			if(new_output_index[sampler.output] == -1)
				new_output_index[sampler.output] = cur_new_output_index++;
		}
	}

	batched_mesh_anim_data.keyframe_times.resize(cur_new_input_index  + 1);
	batched_mesh_anim_data.output_data   .resize(cur_new_output_index + 1);


	// Process channel input accessor data
	for(size_t z=0; z<new_input_index.size(); ++z)
	{
		const size_t old_accessor_i = z;
		const int new_i = new_input_index[z];
		if(new_i != -1) // If used:
		{
			// Read keyframe time values from the input accessor, write to time_vals vector.
			std::vector<float>& time_vals = batched_mesh_anim_data.keyframe_times[new_i].times;
			const GLTFAccessor& input_accessor = getAccessor(data, old_accessor_i);

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
	}

	// Process channel output accessor data
	for(size_t z=0; z<new_output_index.size(); ++z)
	{
		const size_t old_accessor_i = z;
		const int new_i = new_output_index[z];
		if(new_i != -1) // If used:
		{
			const GLTFAccessor& output_accessor = getAccessor(data, old_accessor_i);

			js::Vector<Vec4f, 16>& output_data = batched_mesh_anim_data.output_data[new_i];

			// NOTE: GLTF translations and scales must be FLOAT.  But rotation can be a bunch of stuff, BYTE etc..  TODO: handle.
			if(output_accessor.component_type != GLTF_COMPONENT_TYPE_FLOAT)
				throw glare::Exception("anim output component_type was not float");

			const GLTFBufferView& buf_view = getBufferView(data, output_accessor.buffer_view);
			const GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);
			const size_t offset_B = output_accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
			const uint8* offset_base = buffer.binary_data + offset_B;
			const size_t num_components = typeNumComponents(output_accessor.type);
			const size_t value_size_B = componentTypeByteSize(output_accessor.component_type) * num_components;
			const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : value_size_B;

			checkAccessorBounds(byte_stride, offset_B, value_size_B, output_accessor, buffer);
			checkProperty(num_components == 1 || num_components == 3 || num_components == 4, "Animation: num components must be 1, 3 or 4.");

			output_data.resize(output_accessor.count, Vec4f(0.f));
			
			if(num_components == 1)
			{
				// This seems to be used by morph targets, that we don't support currently.  Just read and convert to vec4s.
				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float)); // other components should already be zeroed.
			}
			else if(num_components == 3)
			{
				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float) * 3); // W component should already be zeroed.
			}
			else
			{
				for(size_t i=0; i<output_accessor.count; ++i)
					std::memcpy(&output_data[i], offset_base + byte_stride * i, sizeof(float) * 4);
			}
		}
	}


	batched_mesh_anim_data.animations.resize(data.animations.size());
	for(size_t i=0; i<data.animations.size(); ++i)
	{
		batched_mesh_anim_data.animations[i] = new AnimationDatum();
		processAnimation(data, *data.animations[i], gltf_base_dir, new_input_index, new_output_index, batched_mesh_anim_data, *batched_mesh_anim_data.animations[i]);
	}

	// Process skins.  Use last skin if there is more than 1.
	if(!data.skins.empty())
		processSkin(data, *data.skins.back(), gltf_base_dir, batched_mesh_anim_data);

	batched_mesh->animation_data.vrm_data = data.vrm_data;

	batched_mesh->animation_data.build();

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


static int gltfComponentType(BatchedMesh::ComponentType t)
{
	switch(t)
	{
	case BatchedMesh::ComponentType_Float: return GLTF_COMPONENT_TYPE_FLOAT;
	case BatchedMesh::ComponentType_Half: throw glare::Exception("Unhandled component type ComponentType_Half");
	case BatchedMesh::ComponentType_UInt8: return GLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	case BatchedMesh::ComponentType_UInt16: return GLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	case BatchedMesh::ComponentType_UInt32: return GLTF_COMPONENT_TYPE_UNSIGNED_INT;
	case BatchedMesh::ComponentType_PackedNormal: throw glare::Exception("Unhandled component type ComponentType_PackedNormal");
	default: throw glare::Exception("Error");
	}
};


void FormatDecoderGLTF::makeGLTFJSONAndBin(const BatchedMesh& mesh, const std::string& bin_path, std::string& json_out, js::Vector<uint8, 16>& bin_out)
{
	json_out = 
		"{\n"
		"\"asset\": {\n"
		"	\"generator\": \"Glare Technologies GLTF Writer\",\n"
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


	// Compute new vert size, putting data in GLTF compatible format

	const BatchedMesh::VertAttribute* pos_attr = mesh.findAttribute(BatchedMesh::VertAttribute_Position);
	const BatchedMesh::VertAttribute* normal_attr = mesh.findAttribute(BatchedMesh::VertAttribute_Normal);
	const BatchedMesh::VertAttribute* colour_attr = mesh.findAttribute(BatchedMesh::VertAttribute_Colour);
	const BatchedMesh::VertAttribute* uv0_attr = mesh.findAttribute(BatchedMesh::VertAttribute_UV_0);
	const BatchedMesh::VertAttribute* uv1_attr = mesh.findAttribute(BatchedMesh::VertAttribute_UV_1);

	if(!pos_attr)
		throw glare::Exception("pos_attr was missing.");

	size_t dest_vert_size = sizeof(Vec3f); // position
	int normal_offset = -1;
	if(normal_attr)
	{
		normal_offset = (int)dest_vert_size;
		dest_vert_size += sizeof(Vec3f);
	};
	int colour_offset = -1;
	if(colour_attr)
	{
		colour_offset = (int)dest_vert_size;
		dest_vert_size += sizeof(Vec3f);
	};
	int uv0_offset = -1;
	if(uv0_attr)
	{
		uv0_offset = (int)dest_vert_size;
		dest_vert_size += sizeof(float) * 2;
	};
	int uv1_offset = -1;
	if(uv1_attr)
	{
		uv1_offset = (int)dest_vert_size;
		dest_vert_size += sizeof(float) * 2;
	};

	const size_t mesh_vert_size = mesh.vertexSize();
	const size_t num_verts = mesh.numVerts();
	const size_t total_vert_data_size = num_verts * dest_vert_size;

	// The vertex data needs to be 4-byte aligned, so pad the index data.
	const size_t tri_index_data_size = mesh.index_data.size();
	const size_t padded_tri_index_data_size = Maths::roundUpToMultipleOfPowerOf2<size_t>(tri_index_data_size, 4);
	const size_t tri_index_padding_bytes = padded_tri_index_data_size - tri_index_data_size;
	assert(tri_index_padding_bytes < 4);

	// Build and then write data binary (.bin) file.
	js::Vector<uint8, 16>& data = bin_out;
	data.resize(padded_tri_index_data_size + total_vert_data_size);

	std::memcpy(data.data(), mesh.index_data.data(), mesh.index_data.size()); // Copy vert indices

	// Zero out padding if present
	if(tri_index_padding_bytes > 0)
		std::memset(&data[tri_index_data_size], 0, tri_index_padding_bytes);

	float* write_pos = (float*)(data.data() + padded_tri_index_data_size);
	for(size_t i=0; i<num_verts; ++i)
	{
		//float* write_pos = (float*)(data.data() + padded_tri_index_data_size + dest_vert_size * i);

		if(pos_attr->component_type == BatchedMesh::ComponentType_Float)
		{
			std::memcpy(write_pos, &mesh.vertex_data[mesh_vert_size * i], sizeof(Indigo::Vec3f)); // Copy vert position
			write_pos += 3;
		}
		else
			throw glare::Exception("Unhandled pos component type: " + toString((int)pos_attr->component_type));

		if(normal_attr)
		{
			if(normal_attr->component_type == BatchedMesh::ComponentType_Float)
			{
				std::memcpy(write_pos, &mesh.vertex_data[mesh_vert_size * i + normal_attr->offset_B], sizeof(Indigo::Vec3f)); // Copy vert position
				write_pos += 3;
			}
			else if(normal_attr->component_type == BatchedMesh::ComponentType_PackedNormal)
			{
				uint32 packed_normal;
				std::memcpy(&packed_normal, &mesh.vertex_data[mesh_vert_size * i + normal_attr->offset_B], sizeof(uint32));
				const Vec4f vert_normal = batchedMeshUnpackNormal(packed_normal);
				std::memcpy(write_pos, vert_normal.x, sizeof(Indigo::Vec3f)); // Copy vert normal
				write_pos += 3;
			}
			else
				throw glare::Exception("Unhandled normal component type: " + toString((int)normal_attr->component_type));
		}

		if(colour_attr)
		{
			if(colour_attr->component_type == BatchedMesh::ComponentType_Float)
			{
				std::memcpy(write_pos, &mesh.vertex_data[mesh_vert_size * i + colour_attr->offset_B], sizeof(Indigo::Vec3f)); // Copy vert colour
				write_pos += 3;
			}
			else
				throw glare::Exception("Unhandled colour component type: " + toString((int)colour_attr->component_type));
		}

		if(uv0_attr)
		{
			if(uv0_attr->component_type == BatchedMesh::ComponentType_Float)
			{
				std::memcpy(write_pos, &mesh.vertex_data[mesh_vert_size * i + uv0_attr->offset_B], sizeof(Indigo::Vec2f)); // Copy vert UV
				write_pos += 2;
			}
			else
				throw glare::Exception("Unhandled uv0 component type: " + toString((int)uv0_attr->component_type));
		}

		if(uv1_attr)
		{
			if(uv1_attr->component_type == BatchedMesh::ComponentType_Float)
			{
				std::memcpy(write_pos, &mesh.vertex_data[mesh_vert_size * i + uv1_attr->offset_B], sizeof(Indigo::Vec2f)); // Copy vert UV
				write_pos += 2;
			}
			else
				throw glare::Exception("Unhandled uv0 component type: " + toString((int)uv1_attr->component_type));
		}
	}
	assert(write_pos == (float*)(data.data() + padded_tri_index_data_size + dest_vert_size * num_verts));


	if(bin_path.empty())
	{
		json_out +=
			"\"buffers\": [\n"
			"	{\n"
			"		\"byteLength\": " + toString(data.size()) + "\n"
			"	}\n"
			"],\n";
	}
	else
	{
		const std::string bin_filename = FileUtils::getFilename(bin_path);

		// Write buffer element
		// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#binary-data-storage
		json_out +=
		"\"buffers\": [\n"
		"	{\n"
		"		\"byteLength\": " + toString(data.size()) + ",\n"
		"		\"uri\": \"" + bin_filename + "\"\n"
		"	}\n"
		"],\n";
	}


	// Write bufferViews
	json_out += "\"bufferViews\": [\n";

	json_out +=
	"	{\n" // Buffer view for triangle vert indices (for all chunks)
	"		\"buffer\": 0,\n"
	"		\"byteLength\": " + toString(tri_index_data_size) + ",\n"
	"		\"byteOffset\": 0,\n"
	"		\"target\": 34963\n" // 34963 = ELEMENT_ARRAY_BUFFER
	"	},\n";
	
	json_out +=
	"	{\n" // Buffer view for vertex data
	"		\"buffer\": 0,\n"
	"		\"byteLength\": " + toString(total_vert_data_size) + ",\n"
	"		\"byteOffset\": " + toString(padded_tri_index_data_size) + ",\n"
	"		\"byteStride\": " + toString(dest_vert_size) + ",\n"
	"		\"target\": 34962\n" // 34962 = ARRAY_BUFFER
	"	}\n"
	"],\n";
	
	// Write accessors
	json_out += "\"accessors\": [\n";

	const size_t index_type_size = BatchedMesh::componentTypeSize(mesh.index_type);

	for(size_t i=0; i<mesh.batches.size(); ++i)
	{
		const BatchedMesh::IndicesBatch& batch = mesh.batches[i];

		// Make accessors for chunk indices
		json_out +=
			"	" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"		\"bufferView\": 0,\n" // buffer view 0 is the index buffer view
			"		\"byteOffset\": " + toString(batch.indices_start * index_type_size) + ",\n"
			"		\"componentType\": " + toString(gltfComponentType(mesh.index_type)) + ",\n"
			"		\"count\": " + toString(batch.num_indices) + ",\n"
			"		\"type\": \"SCALAR\"\n"
			"	}\n";
	}


	int next_accessor = (int)mesh.batches.size();

	// Write accessor for vertex position
	const int position_accessor = next_accessor++;
	json_out +=
	"	,{\n"
	"		\"bufferView\": 1,\n"
	"		\"byteOffset\": 0,\n"
	"		\"componentType\": " + toString(GLTF_COMPONENT_TYPE_FLOAT) + ",\n"
	"		\"count\": " + toString(mesh.numVerts()) + ",\n"
	"		\"max\": [\n"
	"			" + formatDouble(mesh.aabb_os.max_[0]) + ",\n"
	"			" + formatDouble(mesh.aabb_os.max_[1]) + ",\n"
	"			" + formatDouble(mesh.aabb_os.max_[2]) + "\n"
	"		],\n"
	"		\"min\": [\n"
	"			" + formatDouble(mesh.aabb_os.min_[0]) + ",\n"
	"			" + formatDouble(mesh.aabb_os.min_[1]) + ",\n"
	"			" + formatDouble(mesh.aabb_os.min_[2]) + "\n"
	"		],\n"
	"		\"type\": \"VEC3\"\n"
	"	}\n";

	int vert_normal_accessor = -1;
	if(normal_attr)
	{
		//  Write accessor for vertex normals
		json_out +=
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(normal_offset) + ",\n"
		"		\"componentType\": " + toString(GLTF_COMPONENT_TYPE_FLOAT) + ",\n"
		"		\"count\": " + toString(mesh.numVerts()) + ",\n"
		"		\"type\": \"VEC3\"\n"
		"	}\n";

		vert_normal_accessor = next_accessor++;
	}

	int vert_colour_accessor = -1;
	if(colour_attr)
	{
		//  Write accessor for vertex colours
		json_out +=
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(colour_offset) + ",\n"
		"		\"componentType\": " + toString(GLTF_COMPONENT_TYPE_FLOAT) + ",\n"
		"		\"count\": " + toString(mesh.numVerts()) + ",\n"
		"		\"type\": \"VEC3\"\n"
		"	}\n";
		vert_colour_accessor = next_accessor++;
	}
	
	int uv0_accessor = -1;
	if(uv0_attr)
	{
		//  Write accessor for vertex UVs
		json_out +=
		"	,{\n"
		"		\"bufferView\": 1,\n"
		"		\"byteOffset\": " + toString(uv0_offset) + ",\n"
		"		\"componentType\": " + toString(GLTF_COMPONENT_TYPE_FLOAT) + ",\n"
		"		\"count\": " + toString(mesh.numVerts()) + ",\n"
		"		\"type\": \"VEC2\"\n"
		"	}\n";
		uv0_accessor = next_accessor++;
	}

	int uv1_accessor = -1;
	if(uv1_attr)
	{
		//  Write accessor for vertex UVs
		json_out +=
			"	,{\n"
			"		\"bufferView\": 1,\n"
			"		\"byteOffset\": " + toString(uv1_offset) + ",\n"
			"		\"componentType\": " + toString(GLTF_COMPONENT_TYPE_FLOAT) + ",\n"
			"		\"count\": " + toString(mesh.numVerts()) + ",\n"
			"		\"type\": \"VEC2\"\n"
			"	}\n";
		uv1_accessor = next_accessor++;
	}

	// Finish accessors array
	json_out += "],\n";

	// Write meshes element.
	json_out += "\"meshes\": [\n"
	"	{\n"
	"		\"primitives\": [\n";

	for(size_t i=0; i<mesh.batches.size(); ++i)
	{
		const BatchedMesh::IndicesBatch& batch = mesh.batches[i];

		json_out += "			" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"				\"attributes\": {\n"
			"					\"POSITION\": " + toString(position_accessor);

		if(vert_normal_accessor >= 0)
			json_out += ",\n					\"NORMAL\": " + toString(vert_normal_accessor);

		if(vert_colour_accessor >= 0)
			json_out += ",\n					\"COLOR_0\": " + toString(vert_colour_accessor);

		if(uv0_accessor >= 0)
			json_out += ",\n					\"TEXCOORD_0\": " + toString(uv0_accessor);

		if(uv1_accessor >= 0)
			json_out += ",\n					\"TEXCOORD_1\": " + toString(uv1_accessor);

		json_out += "\n";

		json_out +=
		"				},\n"
		"				\"indices\": " + toString(i) + ",\n" // First accessors are the index accessors
		"				\"material\": " + toString(batch.material_index) + ",\n"
		"				\"mode\": 4\n" // 4 = GLTF_MODE_TRIANGLES
		"			}\n";
	}

	// End primitives and meshes array
	json_out +=
	"		]\n" // End primitives array
	"	}\n" // End mesh hash
	"],\n"; // End meshes array


	// Write materials
	json_out += "\"materials\": [\n";

	std::vector<GLTFResultMap> textures;

	for(size_t i=0; i<mesh.numMaterialsReferenced(); ++i)
	{
		json_out +=
			"	" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"		\"pbrMetallicRoughness\": {\n";

		/*if(!gltf_data.materials.materials[i].diffuse_map.path.empty())
		{
			file <<
				"			\"baseColorTexture\": {\n"
				"				\"index\": " + toString(textures.size()) + "\n"
				"			},\n";

			textures.push_back(gltf_data.materials.materials[i].diffuse_map);
		}*/

		json_out +=
			"			\"metallicFactor\": 0.0\n"
			"		},\n" //  end of pbrMetallicRoughness hash
			"		\"emissiveFactor\": [\n"
			"			0.0,\n"
			"			0.0,\n"
			"			0.0\n"
			"		]\n"
			"	}\n"; // end of material hash
	}

	json_out += "]\n"; // end materials array


	// Write textures
	/*if(!textures.empty())
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
	}*/

	// Write samplers
	/*file <<
		"\"samplers\": [\n"
		"	{\n"
		"		\"magFilter\": 9729,\n" // TODO: work out best values for these
		"		\"minFilter\": 9986,\n"
		"		\"wrapS\": 10497,\n"
		"		\"wrapT\": 10497\n"
		"	}\n"
		"]\n";
	*/

	json_out += "}\n";
}


void FormatDecoderGLTF::writeBatchedMeshToGLTFFile(const BatchedMesh& mesh, const std::string& path, const GLTFWriteOptions& options)
{
	const std::string bin_filename = ::eatExtension(FileUtils::getFilename(path)) + "bin";
	const std::string bin_path = ::eatExtension(path) + "bin";

	std::string json;
	js::Vector<uint8, 16> bin;
	makeGLTFJSONAndBin(mesh, bin_path, json, bin);

	std::ofstream file(path);
	file << json;
	
	FileUtils::writeEntireFile(bin_path, (const char*)bin.data(), bin.size());
}


void FormatDecoderGLTF::writeBatchedMeshToGLBFile(const BatchedMesh& mesh, const std::string& path, const GLTFWriteOptions& options) // throws glare::Exception on failure
{
	// Make the JSON and binary buffer
	std::string json;
	js::Vector<uint8, 16> bin;
	makeGLTFJSONAndBin(mesh, /*bin_path=*/"", json, bin);


	FileOutStream file(path);

	const size_t padded_json_size = Maths::roundUpToMultipleOfPowerOf2<size_t>(json.size(), 4);
	const size_t num_json_padding_bytes = padded_json_size - json.size();
	assert(num_json_padding_bytes < 4);

	const size_t padded_bin_size = Maths::roundUpToMultipleOfPowerOf2<size_t>(bin.size(), 4);
	const size_t num_bin_padding_bytes = padded_bin_size - bin.size();
	assert(num_bin_padding_bytes < 4);

	GLBHeader glb_header;
	glb_header.magic = 0x46546C67;
	glb_header.version = 2;
	glb_header.length = (uint32)(sizeof(GLBHeader) + 
		sizeof(GLBChunkHeader) + padded_json_size + 
		sizeof(GLBChunkHeader) + padded_bin_size);
		// length = "total length of the Binary glTF, including header and all chunks, in bytes."

	file.writeData(&glb_header, sizeof(GLBHeader));

	//--------------- Write JSON chunk ---------------
	GLBChunkHeader json_chunk_header;
	json_chunk_header.chunk_length = (uint32)padded_json_size;
	json_chunk_header.chunk_type = 0x4E4F534A;

	file.writeData(&json_chunk_header, sizeof(GLBChunkHeader));

	file.writeData(json.data(), json.size());

	// Write padding after json data if needed, to get to multiple of 4 bytes.
	const char json_padding[4] = {' ', ' ', ' ', ' '}; // Pad with spaces
	file.writeData(json_padding, num_json_padding_bytes);

	//--------------- Write Binary chunk ---------------
	GLBChunkHeader bin_chunk_header;
	bin_chunk_header.chunk_length = (uint32)padded_bin_size;
	bin_chunk_header.chunk_type = 0x004E4942;

	file.writeData(&bin_chunk_header, sizeof(GLBChunkHeader));

	file.writeData(bin.data(), bin.size());

	// Write padding after binary data if needed, to get to multiple of 4 bytes.
	const uint8 zero_padding[4] = {0, 0, 0, 0};
	file.writeData(zero_padding, num_bin_padding_bytes);

	assert(file.getWriteIndex() == glb_header.length);
}


#if BUILD_TESTS && !defined(SDK_LIB)


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


/*static void testWritingToGLB(const std::string& bmesh_path)
{
	try
	{
		Reference<BatchedMesh> mesh = new BatchedMesh();
		BatchedMesh::readFromFile(bmesh_path, *mesh);

		GLTFWriteOptions options;
		//const std::string new_path = PlatformUtils::getTempDirPath() + "/temp.glb";
		const std::string new_path = "d:/files/temp.glb";

		FormatDecoderGLTF::writeBatchedMeshToGLBFile(*mesh, new_path, options);

		// Load again
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh2 = FormatDecoderGLTF::loadGLBFile(new_path, data);

		testEqual(mesh2->numMaterialsReferenced(), mesh->numMaterialsReferenced());
		//testEqual(mesh2->numVerts(), mesh->numVerts());
		testAssert(mesh2->numVerts() >= mesh->numVerts());
		testEqual(mesh2->numIndices(), mesh->numIndices());
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}*/


#if 0 // FUZZING
// Command line:
// C:\fuzz_corpus\glb N:\glare-core\trunk\testfiles\gltf -max_len=1000000

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	Clock::init();
	return 0;
}

static int iter = 0;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		//printVar(iter);
		//printVar(size);
		//iter++;

		GLTFLoadedData loaded_data;
		FormatDecoderGLTF::loadGLBFileFromData(data, size, "dummy_path", /*write_images_to_disk=*/false, loaded_data);
	
		//conPrint("parsed ok");
	}
	catch(glare::Exception& )
	{
		//conPrint("excep");
		//conPrint("excep: " + e.what());
	}
	return 0;  // Non-zero return values are reserved for future use.
}

#if 0
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		GLTFLoadedData loaded_data;
		FormatDecoderGLTF::loadGLTFFileFromData(data, size, "dummy_path", /*write_images_to_disk=*/false, loaded_data);
	
		conPrint("parsed ok");
	}
	catch(glare::Exception& e)
	{
		//conPrint("excep");
		conPrint("excep: " + e.what());
	}
	return 0;  // Non-zero return values are reserved for future use.
}
#endif

#endif // FUZZING


void FormatDecoderGLTF::test()
{
	conPrint("FormatDecoderGLTF::test()");

	//================= Test writeBatchedMeshToGLBFile =================
	/*try
	{
		// Read a GLTF file from disk
		const std::string path = "C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources/lambo_glb_14580587703686830024.bmesh";
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = BatchedMesh::readFromFile(path, nullptr);

		GLTFWriteOptions options;
		writeBatchedMeshToGLTFFile(*mesh, "d:/models/lambo_exported.gltf", options);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	return;*/

	//-----------------------------------  -----------------------------------
	/*try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile("D:\\models\\Alotta Money Artifex.glb", data);
		testAssert(mesh->numIndices() == 6046 * 3);
	}
	catch(glare::Exception&)
	{}*/

	//----------------------------------- Test handling of a GLB file with misaligned BIN chunk -----------------------------------
	try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/Blob_incorrect_BIN_chunk_alignment.glb", data);
		testAssert(mesh->numIndices() == 6046 * 3);
	}
	catch(glare::Exception&)
	{}

	//----------------------------------- Test handling of some invalid files -----------------------------------
	// channel.target_node out of bounds
	try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/crash-13e1627185a5ccb34acb7b96c2f8928d2f7d32ea.glb", data);

		failTest("Expected exception");
	}
	catch(glare::Exception&)
	{}

	// vert index out of bounds, was crashing in geometric normal generation:
	try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/crash-357f6ffbac1bf40494ac432acafd26c3af217e21.glb", data);

		failTest("Expected exception");
	}
	catch(glare::Exception&)
	{}
	
	// Cycle in node children
	try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/oom-bc3108a7c4460f210c7cbec3e5e968735adf493e.glb", data);

		failTest("Expected exception");
	}
	catch(glare::Exception&)
	{}

	// Invalid animation accessor index
	try
	{
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/crash-77909764a05558469c0dd179087495ec05ddf0d2.glb", data);

		failTest("Expected exception");
	}
	catch(glare::Exception&)
	{}


	try
	{
	
		/*{
			// Test loading a VRM file
			conPrint("---------------------------------meebit_09842_t_solid-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile("D:\\models\\fARTofwar_final.glb", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 5258);
			testAssert(mesh->numIndices() == 9348 * 3);

			//mesh->writeToFile(TestUtils::getTestReposDir() + "/testfiles/VRMs/meebit_09842_t_solid_vrm.bmesh");
			//testWriting(mesh, data);
		}*/


		//
		/*{
			// Test loading a VRM file
			conPrint("---------------------------------Wave_Hip_Hop_Dance.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile("D:\\models\\VRMs\\Wave_Hip_Hop_Dance.glb", data);
		}*/

		// Test a file with an image with a data URI with embedded base64 data.
		{
			// Read a PNG, base-64 encode it, to get the embedded texture for duck_with_embedded_texture.gltf.
			if(false)
			{
				std::string data = FileUtils::readEntireFile(TestUtils::getTestReposDir() + "/testfiles/pngs/PngSuite-2013jan13/basn3p02.png");
				std::string data_base64;
				Base64::encode(data.data(), data.size(), data_base64);

				conPrint(data_base64); // Gives iVBORw0KGgoAAAANSUhEUgAAACAAAAAgAgMAAAAOFJJnAAAABGdBTUEAAYagMeiWXwAAAANzQklUAQEBfC53ggAAAAxQTFRFAP8A/wAA//8AAAD/ZT8rugAAACJJREFUeJxj+B+6igGEGfAw8MnBGKugLHwMqNL/+BiDzD0AvUl/geqJjhsAAAAASUVORK5CYII=
			}


			// Has a buffer with a data URI with embedded base64 data.
			conPrint("---------------------------------RockWithDataURI.gltf-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLTFFile(TestUtils::getTestReposDir() + "/testfiles/gltf/duck_with_embedded_texture.gltf", data);

			testAssert(data.materials.materials.size() == 1);
			testAssert(hasExtension(data.materials.materials[0].diffuse_map.path, "png")); // Check the embedded PNG has been saved on disk and the image URI has been updated to the path to that file.

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 2399);
			testAssert(mesh->numIndices() == 4212 * 3);
		}


		{
			// Test loading a VRM file
			conPrint("---------------------------------meebit_09842_t_solid-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/VRMs/meebit_09842_t_solid.vrm", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 5258);
			testAssert(mesh->numIndices() == 9348 * 3);

			//mesh->writeToFile(TestUtils::getTestReposDir() + "/testfiles/VRMs/meebit_09842_t_solid_vrm.bmesh");
			//testWriting(mesh, data);
		}

		{
			// Test loading a VRM file
			conPrint("---------------------------------schurli.vrm-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/VRMs/schurli.vrm", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 4678);
			testAssert(mesh->numIndices() == 7444 * 3);

			mesh->writeToFile(TestUtils::getTestReposDir() + "/testfiles/VRMs/schurli_vrm.bmesh");
			//testWriting(mesh, data);
		}


		{
			// Has a buffer with a data URI with embedded base64 data.
			conPrint("---------------------------------RockWithDataURI.gltf-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLTFFile(TestUtils::getTestReposDir() + "/testfiles/gltf/RockWithDataURI.gltf", data);

			testAssert(mesh->numMaterialsReferenced() == 1);
			testAssert(mesh->numVerts() == 206);
			testAssert(mesh->numIndices() == 290 * 3);

			testWriting(mesh, data);
		}

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
			testAssert(mesh->animation_data.nodes.size() == 22);
			testAssert(mesh->animation_data.joint_nodes.size() == 19); // Aka num bones.
			testAssert(mesh->animation_data.animations.size() == 1);

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
			testAssert(mesh->animation_data.nodes.size() == 22);
			testAssert(mesh->animation_data.joint_nodes.size() == 19); // Aka num bones.
			testAssert(mesh->animation_data.animations.size() == 1);

			testWriting(mesh, data);
		}

		// Test loading Avocado.gltf, has tangents
		{
			conPrint("---------------------------------Avocado.gltf-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLTFFile(TestUtils::getTestReposDir() + "/testfiles/gltf/Avocado.gltf", data);

			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Tangent) != NULL);
			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Position) != NULL);
			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_UV_0) != NULL);
			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Normal) != NULL);
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

			testAssert(mesh->animation_data.nodes.size() == 5);
			testAssert(mesh->animation_data.joint_nodes.size() == 2); // Aka num bones.
			testAssert(mesh->animation_data.animations.size() == 1);

			testWriting(mesh, data);
		}


		// This file has a rigid-body animation.  We should have converted it to use a skinned animation (joints, weights etc.)
		{
			conPrint("---------------------------------BoxAnimated.glb-----------------------------------");
			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxAnimated.glb", data);

			testAssert(mesh->animation_data.animations.size() == 1);
			testAssert(mesh->animation_data.nodes.size() == 4);
			testAssert(mesh->animation_data.joint_nodes.size() == 2); // Aka num bones.  Only the 2 mesh nodes are converted to joints.

			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Joints) != NULL);
			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Weights) != NULL);

			testWriting(mesh, data);
		}


		// Test a mesh with a joint hierarchy, that originally had no vertex joints indices or weights.
		// We should have added vertex joints indices ands weights for animation.
		{

			GLTFLoadedData data;
			Reference<BatchedMesh> mesh = loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/MisfitPixels606.glb", data);

			testAssert(mesh->animation_data.nodes.size() == 39);
			testAssert(mesh->animation_data.joint_nodes.size() == 38); // Aka num bones.
			testAssert(mesh->animation_data.animations.size() == 0);

			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Joints) != NULL);
			testAssert(mesh->findAttribute(BatchedMesh::VertAttribute_Weights) != NULL);

			testWriting(mesh, data);
		}

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//================= Test writeBatchedMeshToGLBFile =================
	try
	{
		// Read a GLTF file from disk
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/gltf/duck/Duck.gltf";
		GLTFLoadedData data;
		Reference<BatchedMesh> mesh = loadGLTFFile(path, data);

		testAssert(mesh->numMaterialsReferenced() == 1);
		testAssert(mesh->numVerts() == 2399);
		testAssert(mesh->numIndices() == 4212 * 3);


		GLTFWriteOptions options;
		//writeBatchedMeshToGLTFFile(*mesh, TestUtils::getTestReposDir() + "/testfiles/gltf/duck/duck_new.gltf", options);

		const std::string new_path = PlatformUtils::getTempDirPath() + "/duck_new.glb";
		//const std::string write_path = TestUtils::getTestReposDir() + "/testfiles/gltf/duck/duck_new.glb";

		writeBatchedMeshToGLBFile(*mesh, new_path, options);

		// Load again
		Reference<BatchedMesh> mesh2 = loadGLBFile(new_path, data);

		testAssert(mesh2->numMaterialsReferenced() == 1);
		testAssert(mesh2->numVerts() == 2399);
		testAssert(mesh2->numIndices() == 4212 * 3);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	/*{
		const std::string path = "C:\\Users\\nick\\AppData\\Roaming\\Substrata/server_data/server_resources/arm_lower_glb_4324645340467802603.bmesh";
		testWritingToGLB(path);
	}*/

	//================= Test writeBatchedMeshToGLBFile on all GLBs we can find =================
	/*{
		std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPathsRecursive("C:\\Users\\nick\\AppData\\Roaming\\Cyberspace\\resources", "bmesh");
		std::sort(paths.begin(), paths.end());

		for(size_t i=0; i<paths.size(); ++i)
		{
			const std::string path = paths[i];
			conPrint(path);
			
			testWritingToGLB(path);
		}
	}*/

	conPrint("FormatDecoderGLTF::test() done.");
}


#endif // BUILD_TESTS
