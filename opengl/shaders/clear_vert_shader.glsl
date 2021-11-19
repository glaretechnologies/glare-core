#version 330 core

in vec3 position_in;
uniform mat4 model_matrix;

void main()
{
	gl_Position = model_matrix * vec4(position_in, 1.0);
}
