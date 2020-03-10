/*=====================================================================
FormatDecoderGLTF.h
-------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "../graphics/colour3.h"
#include <string>
#include <vector>
namespace Indigo { class Mesh; }


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


struct GLTFWriteOptions
{
	GLTFWriteOptions() : write_vert_normals(true) {}

	bool write_vert_normals; // Write vertex normals, if present in mesh.
};


/*=====================================================================
FormatDecoderGLTF
-----------------
See https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md
=====================================================================*/
class FormatDecoderGLTF
{
public:
	static void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale,
		GLTFMaterials& mats_out); // throws Indigo::Exception on failure

	static void writeToDisk(const Indigo::Mesh& mesh, const std::string& path, const GLTFWriteOptions& options, const GLTFMaterials& mats); // throws Indigo::Exception on failure

	static void test();
};
