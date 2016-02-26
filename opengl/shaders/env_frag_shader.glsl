varying vec4 texture_coords;

uniform vec4 diffuse_colour;
uniform int have_texture;
uniform sampler2D diffuse_tex;
uniform mat4 texture_matrix;

void main()
{
	vec4 col;
	if(have_texture != 0)
		col = texture2D(diffuse_tex, texture_matrix * texture_coords);
	else
		col = diffuse_colour;

	gl_FragColor = col;
}
