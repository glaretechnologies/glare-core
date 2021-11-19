#version 330 core

uniform vec4 diffuse_colour;
uniform mat3 texture_matrix;
uniform sampler2D diffuse_tex;
uniform int have_texture;

in vec2 texture_coords;

out vec4 colour_out;

void main()
{
	if(have_texture != 0)
	{
		vec4 texcol = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
		colour_out = vec4(texcol.x, texcol.y, texcol.z, texcol.w) * diffuse_colour;
	}
	else
		colour_out = diffuse_colour;

#if USE_LOGARITHMIC_DEPTH_BUFFER
	gl_FragDepth = -1.f; // Just use a fixed frag depth that should be in front of everything else in the scene.  Depth buffer writes are off for overlays so shouldn't affect ordering.
#endif
}
