/*=====================================================================
FormatDecoderVox.h
------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "../graphics/colour3.h"
#include "../graphics/Colour4f.h"
#include "../utils/Vector.h"
#include "../physics/jscol_aabbox.h"
#include <string>
#include <vector>


struct VoxMaterial
{
	VoxMaterial()
	:	type(Type_Diffuse),
		weight(1.f),
		roughness(0.5f),
		specular(0.f),
		IOR(1.5f),
		attenuation(0.f),
		flux(0.f)
	{}

	enum Type
	{
		Type_Diffuse = 0,
		Type_Metal = 1,
		Type_Glass = 2,
		Type_Emissive = 3
	};

	Type type;
	float weight;

	float roughness;
	float specular;
	float IOR;
	float attenuation;
	float flux; // Luminous flux?

	Colour4f col_from_palette; // Computed data.
};


struct VoxVoxel
{
	int x, y, z;
	int palette_index; // [1-255]

	int mat_index; // Computed data.  Index into used_materials.
};


struct VoxModel
{
	GLARE_ALIGNED_16_NEW_DELETE

	int size_x, size_y, size_z;
	std::vector<VoxVoxel> voxels;

	js::AABBox aabb; // Computed data.  AABB of model voxels (Assuming each voxel is 1 unit wide).
};


struct VoxFileContents
{
	int version;

	std::vector<VoxModel> models;

	js::Vector<Colour4f, 16> palette; // will have 256 elems

	std::vector<VoxMaterial> used_materials; // Computed data.


	// Internal:
	std::vector<VoxMaterial> internal_materials;
};


/*=====================================================================
FormatDecoderVox
----------------
See https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt for format details.

Updated format:
https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox-extension.txt
=====================================================================*/
class FormatDecoderVox
{
public:
	static void loadModel(const std::string& filename, VoxFileContents& contents_out); // Throws glare::Exception on failure.

	static void loadModelFromData(const uint8* data, const size_t datalen, VoxFileContents& contents_out); // Throws glare::Exception on failure.

	static bool isValidVoxFile(const std::string& filename);

	static void test();
};
