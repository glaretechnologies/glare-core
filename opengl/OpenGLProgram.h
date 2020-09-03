/*=====================================================================
OpenGLProgram.h
----------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <vector>
class OpenGLShader;


struct UniformLocations
{
	int diffuse_colour_location;
	int have_shading_normals_location;
	int have_texture_location;
	int diffuse_tex_location;
	int cosine_env_tex_location;
	int specular_env_tex_location;
	int lightmap_tex_location;
	int texture_matrix_location;
	int sundir_cs_location;
	int roughness_location;
	int fresnel_scale_location;
	int metallic_frac_location;
	int campos_ws_location;

	int dynamic_depth_tex_location;
	int static_depth_tex_location;
	int shadow_texture_matrix_location;
};


struct UserUniformInfo
{
	enum UniformType
	{
		UniformType_Vec2,
		UniformType_Vec3,
		UniformType_Int,
		UniformType_Float
	};

	UserUniformInfo() {}
	UserUniformInfo(int loc_, int index_, UniformType uniform_type_) : loc(loc_), index(index_), uniform_type(uniform_type_) {}

	int loc; // Location in shader as retrieved from glGetUniformLocation()
	int index; // Index into material.user_uniform_vals
	UniformType uniform_type;
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

	GLuint program;
private:
	INDIGO_DISABLE_COPY(OpenGLProgram);
public:
	int model_matrix_loc;
	int view_matrix_loc;
	int proj_matrix_loc;
	int normal_matrix_loc;

	int campos_ws_loc;
	int time_loc;
	int colour_loc;
	int albedo_texture_loc;
	int lightmap_tex_loc;
	int texture_2_loc;

	Reference<OpenGLShader> vert_shader;
	Reference<OpenGLShader> frag_shader;

	std::string prog_name;

	bool is_phong; // bit of a hack
	bool uses_phong_uniforms; // bit of a hack


	UniformLocations uniform_locations;

	std::vector<UserUniformInfo> user_uniform_info;
};


typedef Reference<OpenGLProgram> OpenGLProgramRef;
