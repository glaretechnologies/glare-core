#version 150

// For capturing depth data for shadow mapping

in vec3 position_in;
in vec2 texture_coords_0_in;

out vec2 texture_coords;

uniform mat4 proj_view_model_matrix; 

void main()
{
	gl_Position = proj_view_model_matrix * vec4(position_in, 1.0);

	texture_coords = texture_coords_0_in;
}
