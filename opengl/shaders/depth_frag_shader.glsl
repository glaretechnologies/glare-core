
#if USE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

in vec2 texture_coords;


#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
#endif

layout (std140) uniform DepthUniforms
{
	mat3 texture_matrix;
#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
#else
	int padding0;
	int padding1;
#endif
};

void main()
{
#if ALPHA_TEST
	vec4 col = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
	if(col.a < 0.5f)
		discard;
#endif
}
