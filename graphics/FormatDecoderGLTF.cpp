/*=====================================================================
FormatDecoderGLTF.cpp
---------------------
Copyright Glare Technologies Limited 2018 -
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
#include "../maths/Quat.h"
#include "../graphics/Colour4f.h"
#include <assert.h>
#include <vector>
#include <map>


static void checkNodeType(const JSONNode& node, JSONNode::Type type)
{
	if(node.type != type)
		throw Indigo::Exception("Expected type " + JSONNode::typeString(type) + ", got type " + JSONNode::typeString(node.type) + ".");
}


struct GLTFBuffer : public RefCounted
{
	GLTFBuffer() : file(NULL) {}
	~GLTFBuffer() { delete file; }
	
	MemMappedFile* file;
	const uint8* binary_data;
	std::string uri;
};
typedef Reference<GLTFBuffer> GLTFBufferRef;


// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-bufferview
struct GLTFBufferView : public RefCounted
{
	size_t buffer;
	size_t byte_length;
	size_t byte_offset;
	size_t byte_stride;
	std::string name;
	size_t target;
};
typedef Reference<GLTFBufferView> GLTFBufferViewRef;


struct GLTFImage : public RefCounted
{
	std::string uri;
};
typedef Reference<GLTFImage> GLTFImageRef;


struct GLTFTexture : public RefCounted
{
	size_t sampler;
	size_t source;
};
typedef Reference<GLTFTexture> GLTFTextureRef;


struct GLTPrimitive : public RefCounted
{
	std::map<std::string, size_t> attributes;
	size_t indices;
	size_t material;
	size_t mode;
};
typedef Reference<GLTPrimitive> GLTPrimitiveRef;


struct GLTFMesh: public RefCounted
{
	std::vector<GLTPrimitiveRef> primitives;
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
	INDIGO_ALIGNED_NEW_DELETE

	GLTFNode() : mesh(std::numeric_limits<size_t>::max()) {}

	std::vector<size_t> children;
	std::string name;
	Quatf rotation;
	size_t mesh;
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


// This is the following JSON object:
/*
{
	"index": 0,
	"texCoord": 0
}
*/
struct GLTFTextureObject
{
	GLTFTextureObject() : index(std::numeric_limits<size_t>::max()) {}

	bool valid() const { return index != std::numeric_limits<size_t>::max(); }

	size_t index;
	size_t texCoord;
};


struct GLTFMaterial : public RefCounted
{
	INDIGO_ALIGNED_NEW_DELETE

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
	AttributesPresent() : normal_present(false), texcoord_0_present(false) {}

	bool normal_present;
	bool texcoord_0_present;
};


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
	std::vector<GLTFSceneRef> scenes;
	size_t scene;

	AttributesPresent attr_present; // Attributes present on any of the meshes.
};


GLTFAccessor& getAccessor(GLTFData& data, size_t accessor_index)
{
	if(accessor_index >= data.accessors.size())
		throw Indigo::Exception("accessor_index out of bounds.");
	return *data.accessors[accessor_index];
}


GLTFAccessor& getAccessorForAttribute(GLTFData& data, GLTPrimitive& primitive, const std::string& attr_name)
{
	if(primitive.attributes.count(attr_name) == 0)
		throw Indigo::Exception("Expected " + attr_name + " attribute.");

	const size_t attribute_val = primitive.attributes[attr_name];
	if(attribute_val >= data.accessors.size())
		throw Indigo::Exception("attribute " + attr_name + " out of bounds.");

	return *data.accessors[attribute_val];
}


GLTFBufferView& getBufferView(GLTFData& data, size_t buffer_view_index)
{
	if(buffer_view_index >= data.buffer_views.size())
		throw Indigo::Exception("buffer_view_index out of bounds.");
	return *data.buffer_views[buffer_view_index];
}


GLTFBuffer& getBuffer(GLTFData& data, size_t buffer_index)
{
	if(buffer_index >= data.buffers.size())
		throw Indigo::Exception("buffer_index out of bounds.");
	return *data.buffers[buffer_index];
}


GLTFTexture& getTexture(GLTFData& data, size_t texture_index)
{
	if(texture_index >= data.textures.size())
		throw Indigo::Exception("texture_index out of bounds.");
	return *data.textures[texture_index];
}


GLTFImage& getImage(GLTFData& data, size_t image_index)
{
	if(image_index >= data.textures.size())
		throw Indigo::Exception("image_index out of bounds.");
	return *data.images[image_index];
}


Colour3f parseColour3ChildArrayWithDefault(const JSONParser& parser, const JSONNode& node, const std::string& name, const Colour3f& default_val)
{
	if(node.type != JSONNode::Type_Object)
		throw Indigo::Exception("Expected type object.");

	for(size_t i=0; i<node.name_val_pairs.size(); ++i)
		if(node.name_val_pairs[i].name == name)
		{
			const JSONNode& array_node = parser.nodes[node.name_val_pairs[i].value_node_index];
			checkNodeType(array_node, JSONNode::Type_Array);

			if(array_node.child_indices.size() != 3)
				throw Indigo::Exception("Expected 3 elements in array.");

			float v[3];
			for(size_t q=0; q<3; ++q)
				v[q] = (float)parser.nodes[array_node.child_indices[q]].getDoubleValue();

			return Colour3f(v[0], v[1], v[2]);
		}

	return default_val;
}


Colour4f parseColour4ChildArrayWithDefault(const JSONParser& parser, const JSONNode& node, const std::string& name, const Colour4f& default_val)
{
	if(node.type != JSONNode::Type_Object)
		throw Indigo::Exception("Expected type object.");

	for(size_t i=0; i<node.name_val_pairs.size(); ++i)
		if(node.name_val_pairs[i].name == name)
		{
			const JSONNode& array_node = parser.nodes[node.name_val_pairs[i].value_node_index];
			checkNodeType(array_node, JSONNode::Type_Array);

			if(array_node.child_indices.size() != 4)
				throw Indigo::Exception("Expected 4 elements in array.");
			
			float v[4];
			for(size_t q=0; q<4; ++q)
				v[q] = (float)parser.nodes[array_node.child_indices[q]].getDoubleValue();
			
			return Colour4f(v[0], v[1], v[2], v[3]);
		}

	return default_val;
}


GLTFTextureObject parseTextureIfPresent(const JSONParser& parser, const JSONNode& node, const std::string& name)
{
	if(node.hasChild(parser, name))
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


static const int GLTF_COMPONENT_TYPE_UNSIGNED_INT = 5125;
static const int GLTF_COMPONENT_TYPE_FLOAT = 5126;


//static const int GLTF_MODE_POINTS			= 0;
//static const int GLTF_MODE_LINES			= 1;
//static const int GLTF_MODE_LINE_LOOP		= 2;
//static const int GLTF_MODE_LINE_STRIP		= 3;
static const int GLTF_MODE_TRIANGLES		= 4;
//static const int GLTF_MODE_TRIANGLE_STRIP	= 5;
//static const int GLTF_MODE_TRIANGLE_FAN		= 6;


void processNode(GLTFData& data, GLTFNode& node, Indigo::Mesh& mesh_out)
{
	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		if(node.mesh >= data.meshes.size())
			throw Indigo::Exception("node mesh index out of bounds");

		GLTFMesh& mesh = *data.meshes[node.mesh];

		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTPrimitive& primitive = *mesh.primitives[i];

			if(primitive.mode != GLTF_MODE_TRIANGLES)
				throw Indigo::Exception("Only GLTF_MODE_TRIANGLES handled currently.");

			if(primitive.indices == std::numeric_limits<size_t>::max())
				throw Indigo::Exception("Primitve did not have indices..");

#if USE_INDIGO_MESH_INDICES
			size_t indices_write_i;
#else
			size_t tri_write_i;
#endif
			// if(primitive.indices != std::numeric_limits<size_t>::max())
			
			GLTFAccessor& index_accessor = getAccessor(data, primitive.indices);

			{
				GLTFBufferView& index_buf_view = getBufferView(data, index_accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, index_buf_view.buffer);

				const uint8* offset_base = buffer.binary_data + index_accessor.byte_offset + index_buf_view.byte_offset;

#if USE_INDIGO_MESH_INDICES
				mesh_out.chunks.push_back(Indigo::PrimitiveChunk());
				Indigo::PrimitiveChunk& chunk = mesh_out.chunks.back();
				chunk.indices_start = (uint32)mesh_out.indices.size();
				chunk.num_indices	= (uint32)index_accessor.count;
				chunk.mat_index		= (uint32)primitive.material;


				// Process indices:
				indices_write_i = mesh_out.indices.size();
				mesh_out.indices.resize(indices_write_i + index_accessor.count);
#else
				tri_write_i = mesh_out.triangles.size();
				mesh_out.triangles.resize(tri_write_i + index_accessor.count / 3);
#endif

				const uint32 vert_i_offset = (uint32)mesh_out.vert_positions.size();
				
				if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
#if USE_INDIGO_MESH_INDICES
					for(size_t z=0; z<index_accessor.count; ++z)
						mesh_out.indices[indices_write_i + z] = vert_i_offset + ((const uint32*)offset_base)[z];
#else
					for(size_t z=0; z<index_accessor.count / 3; ++z)
					{
						mesh_out.triangles[tri_write_i + z].vertex_indices[0]	= vert_i_offset + ((const uint32*)offset_base)[z*3 + 0];
						mesh_out.triangles[tri_write_i + z].vertex_indices[1]	= vert_i_offset + ((const uint32*)offset_base)[z*3 + 1];
						mesh_out.triangles[tri_write_i + z].vertex_indices[2]	= vert_i_offset + ((const uint32*)offset_base)[z*3 + 2];
						mesh_out.triangles[tri_write_i + z].uv_indices[0]		= vert_i_offset + ((const uint32*)offset_base)[z*3 + 0];
						mesh_out.triangles[tri_write_i + z].uv_indices[1]		= vert_i_offset + ((const uint32*)offset_base)[z*3 + 1];
						mesh_out.triangles[tri_write_i + z].uv_indices[2]		= vert_i_offset + ((const uint32*)offset_base)[z*3 + 2];
						mesh_out.triangles[tri_write_i + z].tri_mat_index = (uint32)primitive.material;

						//conPrint(toString(mesh_out.triangles[tri_write_i + z].vertex_indices[0]));
						//conPrint(toString(mesh_out.triangles[tri_write_i + z].vertex_indices[1]));
						//conPrint(toString(mesh_out.triangles[tri_write_i + z].vertex_indices[2]));
					}
#endif
				}
				else
					throw Indigo::Exception("unhandled component type.");
			}

			// Process vertex positions
			GLTFAccessor& vert_pos_accessor = getAccessorForAttribute(data, primitive, "POSITION");
			size_t pos_write_i;
			{
				
				GLTFBufferView& buf_view = getBufferView(data, vert_pos_accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const uint8* offset_base = buffer.binary_data + vert_pos_accessor.byte_offset + buf_view.byte_offset;
				const size_t byte_stride = buf_view.byte_stride;

				pos_write_i = mesh_out.vert_positions.size();
				mesh_out.vert_positions.resize(pos_write_i + vert_pos_accessor.count);

				if(vert_pos_accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<vert_pos_accessor.count; ++z)
					{
						mesh_out.vert_positions[pos_write_i + z].x = ((const float*)(offset_base + byte_stride * z))[0];
						mesh_out.vert_positions[pos_write_i + z].y = ((const float*)(offset_base + byte_stride * z))[1];
						mesh_out.vert_positions[pos_write_i + z].z = ((const float*)(offset_base + byte_stride * z))[2];

						//conPrint(toString(mesh_out.vert_positions[pos_write_i + z].x));
						//conPrint(toString(mesh_out.vert_positions[pos_write_i + z].y));
						//conPrint(toString(mesh_out.vert_positions[pos_write_i + z].z));
					}
				}
				else
					throw Indigo::Exception("unhandled component type.");
			}

			// Process vertex normals
			if(primitive.attributes.count("NORMAL"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "NORMAL");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const uint8* offset_base = buffer.binary_data + accessor.byte_offset + buf_view.byte_offset;
				const size_t byte_stride = buf_view.byte_stride;

				const size_t normals_write_i = mesh_out.vert_normals.size();
				mesh_out.vert_normals.resize(normals_write_i + accessor.count);

				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<accessor.count; ++z)
					{
						mesh_out.vert_normals[normals_write_i + z].x = ((const float*)(offset_base + byte_stride * z))[0];
						mesh_out.vert_normals[normals_write_i + z].y = ((const float*)(offset_base + byte_stride * z))[1];
						mesh_out.vert_normals[normals_write_i + z].z = ((const float*)(offset_base + byte_stride * z))[2];

						//conPrint(toString(mesh_out.vert_normals[normals_write_i + z].x));
						//conPrint(toString(mesh_out.vert_normals[normals_write_i + z].y));
						//conPrint(toString(mesh_out.vert_normals[normals_write_i + z].z));
					}
				}
				else
					throw Indigo::Exception("unhandled component type.");
			}
			else
			{
				if(data.attr_present.normal_present)
				{
					// Pad with normals.  This is a hack, needed because we only have one vertex layout per mesh.

					// NOTE: Should use geometric normals here

					const size_t normals_write_i = mesh_out.vert_normals.size();
					mesh_out.vert_normals.resize(normals_write_i + vert_pos_accessor.count);
					for(size_t z=0; z<vert_pos_accessor.count; ++z)
						mesh_out.vert_normals[normals_write_i + z].set(0, 0, 1);

#if USE_INDIGO_MESH_INDICES
					for(size_t z=0; z<index_accessor.count; z += 3) // For each tri (group of 3 indices)
					{
						uint32 v0i = mesh_out.indices[indices_write_i + z + 0];
						uint32 v1i = mesh_out.indices[indices_write_i + z + 1];
						uint32 v2i = mesh_out.indices[indices_write_i + z + 2];
#else
					for(size_t z=0; z<index_accessor.count/3; z++) // For each tri
					{
						uint32 v0i = mesh_out.triangles[tri_write_i + z].vertex_indices[0];
						uint32 v1i = mesh_out.triangles[tri_write_i + z].vertex_indices[1];
						uint32 v2i = mesh_out.triangles[tri_write_i + z].vertex_indices[2];
#endif

						Indigo::Vec3f v0 = mesh_out.vert_positions[v0i];
						Indigo::Vec3f v1 = mesh_out.vert_positions[v1i];
						Indigo::Vec3f v2 = mesh_out.vert_positions[v2i];

						Indigo::Vec3f normal = normalise(crossProduct(v1 - v0, v2 - v0));
						mesh_out.vert_normals[v0i] = normal;
						mesh_out.vert_normals[v1i] = normal;
						mesh_out.vert_normals[v2i] = normal;
					}
				}
			}

			// Process uvs
			if(primitive.attributes.count("TEXCOORD_0"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "TEXCOORD_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const uint8* offset_base = buffer.binary_data + accessor.byte_offset + buf_view.byte_offset;
				const size_t byte_stride = buf_view.byte_stride;

				const size_t uvs_write_i = mesh_out.uv_pairs.size();
				mesh_out.uv_pairs.resize(uvs_write_i + accessor.count);
				mesh_out.num_uv_mappings = 1;

				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					for(size_t z=0; z<accessor.count; ++z)
					{
						mesh_out.uv_pairs[uvs_write_i + z].x = ((const float*)(offset_base + byte_stride * z))[0];
						mesh_out.uv_pairs[uvs_write_i + z].y = ((const float*)(offset_base + byte_stride * z))[1];

						//conPrint(toString(mesh_out.uv_pairs[uvs_write_i + z].x));
						//conPrint(toString(mesh_out.uv_pairs[uvs_write_i + z].y));
					}
				}
				else
					throw Indigo::Exception("unhandled component type.");
			}
			else
			{
				if(data.attr_present.texcoord_0_present)
				{
					// Pad with UVs.  This is a bit of a hack, needed because we only have one vertex layout per mesh.
					const size_t uvs_write_i = mesh_out.uv_pairs.size();
					mesh_out.uv_pairs.resize(uvs_write_i + vert_pos_accessor.count);
					for(size_t z=0; z<vert_pos_accessor.count; ++z)
						mesh_out.uv_pairs[uvs_write_i + z].set(0, 0);
				}
			}
		}
	}


	// Process children
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw Indigo::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNode(data, child, mesh_out);
	}
}


void processMaterial(GLTFData& data, GLTFMaterial& mat, const std::string& gltf_folder, GLTFResultMaterial& mat_out)
{
	if(mat.pbrMetallicRoughness_present)
	{
		mat_out.diffuse = Colour3f(mat.baseColorFactor.x[0], mat.baseColorFactor.x[1], mat.baseColorFactor.x[2]);
		if(mat.baseColorTexture.valid())
		{
			GLTFTexture& texture = getTexture(data, mat.baseColorTexture.index);
			GLTFImage& image = getImage(data, texture.source);
			const std::string path = gltf_folder + "/" + image.uri;
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
			const std::string path = gltf_folder + "/" + image.uri;
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


void FormatDecoderGLTF::streamModel(const std::string& pathname, Indigo::Mesh& mesh, float scale, GLTFMaterials& mats_out)
{
	JSONParser parser;
	parser.parseFile(pathname);

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
				if(buffer_node.hasChild(parser, "uri")) buffer->uri = buffer_node.getChildStringValue(parser, "uri");
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
				image->uri = image_node.getChildStringValueWithDefaultVal(parser, "uri", "");
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


	// Load meshes	
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

					GLTPrimitiveRef primitive = new GLTPrimitive();

					const JSONNode& attributes_ob = primitive_node.getChildObject(parser, "attributes");
					for(size_t t=0; t<attributes_ob.name_val_pairs.size(); ++t)
						primitive->attributes[attributes_ob.name_val_pairs[t].name] =
							parser.nodes[attributes_ob.name_val_pairs[t].value_node_index].getUIntValue();

					primitive->indices	= primitive_node.getChildUIntValueWithDefaultVal(parser, "indices", 0);
					primitive->material = primitive_node.getChildUIntValueWithDefaultVal(parser, "material", 0);
					primitive->mode		= primitive_node.getChildUIntValueWithDefaultVal(parser, "mode", 4);

					gltf_mesh->primitives.push_back(primitive);
				}

				data.meshes.push_back(gltf_mesh);
			}
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
				if(node_node.hasChild(parser, "name")) node->name = node_node.getChildStringValue(parser, "name");
				if(node_node.hasChild(parser, "mesh")) node->mesh = node_node.getChildUIntValue(parser, "mesh");

				// parse children array
				if(node_node.hasChild(parser, "children"))
				{
					const JSONNode& children_array_node = node_node.getChildArray(parser, "children");
					for(size_t q=0; q<children_array_node.child_indices.size(); ++q)
					{
						node->children.push_back(parser.nodes[children_array_node.child_indices[q]].getUIntValue());
					}
				}

				if(node_node.hasChild(parser, "rotation"))
				{
					const JSONNode& rotation_node = node_node.getChildArray(parser, "rotation");
					if(rotation_node.child_indices.size() != 4)
						throw Indigo::Exception("Expected 4 elements in rotation array.");
					float v[4];
					for(size_t q=0; q<4; ++q)
						v[q] = (float)parser.nodes[rotation_node.child_indices[q]].getDoubleValue();
					node->rotation = Quatf(Vec4f(v[0], v[1], v[2], v[3]));
				}
				else
					node->rotation = Quatf::identity();

				if(node_node.hasChild(parser, "scale"))
				{
					const JSONNode& scale_node = node_node.getChildArray(parser, "scale");
					if(scale_node.child_indices.size() != 4)
						throw Indigo::Exception("Expected 4 elements in scale array.");
					float v[3];
					for(size_t q=0; q<3; ++q)
						v[q] = (float)parser.nodes[scale_node.child_indices[q]].getDoubleValue();
					node->scale = Vec3f(v[0], v[1], v[2]);
				}
				else
					node->scale = Vec3f(1.f);

				if(node_node.hasChild(parser, "translation"))
				{
					const JSONNode& translation_node = node_node.getChildArray(parser, "translation");
					if(translation_node.child_indices.size() != 4)
						throw Indigo::Exception("Expected 4 elements in translation array.");
					float v[3];
					for(size_t q=0; q<3; ++q)
						v[q] = (float)parser.nodes[translation_node.child_indices[q]].getDoubleValue();
					node->translation = Vec3f(v[0], v[1], v[2]);
				}
				else
					node->translation = Vec3f(0.f);


				data.nodes.push_back(node);
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

				if(mat_node.hasChild(parser, "name")) mat->name = mat_node.getChildStringValue(parser, "name");

				if(mat_node.hasChild(parser, "extensions"))
				{
					const JSONNode& extensions_node = mat_node.getChildObject(parser, "extensions");
					
					if(extensions_node.hasChild(parser, "KHR_materials_pbrSpecularGlossiness"))
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
				if(mat_node.hasChild(parser, "pbrMetallicRoughness"))
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

				if(scene_node.hasChild(parser, "name")) scene->name = scene_node.getChildStringValue(parser, "name");

				// parse nodes array
				if(scene_node.hasChild(parser, "nodes"))
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

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	// Load all buffers
	for(size_t i=0; i<data.buffers.size(); ++i)
	{
		const std::string path = gltf_base_dir + "/" + data.buffers[i]->uri;
		data.buffers[i]->file = new MemMappedFile(path);
		data.buffers[i]->binary_data = (const uint8*)data.buffers[i]->file->fileData();
	}


	// Process meshes to get the set of attributes used
	for(size_t i=0; i<data.meshes.size(); ++i)
	{
		for(size_t z=0; z<data.meshes[i]->primitives.size(); ++z)
		{
			if(data.meshes[i]->primitives[z]->attributes.count("NORMAL"))
				data.attr_present.normal_present = true;
			if(data.meshes[i]->primitives[z]->attributes.count("TEXCOORD_0"))
				data.attr_present.texcoord_0_present = true;
		}
	}



	// Process scene nodes

	// Get the scene to use
	if(data.scene >= data.scenes.size())
		throw Indigo::Exception("scene index out of bounds.");
	GLTFScene& scene_node = *data.scenes[data.scene];

	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw Indigo::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];
		processNode(data, root_node, mesh);
	}

	// Process materials
	mats_out.materials.resize(data.materials.size());
	for(size_t i=0; i<data.materials.size(); ++i)
	{
		processMaterial(data, *data.materials[i], gltf_base_dir, mats_out.materials[i]);
	}

	mesh.endOfModel();
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/ConPrint.h"


void FormatDecoderGLTF::test()
{
	conPrint("FormatDecoderGLTF::test()");

	try
	{
		const std::string path = "C:\\Users\\nick\\Downloads\\skull_downloadable\\scene.gltf";
		Indigo::Mesh mesh;
		GLTFMaterials mats;
		streamModel(path, mesh, 1.0, mats);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

}


#endif // BUILD_TESTS