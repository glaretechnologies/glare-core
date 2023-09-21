/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2016 -
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
	UniformLocations() : caustic_tex_a_location(-1), caustic_tex_b_location(-1) {}

	int diffuse_tex_location;
	int metallic_roughness_tex_location;
	int emission_tex_location;
	int backface_diffuse_tex_location;
	int transmission_tex_location;
	int cosine_env_tex_location;
	int specular_env_tex_location;
	int lightmap_tex_location;
	int fbm_tex_location;
	int cirrus_tex_location; // Just for water reflection of cirrus
	int detail_tex_0_location;
	int detail_tex_1_location;
	int detail_tex_2_location;
	int detail_tex_3_location;
	int blue_noise_tex_location;
	int main_colour_texture_location;
	int main_normal_texture_location;
	int main_depth_texture_location;
	int caustic_tex_a_location;
	int caustic_tex_b_location;
	int water_colour_texture_location;
	
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


struct ProgramKeyArgs
{
	ProgramKeyArgs() : alpha_test(false), vert_colours(false), instance_matrices(false), lightmapping(false), gen_planar_uvs(false), draw_planar_uv_grid(false), convert_albedo_from_srgb(false), skinning(false),
		imposterable(false), use_wind_vert_shader(false), double_sided(false), materialise_effect(false), geomorphing(false), terrain(false)
	{}

	bool alpha_test, vert_colours, instance_matrices, lightmapping, gen_planar_uvs, draw_planar_uv_grid, convert_albedo_from_srgb, skinning, imposterable, use_wind_vert_shader, double_sided, materialise_effect, geomorphing, terrain;
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
		convert_albedo_from_srgb	= args.convert_albedo_from_srgb;
		skinning					= args.skinning;
		imposterable				= args.imposterable;
		use_wind_vert_shader		= args.use_wind_vert_shader;
		double_sided				= args.double_sided;
		materialise_effect			= args.materialise_effect;
		geomorphing					= args.geomorphing;
		terrain						= args.terrain;

		keyval = (alpha_test   ? 1 : 0) | (vert_colours   ? 2 : 0) | (  instance_matrices ? 4 : 0) | (  lightmapping ? 8 : 0) | (  gen_planar_uvs ? 16 : 0) | (  draw_planar_uv_grid ? 32 : 0) | (  convert_albedo_from_srgb ? 64 : 0) | 
			(  skinning ? 128 : 0) | (  imposterable ? 256 : 0) | (  use_wind_vert_shader ? 512 : 0) | (  double_sided ? 1024 : 0) | (  materialise_effect ? 2048 : 0) | (  geomorphing ? 4096 : 0) | (terrain ? 8192 : 0);
	}

	const std::string description() const { return "alpha_test: " + toString(alpha_test) + ", vert_colours: " + toString(vert_colours) + ", instance_matrices: " + toString(instance_matrices) + 
		", lightmapping: " + toString(lightmapping) + ", gen_planar_uvs: " + toString(gen_planar_uvs) + ", draw_planar_uv_grid_: " + toString(draw_planar_uv_grid) + 
		", convert_albedo_from_srgb: " + toString(convert_albedo_from_srgb) + ", skinning: " + toString(skinning) + ", imposterable: " + toString(imposterable) + ", " + toString(use_wind_vert_shader) + 
		", double_sided: " + toString(double_sided) + ", materialise_effect: " + toString(materialise_effect); }

	std::string program_name;
	bool alpha_test, vert_colours, instance_matrices, lightmapping, gen_planar_uvs, draw_planar_uv_grid, convert_albedo_from_srgb, skinning, imposterable, use_wind_vert_shader, double_sided, materialise_effect, geomorphing, terrain;
	// convert_albedo_from_srgb is unfortunately needed for GPU-decoded video frame textures, which are sRGB but not marked as sRGB.
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
	OpenGLProgram(const std::string& prog_name, const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader, uint32 program_index);
	~OpenGLProgram();

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
	int lightmap_tex_loc;
	int texture_2_loc;

	Reference<OpenGLShader> vert_shader;
	Reference<OpenGLShader> frag_shader;

	std::string prog_name;

	bool uses_phong_uniforms; // Does fragment shader use a PhongUniforms uniform block?
	bool is_transparent; // bit of a hack
	bool is_depth_draw;
	bool is_depth_draw_with_alpha_test;
	bool is_outline;
	bool supports_MDI; // Should batches with this program be drawn with glMultiDrawElementsIndirect, if supported?
	bool uses_vert_uniform_buf_obs; // Does the vertex shader use a PerObjectVertUniforms uniform block?
	bool uses_colour_and_depth_buf_textures;


	UniformLocations uniform_locations;

	std::vector<UserUniformInfo> user_uniform_info;

	uint32 program_index;
};


typedef Reference<OpenGLProgram> OpenGLProgramRef;
