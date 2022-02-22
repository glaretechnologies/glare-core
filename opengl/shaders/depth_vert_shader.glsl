
// For capturing depth data for shadow mapping

in vec3 position_in;
in vec3 normal_in;
in vec2 texture_coords_0_in;
#if VERT_COLOURS
in vec3 vert_colours_in;
#endif
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

#if SKINNING
in vec4 joint;
in vec4 weight;
#endif

out vec2 texture_coords;

uniform mat4 proj_view_model_matrix; 

#if INSTANCE_MATRICES || SKINNING || USE_WIND_VERT_SHADER
layout(std140) uniform SharedVertUniforms
{
	mat4 proj_matrix; // same for all objects
	mat4 view_matrix; // same for all objects
//#if NUM_DEPTH_TEXTURES > 0
	mat4 shadow_texture_matrix[5]; // same for all objects
//#endif
	vec4 campos_ws; // same for all objects
	float vert_uniforms_time;
	float wind_strength;
};


layout (std140) uniform PerObjectVertUniforms
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};
#endif


#if SKINNING
uniform mat4 joint_matrix[256];
#endif


float square(float x) { return x * x; }

#if USE_WIND_VERT_SHADER
vec3 newPosGivenWind(vec3 pos_ws, vec3 normal_ws)
{
	vec3 temp_pos_ws = pos_ws;
	vec3 use_normal = normal_ws;
	float ob_phase = temp_pos_ws.x * 0.1f;
	float wind_gust_str = (sin(vert_uniforms_time * 0.8654f + ob_phase) + sin(vert_uniforms_time * 0.343243f + ob_phase) + 2.5f) * 0.2f * wind_strength; // Should be in [0, 1]
#if VERT_COLOURS

	// Do branch/leaf movement up and down in normal direction (flutter)
	float side_factor = ((fract(texture_coords_0_in.x) - 0.5f) + (fract(texture_coords_0_in.y) - 0.5f));
	float flutter_freq = 6.2f * 3.f;
	float flutter_mag_factor = square(wind_gust_str);
	temp_pos_ws += use_normal * sin(vert_colours_in.y * 6.2f + vert_uniforms_time * flutter_freq) *  // assuming vert_colours_in.y can be used as phase.
		vert_colours_in.x * side_factor * flutter_mag_factor * 0.02f; 

	// Branch bend
	float bend_freq =  6.2f * 1.f;
	temp_pos_ws += use_normal * sin(2.f + vert_colours_in.y * 6.2f + vert_uniforms_time * bend_freq) *  // assuming vert_colours_in.y can be used as phase.
		(vert_colours_in.x) * wind_gust_str * 0.02f; 

	float bend_factor = min(3.0, square(square(wind_gust_str)));
	temp_pos_ws.x -= bend_factor * vert_colours_in.z * 0.4f; // tree bend
#endif
	return temp_pos_ws;
}
#endif


void main()
{
#if INSTANCE_MATRICES // -----------------

#if USE_WIND_VERT_SHADER
	vec3 pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;

	pos_ws = newPosGivenWind(pos_ws, normal_ws);
	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#else // USE_WIND_VERT_SHADER
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));
#endif

#else // else if !INSTANCE_MATRICES: -----------------

#if SKINNING
	mat4 skin_matrix =
		weight.x * joint_matrix[int(joint.x)] +
		weight.y * joint_matrix[int(joint.y)] +
		weight.z * joint_matrix[int(joint.z)] +
		weight.w * joint_matrix[int(joint.w)];

	mat4 model_skin_matrix = model_matrix * skin_matrix;

#if USE_WIND_VERT_SHADER
	vec3 pos_ws =    (model_skin_matrix * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (model_skin_matrix * vec4(normal_in, 0.0)).xyz;
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
#else
	vec3 pos_ws = (model_skin_matrix * vec4(position_in, 1.0)).xyz;
#endif

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));

#else // else if !SKINNING:

#if USE_WIND_VERT_SHADER
	vec3 pos_ws =    (model_matrix * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (model_matrix * vec4(normal_in, 0.0)).xyz;
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#else
	gl_Position = proj_view_model_matrix * vec4(position_in, 1.0);
#endif // USE_WIND_VERT_SHADER
	
#endif // endif !SKINNING

#endif // endif !INSTANCE_MATRICES -----------------

	texture_coords = texture_coords_0_in;
}
