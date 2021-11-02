#version 150

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
	//if(use_normal.z < 0.f)
	//	use_normal = -use_normal;
	float ob_phase = temp_pos_ws.x * 0.1f;
	//float wind_gust_str = texture(fbm_tex, vec2(vert_uniforms_time, 0.5f)).x; // 1.f + sin(vert_uniforms_time * 1.f);
	float wind_gust_str = (sin(vert_uniforms_time * 0.8654f + ob_phase) + sin(vert_uniforms_time * 0.343243f + ob_phase) + 2.5f) * (1.f / 4.5f); // Should be in [0, 1]
	//float wind_gust_str = (1.f + sin(vert_uniforms_time * 1.f)) * (1.f + sin(vert_uniforms_time * 0.2f)) * 0.25f;
	//temp_pos_ws.xyz += use_normal * sin(temp_pos_ws.x + vert_colours_in.y * 6.2f + vert_uniforms_time * 30.f) * (vert_colours_in.x) * 0.2f * (texture_coords_0_in.x - 0.5f) * wind_gust_str;
#if VERT_COLOURS
	temp_pos_ws += use_normal * /*sin(temp_pos_ws.x + temp_pos_ws.y * 100.f) **/ sin(vert_colours_in.y * 6.2f + vert_uniforms_time * 6.f) *  // assuming vert_colours_in.y can be used as phase.
		(vert_colours_in.x) * ((texture_coords_0_in.x - texture_coords_0_in.y) - 0.5f) * wind_gust_str * 0.02f; // Branch/leaf movement up and down in normal direction
#endif
	temp_pos_ws.x -= square(square(square(wind_gust_str))) * square(position_in.y)/*temp_pos_ws.z * temp_pos_ws.z*/ * 0.05f; // tree bend
	//temp_pos_ws.x -= wind_gust_str * square(position_in.z)/*temp_pos_ws.z * temp_pos_ws.z*/ * 0.1f; // tree bend
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
