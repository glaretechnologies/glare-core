
// Should be the same layout as in OpenGLEngine.h
layout (std140) uniform MaterialCommonUniforms
{
	mat4 frag_view_matrix;
	vec4 sundir_cs;
	vec4 sundir_ws;
	vec4 sun_spec_rad_times_solid_angle;
	vec4 sun_and_sky_av_spec_rad;
	vec4 air_scattering_coeffs;
	float near_clip_dist;
	float time;
	float l_over_w;
	float l_over_h;
	float env_phi;
	float water_level_z;

	float padding_a0; // Padding to get to a multiple of 4 32-bit members.
	float padding_a1;
};


struct MaterialData
{
	vec4 diffuse_colour;
	vec4 emission_colour;
	vec2 texture_upper_left_matrix_col0;
	vec2 texture_upper_left_matrix_col1;
	vec2 texture_matrix_translation;

#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
	sampler2D metallic_roughness_tex;
	sampler2D lightmap_tex;
	sampler2D emission_tex;
	sampler2D backface_albedo_tex;
	sampler2D transmission_tex;
#else
	float padding0;
	float padding1;
	float padding2;
	float padding3;
	float padding4;
	float padding5;
	float padding6;
	float padding7;
	float padding8;
	float padding9;
	float padding10;
	float padding11;
#endif

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

	float padding_b0;
};


// Should match LightData struct in OpenGLEngine.h
struct LightData
{
	vec4 pos;
	vec4 dir;
	vec4 col;
	int light_type; // 0 = point light, 1 = spotlight
	float cone_cos_angle_start;
	float cone_cos_angle_end;

	float padding_l0;
};
