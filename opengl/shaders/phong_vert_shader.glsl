
in vec3 position_in; // object-space position
in vec3 normal_in;
in vec2 texture_coords_0_in;
#if VERT_COLOURS
in vec3 vert_colours_in;
#endif
#if LIGHTMAPPING
in vec2 lightmap_coords_in;
#endif
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

#if SKINNING
in ivec4 joint;
in vec4 weight;
#endif

#if VERT_TANGENTS
in vec4 tangent_in;
#endif

#if COMBINED
in int combined_mat_index_in;
#endif

out vec3 normal_ws; // world space
out vec3 pos_cs;
#if GENERATE_PLANAR_UVS
out vec3 pos_os;
#endif
out vec3 pos_ws; // Pass along so we can use with dFdx to compute use_normal_ws.
out vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
out vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
out vec3 cam_to_pos_ws;

#if VERT_COLOURS
out vec3 vert_colour;
#endif

#if LIGHTMAPPING
out vec2 lightmap_coords;
#endif

#if USE_MULTIDRAW_ELEMENTS_INDIRECT
flat out int material_index;
#endif

#if DECAL
out mat4 world_to_ob;
#endif

#if VERT_TANGENTS
out vec3 tangent_ws;
out vec3 bitangent_ws;
#endif

#if COMBINED
flat out int combined_mat_index;
#endif

flat out ivec4 light_indices_0;
flat out ivec4 light_indices_1;



//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

layout(std430) buffer PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[];
};

//uniform int ob_and_mat_indices[65536 * 2];

//layout (std140) uniform ObAndMatIndicesStorage
//{
//	int ob_and_mat_indices[65536 * 2];
//};
layout (std430) buffer ObAndMatIndicesStorage
{
	int ob_and_mat_indices[];
};

layout (std430) buffer JointMatricesStorage
{
	mat4 joint_matrix[];
};

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

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

#endif // !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------




float square(float x) { return x * x; }

// vert_colours_in.x: branch bend amount
// vert_colours_in.y: phase
// vert_colours_in.z: tree bend amount

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

#if COMBINED
	combined_mat_index = combined_mat_index_in;
#endif

#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	int per_ob_data_index = ob_and_mat_indices[gl_DrawID * 4 + 0];
	int joints_base_index = ob_and_mat_indices[gl_DrawID * 4 + 1];
	material_index        = ob_and_mat_indices[gl_DrawID * 4 + 2];
	mat4 model_matrix  = per_object_data[per_ob_data_index].model_matrix;
	mat4 normal_matrix = per_object_data[per_ob_data_index].normal_matrix;
#else
	int joints_base_index = 0;
	mat4 model_matrix =  per_object_data.model_matrix;
	mat4 normal_matrix = per_object_data.normal_matrix;
#endif

#if INSTANCE_MATRICES

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;

	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;

#if USE_WIND_VERT_SHADER
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
#endif

	vec4 pos_cs_vec4 = view_matrix * vec4(pos_ws, 1.0);
	gl_Position = proj_matrix * pos_cs_vec4;

	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = pos_cs_vec4.xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * vec4(pos_ws, 1.0)).xyz;
#endif

#if VERT_TANGENTS
	tangent_ws = (instance_matrix_in * vec4(tangent_in.xyz, 0.0)).xyz;
	bitangent_ws = cross(normal_ws.xyz, tangent_ws.xyz) * tangent_in.w; // Following GLTF spec
#endif

#else // else if !INSTANCE_MATRICES:

	mat4 model_skin_matrix;
	mat4 normal_skin_matrix;
#if SKINNING
	// See https://www.khronos.org/files/gltf20-reference-guide.pdf
	mat4 skin_matrix =
		weight.x * joint_matrix[joints_base_index + joint.x] +
		weight.y * joint_matrix[joints_base_index + joint.y] +
		weight.z * joint_matrix[joints_base_index + joint.z] +
		weight.w * joint_matrix[joints_base_index + joint.w];


	model_skin_matrix = model_matrix * skin_matrix;
	normal_skin_matrix = normal_matrix * skin_matrix;// * transpose(skin_matrix);
#else
	model_skin_matrix = model_matrix;
	normal_skin_matrix = normal_matrix;
#endif

	vec3 final_pos_os = position_in;
	vec3 final_normal_in = normal_in;

	pos_ws = (model_skin_matrix  * vec4(final_pos_os, 1.0)).xyz;

	normal_ws = (normal_skin_matrix * vec4(final_normal_in, 0.0)).xyz;

#if VERT_TANGENTS
	tangent_ws = (normal_skin_matrix * vec4(tangent_in.xyz, 0.0)).xyz;
	bitangent_ws = cross(normal_ws.xyz, tangent_ws.xyz) * tangent_in.w; // Following GLTF spec
#endif


#if USE_WIND_VERT_SHADER
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
#endif // end if USE_WIND_VERT_SHADER

#if GENERATE_PLANAR_UVS
	pos_os = final_pos_os;
#endif

	cam_to_pos_ws = pos_ws - campos_ws.xyz;

	vec4 pos_cs_vec4 = view_matrix * vec4(pos_ws, 1.0);
	pos_cs = pos_cs_vec4.xyz;

	gl_Position = proj_matrix * pos_cs_vec4;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * vec4(pos_ws, 1.0)).xyz;
#endif

#endif // end if !INSTANCE_MATRICES

#if TERRAIN
	texture_coords = pos_ws.xy;
#else
	texture_coords = texture_coords_0_in;
#endif

#if VERT_COLOURS
	vert_colour = vert_colours_in;

#if USE_WIND_VERT_SHADER
	vert_colour = vec3(1.f); // Vert colours are for encoding wind deformation amplitude etc.., so render as white
#endif

#endif

#if LIGHTMAPPING
	lightmap_coords = lightmap_coords_in;
#endif

#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	light_indices_0 = per_object_data[per_ob_data_index].light_indices_0;
	light_indices_1 = per_object_data[per_ob_data_index].light_indices_1;
#else
	light_indices_0 = per_object_data.light_indices_0;
	light_indices_1 = per_object_data.light_indices_1;
#endif

#if DECAL
	/*
	We need to compute the world-to-object matrix for decals.
	ob_to_world = T R S
	world_to_ob = S^-1 R^-1 T^-1

	normal_ob_to_world = (R S)^-1^T
	= (S^-1 R^-1)^T

	normal_ob_to_world^T = (S^-1 R^-1)^T^T = S^-1 R^-1
	world_to_ob = normal_ob_to_world^T T^-1
	*/
	mat4 inverse_translation;
	inverse_translation[0] = vec4(1,0,0,0);
	inverse_translation[1] = vec4(0,1,0,0);
	inverse_translation[2] = vec4(0,0,1,0);
	inverse_translation[3] = vec4(-model_skin_matrix[3].xyz, 1.0); // set col 3
	world_to_ob = transpose(normal_skin_matrix) * inverse_translation;
#endif
}
