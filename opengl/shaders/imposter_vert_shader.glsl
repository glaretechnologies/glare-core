
in vec3 position_in; // object-space position
//in vec3 normal_in;
in float imposter_width_in;
in vec2 texture_coords_0_in;
in float imposter_rot_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

out vec3 normal_cs; // cam (view) space
out vec3 normal_ws; // world space
out vec3 pos_cs;
out vec3 pos_ws; // Pass along so we can use with dFdx to compute use_normal_ws.
out vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
out vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
out vec3 cam_to_pos_ws;
out float imposter_rot;


layout (std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data;
};


void main()
{
#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));

	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (instance_matrix_in * vec4(position_in, 1.0))).xyz;

	normal_ws = vec3(-cam_to_pos_ws.xy, 0.0);
	normal_cs = (view_matrix * vec4(normal_ws, 0.0)).xyz;
	//normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;
	//normal_cs = (view_matrix * (instance_matrix_in * vec4(normal_in, 0.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (instance_matrix_in * vec4(position_in, 1.0))).xyz;
#endif

#else // else if !INSTANCE_MATRICES:

	mat4 model_skin_matrix;
	mat4 normal_skin_matrix;
	model_skin_matrix = per_object_data.model_matrix;
	normal_skin_matrix = per_object_data.normal_matrix;

	vec3 orig_pos_ws = (model_skin_matrix  * vec4(position_in, 1.0)).xyz;
	vec3 cam_to_orig_pos_ws = orig_pos_ws - campos_ws.xyz;
	

	float vert_uv_right = texture_coords_0_in.x - 0.5;

	vec3 desired_right_ws = normalize(cross(cam_to_orig_pos_ws, vec3(0, 0, 1.0)));
	pos_ws = orig_pos_ws + imposter_width_in * vert_uv_right * desired_right_ws;

#if USE_WIND_VERT_SHADER
	float ob_phase = pos_ws.x * 0.9f + imposter_rot_in * 0.3;
	float wind_gust_str = (sin(vert_uniforms_time * 3.8654f + ob_phase) + sin(vert_uniforms_time * 2.343243f + ob_phase) + 2.5f) * 0.2f * wind_strength; // Should be in [0, 1]
	
	//const float wind_influence = max(texture_coords_0_in.y - 0.9, 0.0) * 4;//texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y * texture_coords_0_in.y;
	const float wind_influence = texture_coords_0_in.y * (0.75 + imposter_rot_in * (1.0 / 6.28318) * 0.5); // (texture_coords_0_in.y > 0.99) ? 1 : 0;
	pos_ws.x += wind_influence * wind_gust_str * 0.2;
#endif

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));

	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (model_skin_matrix  * vec4(pos_ws, 1.0))).xyz;

	normal_ws = vec3(-cam_to_orig_pos_ws.xy, 0.0);
	normal_cs = (view_matrix * vec4(normal_ws, 0.0)).xyz;
	//normal_ws = (normal_skin_matrix * vec4(normal_in, 0.0)).xyz;
	//normal_cs = (view_matrix * (normal_skin_matrix * vec4(normal_in, 0.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (model_skin_matrix * vec4(pos_ws, 1.0))).xyz;
#endif

#endif // end if !INSTANCE_MATRICES

	texture_coords = texture_coords_0_in;
	imposter_rot = imposter_rot_in;
}
