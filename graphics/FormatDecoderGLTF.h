/*=====================================================================
FormatDecoderGLTF.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include "../graphics/colour3.h"
#include "../graphics/AnimationData.h"
#include "../maths/Quat.h"
#include <string>
#include <vector>
namespace Indigo { class Mesh; }
class JSONParser;
struct GLTFBuffer;
class BatchedMesh;


struct GLTFResultMap
{
	std::string path;
	Vec3f origin;
	Vec3f size;
};


struct GLTFResultMaterial
{
	GLTFResultMaterial() : roughness(0.5f), metallic(0.f), alpha(1.f) {}

	Colour3f diffuse; // diffuse col.
	float alpha;
	GLTFResultMap diffuse_map;
	GLTFResultMap metallic_roughness_map;

	Colour3f emissive_factor; // sRGB, usually in [0, 1].
	GLTFResultMap emissive_map;

	float roughness;
	float metallic;
};


struct GLTFMaterials
{
	std::vector<GLTFResultMaterial> materials;
};


struct GLTFLoadedData
{
	GLTFMaterials materials;
};


struct GLTFWriteOptions
{
	GLTFWriteOptions()/* : write_vert_normals(true) */{}

	//bool write_vert_normals; // Write vertex normals, if present in mesh.
};


/*=====================================================================
FormatDecoderGLTF
-----------------
See https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md

Current limitations:

May duplicate vertices when loading them if the same vertices are referenced from multiple primitives.
Doesn't support morph targets.
Doesn't support non-skinned animations.

TODO: when loading multiple primitives using the same vertex accessors, don't duplicate verts.
=====================================================================*/
class FormatDecoderGLTF
{
public:

	static Reference<BatchedMesh> loadGLBFile(const std::string& filename, GLTFLoadedData& data_out); // throws glare::Exception on failure

	static Reference<BatchedMesh> loadGLTFFile(const std::string& filename, GLTFLoadedData& data_out); // throws glare::Exception on failure

	static void writeBatchedMeshToGLTFFile(const BatchedMesh& mesh, const std::string& path, const GLTFWriteOptions& options); // throws glare::Exception on failure

	static void writeBatchedMeshToGLBFile(const BatchedMesh& mesh, const std::string& path, const GLTFWriteOptions& options); // throws glare::Exception on failure

	static void test();

	// For fuzz testing:
	static Reference<BatchedMesh> loadGLBFileFromData(const void* data, const size_t datalen, const std::string& gltf_base_dir, bool write_images_to_disk, GLTFLoadedData& data_out);
private:
	static Reference<BatchedMesh> loadGivenJSON(JSONParser& parser, const std::string gltf_base_dir, const Reference<GLTFBuffer>& glb_bin_buffer, bool write_images_to_disk,
		GLTFLoadedData& data_out); // throws glare::Exception on failure

	static void makeGLTFJSONAndBin(const BatchedMesh& mesh, const std::string& bin_path, std::string& json_out, js::Vector<uint8, 16>& bin_out);
};
