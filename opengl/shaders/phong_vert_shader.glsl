#version 330 core

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

layout (std140) uniform SharedVertUniforms
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

#if SKINNING
uniform mat4 joint_matrix[256];
#endif


float square(float x) { return x * x; }

void main()
{
#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (instance_matrix_in * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (instance_matrix_in * vec4(position_in, 1.0))).xyz;

	normal_ws = (instance_matrix_in * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (instance_matrix_in * vec4(normal_in, 0.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (instance_matrix_in * vec4(position_in, 1.0))).xyz;
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

	gl_Position = proj_matrix * (view_matrix * (model_skin_matrix * vec4(position_in, 1.0)));

	normal_ws = (normal_skin_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_skin_matrix * vec4(normal_in, 0.0))).xyz;

#if USE_WIND_VERT_SHADER
	vec4 temp_pos_ws = model_skin_matrix * vec4(position_in, 1.0);
	vec3 use_normal = normal_ws;
	if(use_normal.z < 0.f)
		use_normal = -use_normal;
	float ob_phase = temp_pos_ws.x * 0.1f;
	//float wind_gust_str = texture(fbm_tex, vec2(vert_uniforms_time, 0.5f)).x; // 1.f + sin(vert_uniforms_time * 1.f);
	float wind_gust_str = (sin(vert_uniforms_time * 0.8654f + ob_phase) + sin(vert_uniforms_time * 0.343243f + ob_phase) + 2.5f) * (1.f / 4.5f); // Should be in [0, 1]
	//float wind_gust_str = (1.f + sin(vert_uniforms_time * 1.f)) * (1.f + sin(vert_uniforms_time * 0.2f)) * 0.25f;
	//temp_pos_ws.xyz += use_normal * sin(temp_pos_ws.x + vert_colours_in.y * 6.2f + vert_uniforms_time * 30.f) * (vert_colours_in.x) * 0.2f * (texture_coords_0_in.x - 0.5f) * wind_gust_str;
	temp_pos_ws.xyz += use_normal * /*sin(temp_pos_ws.x + temp_pos_ws.y * 100.f) **/ sin(vert_colours_in.y * 6.2f + vert_uniforms_time * 6.f) *  // assuming vert_colours_in.y can be used as phase.
		(vert_colours_in.x) * ((texture_coords_0_in.x - texture_coords_0_in.y) - 0.5f) * wind_gust_str * 0.2f; // Branch/leaf movement up and down in normal direction
	temp_pos_ws.x -= square(square(square(wind_gust_str))) * square(position_in.y)/*temp_pos_ws.z * temp_pos_ws.z*/ * 0.05f; // tree bend
	gl_Position = proj_matrix * (view_matrix * temp_pos_ws);
#endif

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (model_skin_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws.xyz;
	pos_cs = (view_matrix * (model_skin_matrix  * vec4(position_in, 1.0))).xyz;

#if NUM_DEPTH_TEXTURES > 0
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (model_skin_matrix * vec4(position_in, 1.0))).xyz;
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
