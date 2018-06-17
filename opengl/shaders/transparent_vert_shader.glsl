#version 150

in vec3 position_in;
in vec3 normal_in;
//in vec2 texture_coords_0_in;

out vec3 normal_cs; // cam (view) space
out vec3 normal_ws; // world space
out vec3 pos_cs;
//out vec2 texture_coords;
out vec3 cam_to_pos_ws;

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;
uniform vec3 campos_ws;


void main()
{
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

	vec3 pos_ws = (model_matrix  * vec4(position_in, 1.0)).xyz;
	cam_to_pos_ws = pos_ws - campos_ws;
	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;
 
	normal_ws = (normal_matrix * vec4(normal_in, 0.0)).xyz;
	normal_cs = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;

	//texture_coords = texture_coords_0_in;
}
