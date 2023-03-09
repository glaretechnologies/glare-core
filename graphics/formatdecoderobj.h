/*=====================================================================
formatdecoderobj.h
------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../graphics/colour3.h"
#include <string>
#include <vector>
namespace Indigo { class Mesh; }


// See http://www.fileformat.info/format/material/
struct MTLTexMap
{
	MTLTexMap();

	std::string path;
	Vec3f origin; // -o
	Vec3f size; // -s
};


struct MTLMaterial
{
	MTLMaterial();

	std::string name;
	Colour3f Ka; // ambient col
	Colour3f Kd; // diffuse col
	Colour3f Ks; // specular col
	Colour3f Tf; // transmission col
	float Ns_exponent;
	float Ni_ior;
	float d_opacity;

	MTLTexMap map_Kd;
};


struct MLTLibMaterials
{
	std::string mtl_file_path;
	std::vector<MTLMaterial> materials;
};


/*=====================================================================
FormatDecoderObj
----------------

=====================================================================*/
class FormatDecoderObj
{
public:
	static void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale, bool parse_mtllib, MLTLibMaterials& mtllib_mats_out); // Throws glare::Exception on failure.

	// filename is used for finding .mtl file, if parse_mtllib is true.
	static void loadModelFromBuffer(const uint8* data, size_t len, const std::string& filename, Indigo::Mesh& handler, float scale, bool parse_mtllib, MLTLibMaterials& mtllib_mats_out); // Throws glare::Exception on failure.

	static void parseMTLLib(const std::string& filename, MLTLibMaterials& mtllib_mats_out);

	static void parseMTLLibFromBuffer(const uint8* data, size_t len, const std::string& filename, MLTLibMaterials& mtllib_mats_out);

	static void test();
};
