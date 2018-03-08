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
	Colour3f diffuse; // diffuse col
	GLTFResultMap diffuse_map;

	//float roughness;
	//GLTFMap roughness_map;
};


struct GLTFMaterials
{
	std::vector<GLTFResultMaterial> materials;
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

	static void test();
};
