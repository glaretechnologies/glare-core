#version 150

in vec2 texture_coords;

uniform vec4 diffuse_colour;
uniform int have_texture;
uniform sampler2D diffuse_tex;
uniform mat3 texture_matrix;

out vec4 colour_out;

void main()
{
	vec4 col;
	if(have_texture != 0)
		col = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
	else
		col = diffuse_colour;

	colour_out = col;
}
