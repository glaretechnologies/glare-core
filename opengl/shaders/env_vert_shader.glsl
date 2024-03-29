
in vec3 position_in;
//in vec3 normal_in;
in vec2 texture_coords_0_in;

//out vec3 normal; // cam (view) space
out vec3 pos_cs;
out vec2 texture_coords;
out vec3 dir_ws;

uniform mat4 proj_matrix;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 normal_matrix;


void main()
{
	gl_Position = proj_matrix * (view_matrix * (model_matrix * vec4(position_in, 1.0)));

	pos_cs = (view_matrix * (model_matrix  * vec4(position_in, 1.0))).xyz;

	dir_ws = (model_matrix * vec4(position_in, 1.0)).xyz; // dir from camera to frag (ws)
 
	//normal = (view_matrix * (normal_matrix * vec4(normal_in, 0.0))).xyz;

	texture_coords = texture_coords_0_in;
}
