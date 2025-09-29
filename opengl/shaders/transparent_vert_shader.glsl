
#if POSITION_W_IS_OCT16_NORMAL
in vec4 position_in; // object-space vertex position (xyz) and oct16 normal (w).
#else
in vec3 position_in; // object-space vertex position
#endif

#if !POSITION_W_IS_OCT16_NORMAL
in vec3 normal_in;
#endif

in vec2 texture_coords_0_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

#if SKINNING
in uvec4 joint;
in vec4 weight;
#endif

out vec3 normal_cs; // cam (view) space
out vec3 normal_ws; // world space
out vec3 pos_cs;
#if GENERATE_PLANAR_UVS
out vec3 pos_os;
#endif
out vec3 pos_ws;
out vec2 texture_coords;
out vec3 cam_to_pos_ws;

#if OB_AND_MAT_DATA_GPU_RESIDENT
flat out int material_index;
#endif

flat out ivec4 light_indices_0;
flat out ivec4 light_indices_1;

//----------------------------------------------------------------------------------------------------------------------------
#if OB_AND_MAT_DATA_GPU_RESIDENT


layout(std430) buffer PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[];
};

layout (std430) buffer JointMatricesStorage
{
	mat4 joint_matrix[];
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

#if SKINNING
layout (std140) uniform JointMatrixUniforms
{
	mat4 joint_matrix[256];
};
#endif

#endif // !OB_AND_MAT_DATA_GPU_RESIDENT
//----------------------------------------------------------------------------------------------------------------------------


void main()
{
#if OB_AND_MAT_DATA_GPU_RESIDENT
	#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	// Compute from gl_DrawID
	int per_ob_data_index = ob_and_mat_indices[gl_DrawID * OB_AND_MAT_INDICES_STRIDE + 0];
	int joints_base_index = ob_and_mat_indices[gl_DrawID * OB_AND_MAT_INDICES_STRIDE + 1];
	material_index        = ob_and_mat_indices[gl_DrawID * OB_AND_MAT_INDICES_STRIDE + 2];
	#else
	// Get from ob_joint_and_mat_indices uniform
	int per_ob_data_index = ob_joint_and_mat_indices.per_ob_data_index;
	int joints_base_index = ob_joint_and_mat_indices.joints_base_index;
	material_index        = ob_joint_and_mat_indices.material_index;
	#endif

	uint joint_index_mask = 0xFFFFFFFFu;
	mat4 model_matrix  = per_object_data[per_ob_data_index].model_matrix;
	mat4 normal_matrix = per_object_data[per_ob_data_index].normal_matrix;
	float uv0_scale        = per_object_data[per_ob_data_index].uv0_scale;
	vec4 dequantise_scale = per_object_data[per_ob_data_index].dequantise_scale;
	vec4 dequantise_trans = per_object_data[per_ob_data_index].dequantise_translation;
#else
	int joints_base_index = 0;
	uint joint_index_mask = 0xFFu; // Without MDI, joint_matrix has max length 256.  Make sure we don't read out-of-bounds.
	mat4 model_matrix  = per_object_data.model_matrix;
	mat4 normal_matrix = per_object_data.normal_matrix;
	float uv0_scale       = per_object_data.uv0_scale;
	vec4 dequantise_scale = per_object_data.dequantise_scale;
	vec4 dequantise_trans = per_object_data.dequantise_translation;
#endif

	vec4 final_pos_os = dequantise_scale * vec4(position_in.xyz, 1.0) + dequantise_trans;

#if POSITION_W_IS_OCT16_NORMAL
	vec3 final_normal_in = decodeNormalFromPositionW(position_in.w);
#else
	vec3 final_normal_in = normal_in;
#endif

#if INSTANCE_MATRICES //-------------------------
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * final_pos_os));

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (instance_matrix_in * final_pos_os).xyz;
	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (instance_matrix_in * final_pos_os)).xyz;

	normal_ws = (instance_matrix_in * vec4(final_normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (instance_matrix_in * vec4(final_normal_in, 0.0))).xyz;
#else //-------- else if !INSTANCE_MATRICES:

#if SKINNING
	// See https://www.khronos.org/files/gltf20-reference-guide.pdf
	mat4 skin_matrix =
		weight.x * joint_matrix[joints_base_index + int(joint.x & joint_index_mask)] +
		weight.y * joint_matrix[joints_base_index + int(joint.y & joint_index_mask)] +
		weight.z * joint_matrix[joints_base_index + int(joint.z & joint_index_mask)] +
		weight.w * joint_matrix[joints_base_index + int(joint.w & joint_index_mask)];


	mat4 model_skin_matrix = model_matrix * skin_matrix;
	mat4 normal_skin_matrix = normal_matrix * skin_matrix;// * transpose(skin_matrix);
#else
	mat4 model_skin_matrix = model_matrix;
	mat4 normal_skin_matrix = normal_matrix;
#endif

	vec4 pos_ws_vec4 = model_skin_matrix * final_pos_os;
	pos_ws = pos_ws_vec4.xyz;

	gl_Position = proj_matrix * (view_matrix * pos_ws_vec4);

#if GENERATE_PLANAR_UVS
	pos_os = (dequantise_scale * vec4(position_in.xyz, 1.0) + dequantise_trans).xyz;
#endif

	cam_to_pos_ws = pos_ws_vec4.xyz - campos_ws.xyz;
	pos_cs = (view_matrix * pos_ws_vec4).xyz;
 
	normal_ws = (normal_skin_matrix * vec4(final_normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_skin_matrix * vec4(final_normal_in, 0.0))).xyz;
#endif //-------------------------
	//texture_coords = texture_coords_0_in;

	texture_coords = texture_coords_0_in * uv0_scale;

#if OB_AND_MAT_DATA_GPU_RESIDENT
	light_indices_0 = per_object_data[per_ob_data_index].light_indices_0;
	light_indices_1 = per_object_data[per_ob_data_index].light_indices_1;
#else
	light_indices_0 = per_object_data.light_indices_0;
	light_indices_1 = per_object_data.light_indices_1;
#endif
}
