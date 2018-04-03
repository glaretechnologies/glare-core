#version 150

in vec3 position_in;
in vec3 normal_in;
in vec2 texture_coords_0_in;

out vec3 normal; // cam (view) space
out vec3 pos_cs;
out vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
out vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;
#if NUM_DEPTH_TEXTURES > 0
uniform mat4 shadow_texture_matrix[NUM_DEPTH_TEXTURES];
#endif


void main()
{
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;

	normal = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;

	texture_coords = texture_coords_0_in;

#if NUM_DEPTH_TEXTURES > 0
	for(int i=0; i<NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (shadow_texture_matrix[i] * (model_matrix  * vec4(position_in, 1.0))).xyz;
#endif
}
