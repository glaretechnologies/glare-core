#version 150

in vec3 position_in;
in vec3 normal_in;

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;
uniform mat3 shadow_texture_matrix;
uniform float time;


void main()
{
	gl_Position = proj_matrix  * (view_matrix * (model_matrix * vec4(position_in, 1.0)));
}

