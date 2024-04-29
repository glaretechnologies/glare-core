
in vec3 position_in;
in vec2 texture_coords_0_in;

out vec3 pos;
out vec2 texture_coords;

uniform mat4 model_matrix;

void main()
{
	vec4 pos4 = model_matrix * vec4(position_in, 1.0);
	pos = pos4.xyz;
	gl_Position = pos4;

	texture_coords = texture_coords_0_in;
}
