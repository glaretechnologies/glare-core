/*=====================================================================
FormatDecoderGLTF.h
-------------------
Copyright Glare Technologies Limited 2018 -
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

	float roughness;
	float metallic;
};


struct GLTFMaterials
{
	std::vector<GLTFResultMaterial> materials;
};


//struct GLTFNodeData
//{
//	int parent_index;
//};


//struct GLTFAnimationData : public RefCounted
//{
//	std::string name;
//	std::vector<float> time_vals;
//
//	// Translation for node n at input time index i
//	//	i * num_nodes + n						// is n * time_vals.size() + i
//	std::vector<Vec3f> translations;
//	std::vector<Quatf> rotations;
//	std::vector<Vec3f> scales;
//};
//
//
//struct GLTFAnimations
//{
//	std::vector<Reference<GLTFAnimationData> > animations;
//};


struct GLTFLoadedData
{
	//std::vector<GLTFNodeData> nodes;
	//std::vector<int> sorted_nodes; // Node indices sorted such that children always come after parents.

	GLTFMaterials materials;
	//GLTFAnimations animations;
	AnimationData anim_data;
};


struct GLTFWriteOptions
{
	GLTFWriteOptions() : write_vert_normals(true) {}

	bool write_vert_normals; // Write vertex normals, if present in mesh.
};


/*=====================================================================
FormatDecoderGLTF
-----------------
See https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md

Current limitations:

Don't support non-skinned animations.
=====================================================================*/
class FormatDecoderGLTF
{
public:

	static Reference<BatchedMesh> loadGLBFile(const std::string& filename, float scale,
		GLTFLoadedData& data_out); // throws glare::Exception on failure

	static Reference<BatchedMesh> loadGLTFFile(const std::string& filename, float scale,
		GLTFLoadedData& data_out); // throws glare::Exception on failure

	static void writeToDisk(const Indigo::Mesh& mesh, const std::string& path, const GLTFWriteOptions& options, const GLTFLoadedData& data); // throws glare::Exception on failure

	static void test();

private:
	static Reference<BatchedMesh> loadGivenJSON(JSONParser& parser, const std::string gltf_base_dir, const Reference<GLTFBuffer>& glb_bin_buffer, float scale,
		GLTFLoadedData& data_out); // throws glare::Exception on failure

};
