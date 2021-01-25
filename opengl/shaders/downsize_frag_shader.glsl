#version 150

uniform sampler2D albedo_texture;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;

void main()
{
	vec4 texcol = texture(albedo_texture, pos);

	colour_out = vec4(texcol.x, texcol.y, texcol.z, 1);
}

