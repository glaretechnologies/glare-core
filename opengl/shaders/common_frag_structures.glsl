
// Should be the same layout as in OpenGLEngine.h
layout (std140) uniform MaterialCommonUniforms
{
	mat4 frag_view_matrix;
	vec4 sundir_cs; // Dir to sun.
	vec4 sundir_ws; // Dir to sun.
	vec4 sun_spec_rad_times_solid_angle;
	vec4 sun_and_sky_av_spec_rad;
	vec4 air_scattering_coeffs;
	float near_clip_dist;
	float far_clip_dist;
	float time;
	float l_over_w; // lens_sensor_dist / sensor width
	float l_over_h; // lens_sensor_dist / sensor height
	float env_phi;
	float water_level_z;
	int camera_type; // OpenGLScene::CameraType
};


#define HAVE_SHADING_NORMALS_FLAG			1
#define HAVE_TEXTURE_FLAG					2
#define HAVE_METALLIC_ROUGHNESS_TEX_FLAG	4
#define HAVE_EMISSION_TEX_FLAG				8
#define IS_HOLOGRAM_FLAG					16 // e.g. no light scattering, just emission
#define IMPOSTER_TEX_HAS_MULTIPLE_ANGLES	32
#define HAVE_NORMAL_MAP_FLAG				64


#define CameraType_Identity					0
#define CameraType_Perspective				1
#define CameraType_Orthographic				2
#define CameraType_DiagonalOrthographic		3


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
	sampler2D normal_map;
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
	float padding12;
	float padding13;
#endif

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z; // For imposters: begin_fade_in_distance.
	float materialise_upper_z; // For imposters: end_fade_in_distance.     For participating media and decals: dopacity/dt
	float materialise_start_time; // For participating media and decals: spawn time

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
