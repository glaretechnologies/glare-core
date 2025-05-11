/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/StringUtils.h"
#include <string>
#include <vector>
class OpenGLShader;


struct UniformLocations
{
	UniformLocations();

	int diffuse_tex_location;
	int metallic_roughness_tex_location;
	int emission_tex_location;
	int backface_albedo_tex_location;
	int transmission_tex_location;
	int normal_map_location;
	int combined_array_tex_location;
	int cosine_env_tex_location;
	int specular_env_tex_location;
	int lightmap_tex_location;
	int fbm_tex_location;
	int cirrus_tex_location; // Just for water reflection of cirrus
	int aurora_tex_location;
	int ssao_output_tex_location;
	int ssao_specular_output_tex_location;
	int detail_tex_0_location;
	int detail_tex_1_location;
	int detail_tex_2_location;
	int detail_tex_3_location;
	int detail_heightmap_0_location;
	int blue_noise_tex_location;
	int main_colour_texture_location;
	int main_normal_texture_location;
	int main_depth_texture_location;
	int caustic_tex_a_location;
	int caustic_tex_b_location;
	int water_colour_texture_location;
	int snow_ice_normal_map_location;
	
	int dynamic_depth_tex_location;
	int static_depth_tex_location;
	int shadow_texture_matrix_location;

	int num_blob_positions_location;
	int blob_positions_location;
};


struct UserUniformInfo
{
	enum UniformType
	{
		UniformType_Vec2,
		UniformType_Vec3,
		UniformType_Int,
		UniformType_Float,
		UniformType_Sampler2D
	};

	UserUniformInfo() {}
	UserUniformInfo(int loc_, int index_, UniformType uniform_type_) : loc(loc_), index(index_), uniform_type(uniform_type_) {}

	int loc; // Location in shader as retrieved from glGetUniformLocation()
	int index; // Index into material.user_uniform_vals
	UniformType uniform_type;
};


// Options that affect the compilation of programs.  The combined options are used as a key to look up programs.
struct ProgramKeyArgs
{
	ProgramKeyArgs() : alpha_test(false), vert_colours(false), instance_matrices(false), lightmapping(false), gen_planar_uvs(false), draw_planar_uv_grid(false), skinning(false),
		imposter(false), imposterable(false), use_wind_vert_shader(false), simple_double_sided(false), materialise_effect(false), geomorphing(false), terrain(false), decal(false), participating_media(false), 
		vert_tangents(false), sdf_text(false), fancy_double_sided(false), combined(false), position_w_is_oct16_normal(false)
	{}

	bool alpha_test, vert_colours, instance_matrices, lightmapping, gen_planar_uvs, draw_planar_uv_grid, skinning, imposter, imposterable, 
		use_wind_vert_shader, simple_double_sided, materialise_effect, geomorphing, terrain, decal, participating_media, vert_tangents, sdf_text, fancy_double_sided, combined, position_w_is_oct16_normal;
};


struct ProgramKey
{
	ProgramKey(const std::string& program_name_, const ProgramKeyArgs& args)
	:	program_name(program_name_)
	{
		alpha_test					= args.alpha_test;
		vert_colours				= args.vert_colours;
		instance_matrices			= args.instance_matrices;
		lightmapping				= args.lightmapping;
		gen_planar_uvs				= args.gen_planar_uvs;
		draw_planar_uv_grid			= args.draw_planar_uv_grid;
		skinning					= args.skinning;
		imposter					= args.imposter;
		imposterable				= args.imposterable;
		use_wind_vert_shader		= args.use_wind_vert_shader;
		simple_double_sided			= args.simple_double_sided;
		materialise_effect			= args.materialise_effect;
		geomorphing					= args.geomorphing;
		terrain						= args.terrain;
		decal						= args.decal;
		participating_media			= args.participating_media;
		vert_tangents				= args.vert_tangents;
		sdf_text					= args.sdf_text;
		fancy_double_sided			= args.fancy_double_sided;
		combined					= args.combined;
		position_w_is_oct16_normal	= args.position_w_is_oct16_normal;

		rebuildKeyVal();
	}

	const std::string description() const { return "alpha_test: " + toString(alpha_test) + ", vert_colours: " + toString(vert_colours) + ", instance_matrices: " + toString(instance_matrices) + 
		", lightmapping: " + toString(lightmapping) + ", gen_planar_uvs: " + toString(gen_planar_uvs) + ", draw_planar_uv_grid_: " + toString(draw_planar_uv_grid) + 
		", skinning: " + toString(skinning) + ", imposter: " + toString(imposter) + ", imposterable: " + toString(imposterable) + ", use_wind_vert_shader: " + toString(use_wind_vert_shader) + 
		", simple_double_sided: " + toString(simple_double_sided) + ", materialise_effect: " + toString(materialise_effect) + ", geomorphing: " + toString(geomorphing) + ", terrain: " + toString(terrain) + ", decal: " + toString(decal) +
		", participating_media: " + toString(participating_media) + ", vert_tangents: " + toString(vert_tangents) + ", sdf_text: " + toString(sdf_text) + ", fancy_double_sided: " + toString(fancy_double_sided) + ", combined: " + toString(combined) +
		", position_w_is_oct16_normal: " + toString(position_w_is_oct16_normal); }

	std::string program_name;

	// NOTE: if changing any of these fields, need to call rebuildKeyVal() afterwards.
	bool alpha_test, vert_colours, instance_matrices, lightmapping, gen_planar_uvs, draw_planar_uv_grid, skinning, imposter, 
		imposterable, use_wind_vert_shader, simple_double_sided, materialise_effect, geomorphing, terrain, decal, participating_media, vert_tangents, sdf_text, fancy_double_sided, combined, position_w_is_oct16_normal;

	void rebuildKeyVal()
	{
		keyval = (alpha_test   ? 1 : 0) | (vert_colours   ? 2 : 0) | (  instance_matrices ? 4 : 0) | (  lightmapping ? 8 : 0) | (  gen_planar_uvs ? 16 : 0) | (  draw_planar_uv_grid ? 32 : 0) |
			(  skinning ? 128 : 0) | (  imposterable ? 256 : 0) | (  use_wind_vert_shader ? 512 : 0) | (  simple_double_sided ? 1024 : 0) | (  materialise_effect ? 2048 : 0) | (  geomorphing ? 4096 : 0) | (terrain ? 8192 : 0) | 
			(imposter ? 16384 : 0) | (decal ? 32768 : 0) | (participating_media ? 65536 : 0) | (vert_tangents ? 131072 : 0) | (sdf_text ? 262144 : 0) | (fancy_double_sided ? 524288 : 0) | (combined ? 1048576 : 0) | 
			(position_w_is_oct16_normal ? 2097152 : 0);
	}

	uint32 keyval;

	inline bool operator < (const ProgramKey& b) const
	{
		if(program_name < b.program_name)
			return true;
		else if(program_name > b.program_name)
			return false;
		return keyval < b.keyval;
	}
};


/*=====================================================================
OpenGLProgram
-------------
=====================================================================*/
class OpenGLProgram : public RefCounted
{
public:
	OpenGLProgram(const std::string& prog_name, const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader, uint32 program_index, bool wait_for_build_to_complete);
	~OpenGLProgram();

	bool checkLinkingDone(); // Returns true if compilation and linking is done.

	void forceFinishLinkAndDoPostLinkCode(); // May throw if build failed.  If doesn't throw, isBuilt() is true afterwards.

	bool isBuilt() const { return built_successfully; }



	void useProgram() const;

	static void useNoPrograms();

	int getUniformLocation(const std::string& name);

	int getAttributeLocation(const std::string& name);

	void bindAttributeLocation(int index, const std::string& name);

	void appendUserUniformInfo(UserUniformInfo::UniformType uniform_type, const std::string& name);

	/*GLuint*/unsigned int program;
private:
	GLARE_DISABLE_COPY(OpenGLProgram);
public:
	int model_matrix_loc;
	int view_matrix_loc;
	int proj_matrix_loc;
	int normal_matrix_loc;
	int joint_matrix_loc;

	int time_loc;
	int colour_loc;
	int albedo_texture_loc;

	Reference<OpenGLShader> vert_shader;
	Reference<OpenGLShader> frag_shader;

	std::string prog_name;

	bool built_successfully;
	bool uses_phong_uniforms; // Does fragment shader use a PhongUniforms uniform block?
	bool is_depth_draw;
	bool is_depth_draw_with_alpha_test;
	bool is_outline;
	bool supports_MDI; // Should batches with this program be drawn with glMultiDrawElementsIndirect, if supported?
	bool uses_vert_uniform_buf_obs; // Does the vertex shader use a PerObjectVertUniforms uniform block?
	bool uses_skinning;

	UniformLocations uniform_locations;

	std::vector<UserUniformInfo> user_uniform_info;

	uint32 program_index;

	double build_start_time;
};


typedef Reference<OpenGLProgram> OpenGLProgramRef;
