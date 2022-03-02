
#if USE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

in vec2 texture_coords;


#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
#endif

#if USE_MULTIDRAW_ELEMENTS_INDIRECT

in flat int material_index;

struct DepthUniformsStruct
{
	mat3 texture_matrix;
#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
#else
	int padding0;
	int padding1;
#endif
};

layout(std140) uniform DepthUniforms // MaterialDataUBO
{
	DepthUniformsStruct material_data[256];
};

#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform DepthUniforms
{
	mat3 texture_matrix;
#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
#else
	int padding0;
	int padding1;
#endif
} mat_data;

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT


void main()
{
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	DepthUniformsStruct mat_data = material_data[material_index];
#endif

#if ALPHA_TEST
	vec4 col = texture(mat_data.diffuse_tex, (mat_data.texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
	if(col.a < 0.5f)
		discard;
#endif
}
