
in vec3 position_in; // object-space position
in float imposter_width_in; // not used
in vec2 texture_coords_0_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

out vec3 pos_cs;
out vec3 pos_ws;
out vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
out vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
//out vec3 cam_to_pos_ws;
out vec3 rotated_right_ws;
out vec3 rotated_up_ws;


#if OB_AND_MAT_DATA_GPU_RESIDENT
flat out int material_index;
#endif



//----------------------------------------------------------------------------------------------------------------------------
#if OB_AND_MAT_DATA_GPU_RESIDENT

layout(std430) buffer PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[];
};

#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	// If using MDEI, then the object and mat indices are fetched from indexing into ob_and_mat_indices with gl_DrawID.
	layout (std430) buffer ObAndMatIndicesStorage
	{
		int ob_and_mat_indices[];
	};
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT
	// If not using MDEI, the object and mat indices are passed to the shader in this uniform.
	layout (std140) uniform ObJointAndMatIndices
	{
		ObJointAndMatIndicesStruct ob_joint_and_mat_indices;
	};
#endif

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !OB_AND_MAT_DATA_GPU_RESIDENT:

layout (std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data;
};

#endif // !OB_AND_MAT_DATA_GPU_RESIDENT
//----------------------------------------------------------------------------------------------------------------------------


vec2 rot(vec2 p, float theta)
{
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}


void main()
{
#if OB_AND_MAT_DATA_GPU_RESIDENT
	#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	// Compute from gl_DrawID
	int per_ob_data_index = ob_and_mat_indices[gl_DrawID * OB_AND_MAT_INDICES_STRIDE + 0];
	material_index        = ob_and_mat_indices[gl_DrawID * OB_AND_MAT_INDICES_STRIDE + 2];
	#else
	// Get from ob_joint_and_mat_indices uniform
	int per_ob_data_index = ob_joint_and_mat_indices.per_ob_data_index;
	material_index        = ob_joint_and_mat_indices.material_index;
	#endif

	mat4 model_matrix  = per_object_data[per_ob_data_index].model_matrix;
#else
	mat4 model_matrix =  per_object_data.model_matrix;
#endif


#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));

	// TODO: offsetting to make non-zero sized sprite
	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	//cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (instance_matrix_in * vec4(position_in, 1.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (instance_matrix_in * vec4(position_in, 1.0))).xyz;
#endif

#else // else if !INSTANCE_MATRICES:

	vec3 orig_pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz; // Original position in world space (before offsetting to make non-zero sized sprite)
	vec3 cam_to_orig_pos_ws = orig_pos_ws - campos_ws.xyz;
	
	// Since object-space vert positions are just (0,0,0) for particle geometry, we can store info in the model matrix.
	float use_width = model_matrix[0].x;
	float rot_theta = model_matrix[0].y; // Rotation around vector from particle to camera

	float vert_uv_right = texture_coords_0_in.x - 0.5;
	float vert_uv_up    = texture_coords_0_in.y - 0.5;

	vec3 desired_right_ws = normalize(cross(cam_to_orig_pos_ws, vec3(0, 0, 1.0)));
	vec3 desired_up_ws    = normalize(cross(desired_right_ws, cam_to_orig_pos_ws));

	// Rotate right and up vectors around vector to camera
	rotated_right_ws = desired_right_ws *  cos(rot_theta) + desired_up_ws * sin(rot_theta);
	rotated_up_ws    = desired_right_ws * -sin(rot_theta) + desired_up_ws * cos(rot_theta);

	pos_ws = orig_pos_ws + use_width * (rotated_right_ws * vert_uv_right + rotated_up_ws * vert_uv_up);

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));

	//cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * vec4(pos_ws, 1.0)).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * vec4(pos_ws, 1.0)).xyz;
#endif

#endif // end if !INSTANCE_MATRICES

	texture_coords = texture_coords_0_in;
}
