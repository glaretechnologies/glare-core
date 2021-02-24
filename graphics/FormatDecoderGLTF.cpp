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
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/IncludeXXHash.h"
#include "../maths/Quat.h"
#include "../graphics/Colour4f.h"
#include <assert.h>
#include <vector>
#include <map>
#include <fstream>


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
	size_t byte_stride;
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
	GLARE_ALIGNED_16_NEW_DELETE

	GLTFNode() : mesh(std::numeric_limits<size_t>::max()) {}

	std::vector<size_t> children;
	std::string name;
	
	Matrix4f matrix;
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
	AttributesPresent() : normal_present(false), vert_col_present(false), texcoord_0_present(false) {}

	bool normal_present;
	bool vert_col_present;
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


static GLTFAccessor& getAccessorForAttribute(GLTFData& data, GLTPrimitive& primitive, const std::string& attr_name)
{
	if(primitive.attributes.count(attr_name) == 0)
		throw glare::Exception("Expected " + attr_name + " attribute.");

	const size_t attribute_val = primitive.attributes[attr_name];
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


//static const int GLTF_COMPONENT_TYPE_BYTE			= 5120;
//static const int GLTF_COMPONENT_TYPE_UNSIGNED_BYTE= 5121;
//static const int GLTF_COMPONENT_TYPE_SHORT		= 5122;
static const int GLTF_COMPONENT_TYPE_UNSIGNED_SHORT = 5123;
static const int GLTF_COMPONENT_TYPE_UNSIGNED_INT	= 5125;
static const int GLTF_COMPONENT_TYPE_FLOAT			= 5126;


//static const int GLTF_MODE_POINTS			= 0;
//static const int GLTF_MODE_LINES			= 1;
//static const int GLTF_MODE_LINE_LOOP		= 2;
//static const int GLTF_MODE_LINE_STRIP		= 3;
static const int GLTF_MODE_TRIANGLES		= 4;
//static const int GLTF_MODE_TRIANGLE_STRIP	= 5;
//static const int GLTF_MODE_TRIANGLE_FAN	= 6;



static void appendDataToMeshVector(GLTFData& data, GLTPrimitive& primitive, const std::string& attr_name, const Matrix4f& transform, Indigo::Vector<Indigo::Vec3f>& data_out)
{
	GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, attr_name);
	GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
	GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

	const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
	const uint8* offset_base = buffer.binary_data + offset_B;
	const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec3f);

	const size_t write_i = data_out.size();
	data_out.resize(write_i + accessor.count);

	if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
	{
		if(byte_stride < sizeof(float)*3)
			throw glare::Exception("Invalid stride for buffer view: " + toString(byte_stride) + " B");
		if(offset_B + byte_stride * (accessor.count - 1) + sizeof(float)*3 > buffer.data_size)
			throw glare::Exception("Out of bounds while trying to read '" + attr_name + "'");

		for(size_t z=0; z<accessor.count; ++z)
		{
			const Vec4f v(
				((const float*)(offset_base + byte_stride * z))[0],
				((const float*)(offset_base + byte_stride * z))[1],
				((const float*)(offset_base + byte_stride * z))[2],
				1);

			const Vec4f v_primed = transform * v;

			data_out[write_i + z].x = v_primed[0];
			data_out[write_i + z].y = v_primed[1];
			data_out[write_i + z].z = v_primed[2];
		}
	}
	else
		throw glare::Exception("unhandled component type.");
}


static void processNodeToGetMeshCapacity(GLTFData& data, GLTFNode& node, size_t& total_num_tris, size_t& total_num_verts)
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

			total_num_tris  += getAccessor(data, primitive.indices).count / 3;
			total_num_verts += getAccessorForAttribute(data, primitive, "POSITION").count;
		}
	}

	// Process children
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw glare::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNodeToGetMeshCapacity(data, child, total_num_tris, total_num_verts);
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


static void processNode(GLTFData& data, GLTFNode& node, const Matrix4f& parent_transform, Indigo::Mesh& mesh_out)
{
	const Matrix4f trans = Matrix4f::translationMatrix(node.translation.x, node.translation.y, node.translation.z);
	const Matrix4f rot = node.rotation.toMatrix();
	const Matrix4f scale = Matrix4f::scaleMatrix(node.scale.x, node.scale.y, node.scale.z);
	const Matrix4f node_transform = parent_transform * node.matrix * trans * rot * scale; // Matrix and T,R,S transforms should be mutually exclusive in GLTF files.  Just multiply them together however.

	// Process mesh
	if(node.mesh != std::numeric_limits<size_t>::max())
	{
		GLTFMesh& mesh = getMesh(data, node.mesh);

		for(size_t i=0; i<mesh.primitives.size(); ++i)
		{
			GLTPrimitive& primitive = *mesh.primitives[i];

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

				const size_t offset_B = index_accessor.byte_offset + index_buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;

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
				
				if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					if(offset_B + index_accessor.count * sizeof(uint16) > buffer.data_size)
						throw glare::Exception("Out of bounds while trying to read indices");

#if USE_INDIGO_MESH_INDICES
					for(size_t z=0; z<index_accessor.count; ++z)
						mesh_out.indices[indices_write_i + z] = vert_i_offset + ((const uint16*)offset_base)[z];
#else
					for(size_t z=0; z<index_accessor.count / 3; ++z)
					{
						mesh_out.triangles[tri_write_i + z].vertex_indices[0]	= vert_i_offset + ((const uint16*)offset_base)[z*3 + 0];
						mesh_out.triangles[tri_write_i + z].vertex_indices[1]	= vert_i_offset + ((const uint16*)offset_base)[z*3 + 1];
						mesh_out.triangles[tri_write_i + z].vertex_indices[2]	= vert_i_offset + ((const uint16*)offset_base)[z*3 + 2];
						mesh_out.triangles[tri_write_i + z].uv_indices[0]		= vert_i_offset + ((const uint16*)offset_base)[z*3 + 0];
						mesh_out.triangles[tri_write_i + z].uv_indices[1]		= vert_i_offset + ((const uint16*)offset_base)[z*3 + 1];
						mesh_out.triangles[tri_write_i + z].uv_indices[2]		= vert_i_offset + ((const uint16*)offset_base)[z*3 + 2];
						mesh_out.triangles[tri_write_i + z].tri_mat_index = (uint32)primitive.material;
					}
#endif
				}
				else if(index_accessor.component_type == GLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					if(offset_B + index_accessor.count * sizeof(uint32) > buffer.data_size)
						throw glare::Exception("Out of bounds while trying to read indices");

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
					}
#endif
				}
				else
					throw glare::Exception("Unhandled index accessor component type.");
			}

			// Process vertex positions
			size_t vert_pos_count;
			{
				const size_t initial_vert_pos_size = mesh_out.vert_positions.size();

				appendDataToMeshVector(data, primitive, "POSITION", node_transform, mesh_out.vert_positions);

				vert_pos_count = mesh_out.vert_positions.size() - initial_vert_pos_size;
			}

			// Process vertex normals
			if(primitive.attributes.count("NORMAL"))
			{
				Matrix4f normal_transform;
				node_transform.getUpperLeftInverseTranspose(normal_transform);
				appendDataToMeshVector(data, primitive, "NORMAL", normal_transform, mesh_out.vert_normals);
			}
			else
			{
				if(data.attr_present.normal_present)
				{
					// Pad with normals.  This is a hack, needed because we only have one vertex layout per mesh.

					// NOTE: Should use geometric normals here

					const size_t normals_write_i = mesh_out.vert_normals.size();
					mesh_out.vert_normals.resize(normals_write_i + vert_pos_count);
					for(size_t z=0; z<vert_pos_count; ++z)
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

			// Process vertex colours
			if(primitive.attributes.count("COLOR_0"))
			{
				appendDataToMeshVector(data, primitive, "COLOR_0", Matrix4f::identity(), mesh_out.vert_colours);
			}
			else
			{
				if(data.attr_present.vert_col_present)
				{
					// Pad with colours.  This is a hack, needed because we only have one vertex layout per mesh.
					const size_t colours_write_i = mesh_out.vert_colours.size();
					mesh_out.vert_colours.resize(colours_write_i + vert_pos_count);
					for(size_t z=0; z<vert_pos_count; ++z)
						mesh_out.vert_colours[colours_write_i + z].set(1, 1, 1);
				}
			}

			// Process uvs
			if(primitive.attributes.count("TEXCOORD_0"))
			{
				GLTFAccessor& accessor = getAccessorForAttribute(data, primitive, "TEXCOORD_0");
				GLTFBufferView& buf_view = getBufferView(data, accessor.buffer_view);
				GLTFBuffer& buffer = getBuffer(data, buf_view.buffer);

				const size_t offset_B = accessor.byte_offset + buf_view.byte_offset; // Offset in bytes from start of buffer to the data we are accessing.
				const uint8* offset_base = buffer.binary_data + offset_B;
				const size_t byte_stride = (buf_view.byte_stride != 0) ? buf_view.byte_stride : sizeof(Indigo::Vec2f);

				const size_t uvs_write_i = mesh_out.uv_pairs.size();
				mesh_out.uv_pairs.resize(uvs_write_i + accessor.count);
				mesh_out.num_uv_mappings = 1;

				if(accessor.component_type == GLTF_COMPONENT_TYPE_FLOAT)
				{
					if(byte_stride < sizeof(float)*2)
						throw glare::Exception("Invalid stride for buffer view: " + toString(byte_stride) + " B");
					if(offset_B + byte_stride * (accessor.count - 1) + sizeof(float)*2 > buffer.data_size)
						throw glare::Exception("Out of bounds while trying to read 'TEXCOORD_0'");

					for(size_t z=0; z<accessor.count; ++z)
					{
						mesh_out.uv_pairs[uvs_write_i + z].x = ((const float*)(offset_base + byte_stride * z))[0];
						mesh_out.uv_pairs[uvs_write_i + z].y = ((const float*)(offset_base + byte_stride * z))[1];
					}
				}
				else
					throw glare::Exception("unhandled component type.");
			}
			else
			{
				if(data.attr_present.texcoord_0_present)
				{
					// Pad with UVs.  This is a bit of a hack, needed because we only have one vertex layout per mesh.
					const size_t uvs_write_i = mesh_out.uv_pairs.size();
					mesh_out.uv_pairs.resize(uvs_write_i + vert_pos_count);
					for(size_t z=0; z<vert_pos_count; ++z)
						mesh_out.uv_pairs[uvs_write_i + z].set(0, 0);
				}
			}
		} // End for each primitive batch
	} // End if(node.mesh != std::numeric_limits<size_t>::max())


	// Process child nodes
	for(size_t i=0; i<node.children.size(); ++i)
	{
		if(node.children[i] >= data.nodes.size())
			throw glare::Exception("node child index out of bounds");

		GLTFNode& child = *data.nodes[node.children[i]];

		processNode(data, child, node_transform, mesh_out);
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


void FormatDecoderGLTF::loadGLBFile(const std::string& pathname, Indigo::Mesh& mesh, float scale,
	GLTFMaterials& mats_out) // throws glare::Exception on failure
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

	if(false)
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

	loadGivenJSON(parser, gltf_base_dir, buffer, mesh, scale, mats_out);
}


void FormatDecoderGLTF::streamModel(const std::string& pathname, Indigo::Mesh& mesh, float scale, GLTFMaterials& mats_out)
{
	JSONParser parser;
	parser.parseFile(pathname);

	const std::string gltf_base_dir = FileUtils::getDirectory(pathname);

	loadGivenJSON(parser, gltf_base_dir, /*glb_bin_buffer=*/NULL, mesh, scale, mats_out);
}


void FormatDecoderGLTF::loadGivenJSON(JSONParser& parser, const std::string gltf_base_dir, const GLTFBufferRef& glb_bin_buffer, Indigo::Mesh& mesh, float scale,
	GLTFMaterials& mats_out) // throws glare::Exception on failure
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
	size_t total_num_tris = 0;
	size_t total_num_verts = 0;
	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		processNodeToGetMeshCapacity(data, root_node, total_num_tris, total_num_verts);
	}

	// Reserve the space in the Indigo Mesh
	mesh.triangles.reserve(total_num_tris);
	mesh.vert_positions.reserve(total_num_verts);
	if(data.attr_present.normal_present)     mesh.vert_normals.reserve(total_num_verts);
	if(data.attr_present.vert_col_present)   mesh.vert_colours.reserve(total_num_verts);
	if(data.attr_present.texcoord_0_present) mesh.uv_pairs.reserve(total_num_verts);


	for(size_t i=0; i<scene_node.nodes.size(); ++i)
	{
		if(scene_node.nodes[i] >= data.nodes.size())
			throw glare::Exception("scene root node index out of bounds.");
		GLTFNode& root_node = *data.nodes[scene_node.nodes[i]];

		Matrix4f current_transform = Matrix4f::identity();

		processNode(data, root_node, current_transform, mesh);
	}

	// Check we reserved the right amount of space.
	assert(mesh.vert_positions.size()     == total_num_verts);
	assert(mesh.vert_positions.capacity() == total_num_verts);
	assert(mesh.vert_normals.size()       == mesh.vert_normals.capacity());
	assert(mesh.vert_colours.size()       == mesh.vert_colours.capacity());
	assert(mesh.uv_pairs.size()           == mesh.uv_pairs.capacity());

	assert(mesh.triangles.size()          == total_num_tris);
	assert(mesh.triangles.capacity()      == total_num_tris);


	// Process images - for any image embedded in the GLB file data, save onto disk in a temp location
	for(size_t i=0; i<data.images.size(); ++i)
	{
		processImage(data, *data.images[i], gltf_base_dir);
	}

	// Process materials
	mats_out.materials.resize(data.materials.size());
	for(size_t i=0; i<data.materials.size(); ++i)
	{
		processMaterial(data, *data.materials[i], gltf_base_dir, mats_out.materials[i]);
	}

	mesh.endOfModel();
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


void FormatDecoderGLTF::writeToDisk(const Indigo::Mesh& mesh, const std::string& path, const GLTFWriteOptions& options, const GLTFMaterials& mats)
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

	for(size_t i=0; i<mats.materials.size(); ++i)
	{
		file <<
			"	" + ((i > 0) ? std::string(",") : std::string(""))  + "{\n"
			"		\"pbrMetallicRoughness\": {\n";

		if(!mats.materials[i].diffuse_map.path.empty())
		{
			file <<
				"			\"baseColorTexture\": {\n"
				"				\"index\": " + toString(textures.size()) + "\n"
				"			},\n";

			textures.push_back(mats.materials[i].diffuse_map);
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


static void testWriting(const Indigo::Mesh& mesh, const GLTFMaterials& mats)
{
/*	try
	{
		const std::string path = PlatformUtils::getTempDirPath() + "/mesh_" + toString(mesh.checksum()) + ".gltf";

		// Write it back to disk
		GLTFWriteOptions options;
		//options.write_vert_normals = false;
		//FormatDecoderGLTF::writeToDisk(mesh, path, options, mats);

		options.write_vert_normals = true;
		FormatDecoderGLTF::writeToDisk(mesh, path, options, mats);

		// Load again
		Indigo::Mesh mesh2;
		GLTFMaterials mats2;
		FormatDecoderGLTF::streamModel(path, mesh2, 1.0, mats2);

		// Check num vertices etc.. is the same
		testAssert(mesh2.num_materials_referenced	== mesh.num_materials_referenced);
		testAssert(mesh2.vert_positions.size()		== mesh.vert_positions.size());
		testAssert(mesh2.vert_normals.size()		== mesh.vert_normals.size());
		testAssert(mesh2.vert_colours.size()		== mesh.vert_colours.size());
		testAssert(mesh2.uv_pairs.size()			== mesh.uv_pairs.size());
		testAssert(mesh2.triangles.size()			== mesh.triangles.size());
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}*/
}


void FormatDecoderGLTF::test()
{
	conPrint("FormatDecoderGLTF::test()");

	try
	{
		{
			conPrint("---------------------------------Box.glb-----------------------------------");
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/Box.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 24);
			testAssert(mesh.vert_normals.size() == 24);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 0);
			testAssert(mesh.triangles.size() == 12);

			testWriting(mesh, mats);
		}
	
		{
			conPrint("---------------------------------BoxInterleaved.glb-----------------------------------");
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxInterleaved.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 24);
			testAssert(mesh.vert_normals.size() == 24);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 0);
			testAssert(mesh.triangles.size() == 12);

			testWriting(mesh, mats);
		}

		{
			conPrint("---------------------------------BoxTextured.glb-----------------------------------");
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxTextured.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 24);
			testAssert(mesh.vert_normals.size() == 24);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 24);
			testAssert(mesh.triangles.size() == 12);

			testWriting(mesh, mats);
		}
	
		{
			conPrint("---------------------------------BoxVertexColors.glb-----------------------------------");
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/BoxVertexColors.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 24);
			testAssert(mesh.vert_normals.size() == 24);
			testAssert(mesh.vert_colours.size() == 24);
			testAssert(mesh.uv_pairs.size() == 24);
			testAssert(mesh.triangles.size() == 12);

			testWriting(mesh, mats);
		}
	
		{
			conPrint("---------------------------------2CylinderEngine.glb-----------------------------------");
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/2CylinderEngine.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 34);
			testAssert(mesh.vert_positions.size() == 84657);
			testAssert(mesh.vert_normals.size() == 84657);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 0);
			testAssert(mesh.triangles.size() == 121496);

			testWriting(mesh, mats);
		}

		{
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/RiggedFigure.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 370);
			testAssert(mesh.vert_normals.size() == 370);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 0);
			testAssert(mesh.triangles.size() == 256);

			testWriting(mesh, mats);
		}
	
		{
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/CesiumMan.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 1);
			testAssert(mesh.vert_positions.size() == 3273);
			testAssert(mesh.vert_normals.size() == 3273);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 3273);
			testAssert(mesh.triangles.size() == 4672);

			testWriting(mesh, mats);
		}
	
		{
			Indigo::Mesh mesh;
			GLTFMaterials mats;
			loadGLBFile(TestUtils::getTestReposDir() + "/testfiles/gltf/MetalRoughSpheresNoTextures.glb", mesh, 1.0, mats);

			testAssert(mesh.num_materials_referenced == 98);
			testAssert(mesh.vert_positions.size() == 528291);
			testAssert(mesh.vert_normals.size() == 528291);
			testAssert(mesh.vert_colours.size() == 0);
			testAssert(mesh.uv_pairs.size() == 0);
			testAssert(mesh.triangles.size() == 1040409);

			testWriting(mesh, mats);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	/*try
	{
		// Read a GLTF file from disk
		const std::string path = "D:\\downloads\\Avocado.glb";
		Indigo::Mesh mesh;
		GLTFMaterials mats;
		loadGLBFile(path, mesh, 1.0, mats);

		testAssert(mesh.num_materials_referenced == 1);
		testAssert(mesh.vert_positions.size() == 406);
		testAssert(mesh.vert_normals.size() == 406);
		testAssert(mesh.vert_colours.size() == 0);
		testAssert(mesh.uv_pairs.size() == 406);
		testAssert(mesh.triangles.size() == 682);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}*/

	try
	{
		// Read a GLTF file from disk
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/gltf/duck/Duck.gltf";
		Indigo::Mesh mesh;
		GLTFMaterials mats;
		streamModel(path, mesh, 1.0, mats);

		testAssert(mesh.num_materials_referenced == 1);
		testAssert(mesh.vert_positions.size() == 2399);
		testAssert(mesh.vert_normals.size() == 2399);
		testAssert(mesh.vert_colours.size() == 0);
		testAssert(mesh.uv_pairs.size() == 2399);
		testAssert(mesh.triangles.size() == 4212);


		// Write it back to disk
		GLTFWriteOptions options;
		options.write_vert_normals = false;
		writeToDisk(mesh, PlatformUtils::getTempDirPath() + "/duck_new.gltf", options, mats);

		options.write_vert_normals = true;
		writeToDisk(mesh, PlatformUtils::getTempDirPath() + "/duck_new.gltf", options, mats);


		// Load again
		Indigo::Mesh mesh2;
		GLTFMaterials mats2;
		streamModel(path, mesh2, 1.0, mats2);

		testAssert(mesh2.num_materials_referenced == 1);
		testAssert(mesh2.vert_positions.size() == 2399);
		testAssert(mesh2.vert_normals.size() == 2399);
		testAssert(mesh2.vert_colours.size() == 0);
		testAssert(mesh2.uv_pairs.size() == 2399);
		testAssert(mesh2.triangles.size() == 4212);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	/*try
	{
		// Read a GLTF file from disk
		const std::string path = "C:\\programming\\indigo\\output\\vs2019\\indigo_x64\\RelWithDebInfo\\baked_indirect_only.gltf";
		Indigo::Mesh mesh;
		GLTFMaterials mats;
		streamModel(path, mesh, 1.0, mats);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	conPrint("FormatDecoderGLTF::test() done.");*/
}


#endif // BUILD_TESTS
