/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/StringUtils.h"
#include <string>
#include <vector>
class OpenGLShader;


struct UniformLocations
{
	int diffuse_colour_location;
	int have_shading_normals_location;
	int have_texture_location;
	int diffuse_tex_location;
	int metallic_roughness_tex_location;
	int backface_diffuse_tex_location;
	int transmission_tex_location;
	int cosine_env_tex_location;
	int specular_env_tex_location;
	int lightmap_tex_location;
	int fbm_tex_location;
	int blue_noise_tex_location;
	int texture_matrix_location;
	int sundir_cs_location;
	int campos_ws_location;

	int dynamic_depth_tex_location;
	int static_depth_tex_location;
	int shadow_texture_matrix_location;

	int proj_view_model_matrix_location;

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



struct ProgramKey
{
	ProgramKey() {}
	ProgramKey(const std::string& program_name_, bool alpha_test_, bool vert_colours_, bool instance_matrices_, bool lightmapping_, bool gen_planar_uvs_, bool draw_planar_uv_grid_, bool convert_albedo_from_srgb_, bool skinning_,
		bool imposterable_, bool use_wind_vert_shader_, bool double_sided_) :
		program_name(program_name_), alpha_test(alpha_test_), vert_colours(vert_colours_), instance_matrices(instance_matrices_), lightmapping(lightmapping_), gen_planar_uvs(gen_planar_uvs_), draw_planar_uv_grid(draw_planar_uv_grid_), 
		convert_albedo_from_srgb(convert_albedo_from_srgb_), skinning(skinning_), imposterable(imposterable_), use_wind_vert_shader(use_wind_vert_shader_), double_sided(double_sided_) {}

	const std::string description() const { return "alpha_test: " + toString(alpha_test) + ", vert_colours: " + toString(vert_colours) + ", instance_matrices: " + toString(instance_matrices) + 
		", lightmapping: " + toString(lightmapping) + ", gen_planar_uvs: " + toString(gen_planar_uvs) + ", draw_planar_uv_grid_: " + toString(draw_planar_uv_grid) + 
		", convert_albedo_from_srgb: " + toString(convert_albedo_from_srgb) + ", skinning: " + toString(skinning) + ", imposterable: " + toString(imposterable) + ", " + toString(use_wind_vert_shader) + ", double_sided: " + toString(double_sided); }

	std::string program_name;
	bool alpha_test, vert_colours, instance_matrices, lightmapping, gen_planar_uvs, draw_planar_uv_grid, convert_albedo_from_srgb, skinning, imposterable, use_wind_vert_shader, double_sided;
	// convert_albedo_from_srgb is unfortunately needed for GPU-decoded video frame textures, which are sRGB but not marked as sRGB.

	inline bool operator < (const ProgramKey& b) const
	{
		if(program_name < b.program_name)
			return true;
		else if(program_name > b.program_name)
			return false;
		const int  val = (alpha_test   ? 1 : 0) | (vert_colours   ? 2 : 0) | (  instance_matrices ? 4 : 0) | (  lightmapping ? 8 : 0) | (  gen_planar_uvs ? 16 : 0) | (  draw_planar_uv_grid ? 32 : 0) | (  convert_albedo_from_srgb ? 64 : 0) | (  skinning ? 128 : 0) | (  imposterable ? 256 : 0) | (  use_wind_vert_shader ? 512 : 0) | (  double_sided ? 1024 : 0);
		const int bval = (b.alpha_test ? 1 : 0) | (b.vert_colours ? 2 : 0) | (b.instance_matrices ? 4 : 0) | (b.lightmapping ? 8 : 0) | (b.gen_planar_uvs ? 16 : 0) | (b.draw_planar_uv_grid ? 32 : 0) | (b.convert_albedo_from_srgb ? 64 : 0) | (b.skinning ? 128 : 0) | (b.imposterable ? 256 : 0) | (b.use_wind_vert_shader ? 512 : 0) | (b.double_sided ? 1024 : 0);
		return val < bval;
	}
};


/*=====================================================================
OpenGLProgram
-------------
=====================================================================*/
class OpenGLProgram : public RefCounted
{
public:
	OpenGLProgram(const std::string& prog_name, const Reference<OpenGLShader>& vert_shader, const Reference<OpenGLShader>& frag_shader);
	~OpenGLProgram();

	void useProgram() const;

	static void useNoPrograms();

	int getUniformLocation(const std::string& name);

	int getAttributeLocation(const std::string& name);

	void bindAttributeLocation(int index, const std::string& name);

	void appendUserUniformInfo(UserUniformInfo::UniformType uniform_type, const std::string& name);

	GLuint program;
private:
	GLARE_DISABLE_COPY(OpenGLProgram);
public:
	int model_matrix_loc;
	int view_matrix_loc;
	int proj_matrix_loc;
	int normal_matrix_loc;
	int joint_matrix_loc;

	int campos_ws_loc;
	int time_loc;
	int colour_loc;
	int albedo_texture_loc;
	int lightmap_tex_loc;
	int texture_2_loc;

	Reference<OpenGLShader> vert_shader;
	Reference<OpenGLShader> frag_shader;

	std::string prog_name;

	bool uses_phong_uniforms;
	bool is_transparent; // bit of a hack
	bool is_depth_draw;
	bool uses_vert_uniform_buf_obs;


	UniformLocations uniform_locations;

	std::vector<UserUniformInfo> user_uniform_info;

	ProgramKey key;
};


typedef Reference<OpenGLProgram> OpenGLProgramRef;
