
uniform vec4 diffuse_colour;
uniform mat3 texture_matrix;
uniform sampler2D diffuse_tex;
uniform int have_texture;
uniform vec2 clip_min_coords;
uniform vec2 clip_max_coords;

in vec3 pos;
in vec2 texture_coords;

out vec4 colour_out;

void main()
{
	if(pos.x < clip_min_coords.x || pos.y < clip_min_coords.y || pos.x > clip_max_coords.x || pos.y > clip_max_coords.y)
		discard;

	float COLOUR_SCALE = 0.5; // to adjust for tonemapping scale

	if(have_texture != 0)
	{
		vec4 texcol = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
		colour_out = vec4(texcol.x, texcol.y, texcol.z, texcol.w) * diffuse_colour * vec4(vec3(COLOUR_SCALE), 1.0);
	}
	else
		colour_out = diffuse_colour * vec4(vec3(COLOUR_SCALE), 1.0);
}
