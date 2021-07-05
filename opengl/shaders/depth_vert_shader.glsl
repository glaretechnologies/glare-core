#version 150

// For capturing depth data for shadow mapping

in vec3 position_in;
in vec2 texture_coords_0_in;
#if INSTANCE_MATRICES
in mat4 instance_matrix_in;
#endif

out vec2 texture_coords;

uniform mat4 proj_view_model_matrix; 

#if INSTANCE_MATRICES
layout(std140) uniform SharedVertUniforms
{
	mat4 proj_matrix; // same for all objects
	mat4 view_matrix; // same for all objects
//#if NUM_DEPTH_TEXTURES > 0
	mat4 shadow_texture_matrix[5]; // same for all objects
//#endif
	vec3 campos_ws; // same for all objects
};
#endif

void main()
{
#if INSTANCE_MATRICES
	gl_Position = proj_matrix * (view_matrix * (instance_matrix_in * vec4(position_in, 1.0)));
#else
	gl_Position = proj_view_model_matrix * vec4(position_in, 1.0);
#endif

	texture_coords = texture_coords_0_in;
}
