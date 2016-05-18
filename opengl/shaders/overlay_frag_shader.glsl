#version 150

uniform vec4 diffuse_colour;
uniform mat4 texture_matrix;
uniform sampler2D diffuse_tex;
uniform int have_texture;

in vec2 texture_coords;

out vec4 colour_out;

void main()
{
	if(have_texture != 0)
	{
		vec4 texcol = texture(diffuse_tex, (texture_matrix * vec4(texture_coords.x, texture_coords.y, 0.0, 1.0)).xy) * 1.5;
		colour_out = vec4(texcol.x, texcol.y, texcol.z, 1);
	}
	else
		colour_out = diffuse_colour;
}
