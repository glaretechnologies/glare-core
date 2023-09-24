
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
out vec3 pos_ws;
out vec3 normal_ws;

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

//#if SKINNING
layout (std430) buffer JointMatricesStorage
{
	mat4 joint_matrix[];
};
//#endif

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data;
};

#if SKINNING
uniform mat4 joint_matrix[256];
#endif

#endif // !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------





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
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	int per_ob_data_index = ob_and_mat_indices[gl_DrawID * 4 + 0];
	int joints_base_index = ob_and_mat_indices[gl_DrawID * 4 + 1];
	material_index        = ob_and_mat_indices[gl_DrawID * 4 + 2];
	mat4 model_matrix  = per_object_data[per_ob_data_index].model_matrix;
	mat4 normal_matrix = per_object_data[per_ob_data_index].normal_matrix;
#else
	int joints_base_index = 0;
	mat4 model_matrix  = per_object_data.model_matrix;
	mat4 normal_matrix = per_object_data.normal_matrix;
#endif

#if INSTANCE_MATRICES // -----------------

#if USE_WIND_VERT_SHADER
	vec3 pos_ws    = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;

	pos_ws = newPosGivenWind(pos_ws, normal_ws);
	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#else // USE_WIND_VERT_SHADER
	pos_ws =    (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#endif

#else // else if !INSTANCE_MATRICES: -----------------

#if SKINNING
	mat4 skin_matrix =
		weight.x * joint_matrix[joints_base_index + int(joint.x)] +
		weight.y * joint_matrix[joints_base_index + int(joint.y)] +
		weight.z * joint_matrix[joints_base_index + int(joint.z)] +
		weight.w * joint_matrix[joints_base_index + int(joint.w)];

	mat4 model_skin_matrix = model_matrix * skin_matrix;

#if USE_WIND_VERT_SHADER
	vec3 pos_ws =    (model_skin_matrix * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (model_skin_matrix * vec4(normal_in, 0.0)).xyz;
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
#else
	pos_ws    = (model_skin_matrix * vec4(position_in, 1.0)).xyz;
	normal_ws = (model_skin_matrix * vec4(normal_in, 0.0)).xyz;
#endif

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));

#else // else if !SKINNING:

#if USE_WIND_VERT_SHADER
	vec3 pos_ws =    (model_matrix * vec4(position_in, 1.0)).xyz;
	vec3 normal_ws = (model_matrix * vec4(normal_in, 0.0)).xyz;
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#else

	pos_ws =    (model_matrix  * vec4(position_in, 1.0)).xyz;
	normal_ws = (normal_matrix * vec4(normal_in, 0.0)).xyz;

	gl_Position = proj_matrix * (view_matrix * vec4(pos_ws, 1.0));
#endif // USE_WIND_VERT_SHADER
	
#endif // endif !SKINNING

#endif // endif !INSTANCE_MATRICES -----------------

	texture_coords = texture_coords_0_in;
}
