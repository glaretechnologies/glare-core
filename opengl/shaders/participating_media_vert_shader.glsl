
in vec3 position_in; // object-space position
in float imposter_width_in;
in vec2 texture_coords_0_in;
//in float imposter_rot_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

out vec3 pos_cs;
out vec3 pos_ws;
out vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
out vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
out vec3 cam_to_pos_ws;


#if USE_MULTIDRAW_ELEMENTS_INDIRECT
flat out int material_index;
#endif



//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

layout(std430) buffer PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[];
};

layout (std430) buffer ObAndMatIndicesStorage
{
	int ob_and_mat_indices[];
};

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data;
};

#endif // !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------


void main()
{
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	int per_ob_data_index = ob_and_mat_indices[gl_DrawID * 4 + 0];
	material_index        = ob_and_mat_indices[gl_DrawID * 4 + 2];
	mat4 model_matrix  = per_object_data[per_ob_data_index].model_matrix;
#else
	mat4 model_matrix =  per_object_data.model_matrix;
#endif


#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));

	// TODO: offsetting to make non-zero sized sprite
	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (instance_matrix_in * vec4(position_in, 1.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (instance_matrix_in * vec4(position_in, 1.0))).xyz;
#endif

#else // else if !INSTANCE_MATRICES:

	vec3 orig_pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz; // Original position in world space (before offsetting to make non-zero sized sprite)
	vec3 cam_to_orig_pos_ws = orig_pos_ws - campos_ws.xyz;
	

	float vert_uv_right = texture_coords_0_in.x - 0.5;
	float vert_uv_up    = texture_coords_0_in.y - 0.5;

	vec3 desired_right_ws = normalize(cross(cam_to_orig_pos_ws, vec3(0, 0, 1.0)));
	vec3 desired_up_ws    = normalize(cross(desired_right_ws, cam_to_orig_pos_ws));
	pos_ws = orig_pos_ws + imposter_width_in * (vert_uv_right * desired_right_ws + vert_uv_up * desired_up_ws);

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));

	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * vec4(pos_ws, 1.0)).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * vec4(pos_ws, 1.0)).xyz;
#endif

#endif // end if !INSTANCE_MATRICES

	texture_coords = texture_coords_0_in;
}
