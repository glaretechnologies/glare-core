#version 150

// For capturing depth data for shadow mapping

in vec3 position_in;

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;

void main()
{
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));
}
