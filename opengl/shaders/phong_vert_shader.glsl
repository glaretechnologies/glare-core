#version 150

in vec3 position_in;
in vec3 normal_in;
in vec2 texture_coords_0_in;

#if VERT_COLOURS
in vec3 vert_colours_in;
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

#if VERT_COLOURS
out vec3 vert_colour;
#endif

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;
#if NUM_DEPTH_TEXTURES > 0
uniform mat4 shadow_texture_matrix[NUM_DEPTH_TEXTURES];
#endif
uniform vec3 campos_ws;


void main()
{
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

	pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;

	normal_ws = (normal_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;

	texture_coords = texture_coords_0_in;

#if VERT_COLOURS
	vert_colour = vert_colours_in;
#endif

#if NUM_DEPTH_TEXTURES > 0
	for(int i=0; i<NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (model_matrix  * vec4(position_in, 1.0))).xyz;
#endif
}
