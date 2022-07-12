
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
in vec4 joint;
in vec4 weight;
#endif

out vec3 normal_cs; // cam (view) space
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
out flat int material_index;
#endif

layout (std140) uniform SharedVertUniforms
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


#if USE_MULTIDRAW_ELEMENTS_INDIRECT

struct PerObjectVertUniformsStruct
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};


layout(std140) uniform PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[256];
};

#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PerObjectVertUniforms
{
	mat4 model_matrix; // per-object
	mat4 normal_matrix; // per-object
};

#endif // !USE_MULTIDRAW_ELEMENTS_INDIRECT

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
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	material_index = gl_DrawID;
	mat4 model_matrix  = per_object_data[material_index].model_matrix;
	mat4 normal_matrix = per_object_data[material_index].normal_matrix;
#endif

#if INSTANCE_MATRICES

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * vec4(normal_ws, 0.0)).xyz;

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

#else // else if !INSTANCE_MATRICES:

	mat4 model_skin_matrix;
	mat4 normal_skin_matrix;
#if SKINNING
	// See https://www.khronos.org/files/gltf20-reference-guide.pdf
	mat4 skin_matrix =
		weight.x * joint_matrix[int(joint.x)] +
		weight.y * joint_matrix[int(joint.y)] +
		weight.z * joint_matrix[int(joint.z)] +
		weight.w * joint_matrix[int(joint.w)];


	model_skin_matrix = model_matrix * skin_matrix;
	normal_skin_matrix = normal_matrix * skin_matrix;// * transpose(skin_matrix);
#else
	model_skin_matrix = model_matrix;
	normal_skin_matrix = normal_matrix;
#endif

	
	normal_ws = (normal_skin_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_skin_matrix * vec4(normal_in, 0.0))).xyz;

	pos_ws = (model_skin_matrix  * vec4(position_in, 1.0)).xyz;

#if USE_WIND_VERT_SHADER
	pos_ws = newPosGivenWind(pos_ws, normal_ws);
#endif // end if USE_WIND_VERT_SHADER

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
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

	texture_coords = texture_coords_0_in;

#if VERT_COLOURS
	vert_colour = vert_colours_in;

#if USE_WIND_VERT_SHADER
	vert_colour = vec3(1.f); // Vert colours are for encoding wind deformation amplitude etc.., so render as white
#endif

#endif

#if LIGHTMAPPING
	lightmap_coords = lightmap_coords_in;
#endif
}
