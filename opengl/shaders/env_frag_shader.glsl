#version 150

in vec3 pos_cs;
in vec2 texture_coords;

uniform vec4 sundir_cs;
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

	// Render sun
	vec4 suncol = vec4(9124154304.569067, 8038831044.193394, 7154376815.37873, 1);
	float d = dot(sundir_cs.xyz, normalize(pos_cs));
	col = mix(col, suncol, smoothstep(0.9999, 0.9999892083461507, d));

	col *= 0.0000000004;
	float gamma = 0.45;
	colour_out = vec4(pow(col.x, gamma), pow(col.y, gamma), pow(col.z, gamma), 1);
}
