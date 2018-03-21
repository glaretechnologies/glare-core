#version 150


in vec2 texture_coords;
uniform sampler2D diffuse_tex;
uniform mat4 texture_matrix;

void main()
{
#if ALPHA_TEST
	vec4 col = texture(diffuse_tex, (texture_matrix * vec4(texture_coords.x, texture_coords.y, 0.0, 1.0)).xy);
	if(col.a < 0.5f)
		discard;
#endif
}
