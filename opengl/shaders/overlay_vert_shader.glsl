#version 150

in vec3 position_in;

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;

void main()
{
	gl_Position = model_matrix * vec4(position_in, 1.0);
}
