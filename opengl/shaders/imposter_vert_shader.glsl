
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

	vec3 unit_cam_to_pos = normalize(cam_to_orig_pos_ws);

	
#if USE_WIND_VERT_SHADER
	float positon_phase_term =  pos_ws.x * -0.2f;
	float ob_phase_term = imposter_rot_in * 0.3;

	float wind_gust_str = max(0.0, 
		1.0 * sin(vert_uniforms_time*2.0 + positon_phase_term + ob_phase_term) + 
		abs(1 + sin(pos_ws.x * 0.3f + pos_ws.y * 1.3f + vert_uniforms_time*0.5)) + 
		4.0) *
		0.3 * wind_strength;
	float bend_amount = wind_gust_str*wind_gust_str;


	// Start fluttering (high frequency oscillation) when wind is strong enough.
	float flutter_env = bend_amount * 0.02;

	float flutter_ang_freq = 20.0 + imposter_rot_in*3;
	float flutter = sin(vert_uniforms_time * flutter_ang_freq + imposter_rot_in * (1.0 / 6.28318)) * flutter_env;
	bend_amount = min(bend_amount, 5.0);

	pos_ws.x += texture_coords_0_in.y * bend_amount * 0.06 + texture_coords_0_in.y * flutter * 0.03; // Bend top sideways in wind direction
	pos_ws.z -= texture_coords_0_in.y * bend_amount * 0.06 + texture_coords_0_in.y * flutter * 0.03; // Bend top down as well.
#endif

	// If camera is looking nearly straight down, tilt imposters backwards so they are less obviously flat billboards.
	// bend_factor = 1 when z = -1, 0 when z >= -0.7
	// bend_factor = -(unit_cam_to_pos.z - -0.7) = -(unit_cam_to_pos.z + 0.7) = -unit_cam_to_pos.z - 0.7
	float bend_factor = max(0.0, -unit_cam_to_pos.z - 0.7);
	pos_ws += texture_coords_0_in.y * cross(desired_right_ws, unit_cam_to_pos) * bend_factor;

	// Bend grass away from pusher sphere/cylinder (around avatar)
	float pusher_cylinder_rad = 0.3;
	float dist_to_cylinder_orig_xy = length(grass_pusher_sphere_pos.xy - pos_ws.xy);
	if(dist_to_cylinder_orig_xy < pusher_cylinder_rad)
	{
		if(pos_ws.z > grass_pusher_sphere_pos.z)
		{
			vec3 push_vector = normalize(vec3((orig_pos_ws.xy - grass_pusher_sphere_pos.xy), 0.0));
			float push_amount = pusher_cylinder_rad - dist_to_cylinder_orig_xy;
			pos_ws += push_vector * texture_coords_0_in.y * push_amount;
		}
		
	}

	
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
