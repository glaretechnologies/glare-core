#version 150

in vec3 position_in; // object-space position
in vec3 normal_in;
in vec2 texture_coords_0_in;
#if VERT_COLOURS
in vec3 vert_colours_in;
#endif
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif
#if LIGHTMAPPING
in vec2 lightmap_coords_in;
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

uniform mat4 proj_matrix; // same for all objects
uniform mat4 model_matrix; // per-object
uniform mat4 view_matrix; // same for all objects
uniform mat4 normal_matrix; // per-object
#if NUM_DEPTH_TEXTURES > 0
uniform mat4 shadow_texture_matrix[NUM_DEPTH_TEXTURES]; // same for all objects
#endif
uniform vec3 campos_ws; // same for all objects


void main()
{
#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * model_matrix * vec4(position_in, 1.0)));

	pos_ws = (instance_matrix_in * model_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (instance_matrix_in * model_matrix  * vec4(position_in, 1.0))).xyz;

	normal_ws = (instance_matrix_in * normal_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (instance_matrix_in * normal_matrix * vec4(normal_in, 0.0))).xyz;
#else
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

#if GENERATE_PLANAR_UVS
	pos_os = position_in;
#endif

	pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;

	normal_ws = (normal_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;
#endif

	texture_coords = texture_coords_0_in;

#if VERT_COLOURS
	vert_colour = vert_colours_in;
#endif

#if NUM_DEPTH_TEXTURES > 0
	for(int i=0; i<NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (model_matrix  * vec4(position_in, 1.0))).xyz;
#endif

#if LIGHTMAPPING
	lightmap_coords = lightmap_coords_in;
#endif
}
