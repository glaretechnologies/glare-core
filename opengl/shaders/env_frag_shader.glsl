#version 150

in vec3 pos_cs;
in vec2 texture_coords;

uniform vec4 sundir_cs;
uniform vec4 diffuse_colour;
uniform int have_texture;
uniform sampler2D diffuse_tex;
uniform mat3 texture_matrix;

out vec4 colour_out;


vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}


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
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
