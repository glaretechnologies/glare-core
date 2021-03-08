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

#if DEPTH_FOG
	// Blend lower hemisphere into a colour that matches fogged ground quad in Substrata
	// Chosen by hand to match the fogged phong colour at ~2km (edge of ground quad)
	vec4 lower_hemis_col = vec4(pow(6.5, 2.2), pow(6.8, 2.2), pow(7.3, 2.2), 1.0) * 1.6e7;

	float lower_hemis_factor = smoothstep(1.52, 1.6, texture_coords.y);
	col = mix(col, lower_hemis_col, lower_hemis_factor);
#endif

	col *= 0.0000000005;
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
