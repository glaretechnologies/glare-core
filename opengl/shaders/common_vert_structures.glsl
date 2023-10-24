
// Should be the same as SharedVertUniforms in OpenGLEngine.h
layout (std140) uniform SharedVertUniforms
{
	mat4 proj_matrix; // same for all objects
	mat4 view_matrix; // same for all objects
	//#if NUM_DEPTH_TEXTURES > 0
	mat4 shadow_texture_matrix[5]; // same for all objects
	//#endif
	vec4 campos_ws; // same for all objects
	vec4 vert_sundir_ws;

	vec4 grass_pusher_sphere_pos;

	float vert_uniforms_time;
	float wind_strength;

	float padding_0; // AMD drivers have different opinions on if structures are padded than Nvidia drivers, so explicitly pad.
	float padding_1;
};


// Should be the same as PerObjectVertUniforms in OpenGLEngine.h
struct PerObjectVertUniformsStruct
{
	mat4 model_matrix;
	mat4 normal_matrix;

	ivec4 light_indices_0;
	ivec4 light_indices_1;

	float depth_draw_depth_bias;

	float padding_po1;
	float padding_po2;
	float padding_po3;
};
