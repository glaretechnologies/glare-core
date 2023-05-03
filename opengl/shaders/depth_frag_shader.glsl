
#if USE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

in vec2 texture_coords;
in vec3 normal_ws;
in vec3 pos_ws;

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
#endif

uniform sampler2D fbm_tex;


layout (std140) uniform MaterialCommonUniforms
{
	vec4 sundir_cs;
	float time;
};


//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

in flat int material_index;

struct MaterialData
{
	vec4 diffuse_colour;
	vec4 emission_colour;
	vec2 texture_upper_left_matrix_col0;
	vec2 texture_upper_left_matrix_col1;
	vec2 texture_matrix_translation;

#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
	sampler2D metallic_roughness_tex;
	sampler2D lightmap_tex;
	sampler2D emission_tex;
	sampler2D backface_albedo_tex;
	sampler2D transmission_tex;
#else
	float padding0;
	float padding1;
	float padding2;
	float padding3;
	float padding4;
	float padding5;
	float padding6;
	float padding7;
	float padding8;
	float padding9;
	float padding10;
	float padding11;
#endif

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

	ivec4 light_indices_0;
	ivec4 light_indices_1;
};


layout(std430) buffer PhongUniforms
{
	MaterialData material_data[];
};


#define MAT_UNIFORM					material_data[material_index]

#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define METALLIC_ROUGHNESS_TEX		MAT_UNIFORM.metallic_roughness_tex
#define LIGHTMAP_TEX				MAT_UNIFORM.lightmap_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex


//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform DepthUniforms
{
	vec2 texture_upper_left_matrix_col0;
	vec2 texture_upper_left_matrix_col1;
	vec2 texture_matrix_translation;

#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
#else
	int padding0;
	int padding1;
#endif

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

} mat_data;

#define MAT_UNIFORM mat_data

#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX mat_data.diffuse_tex
#else
#define DIFFUSE_TEX diffuse_tex
#endif

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------




#if MATERIALISE_EFFECT
float length2(vec2 v) { return dot(v, v); }

float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2)) * 0.5 +
		0;
}

// https://www.shadertoy.com/view/MdcfDj
#define M1 1597334677U     //1719413*929
#define M2 3812015801U     //140473*2467*11

float hash( uvec2 q )
{
	q *= uvec2(M1, M2); 

	uint n = (q.x ^ q.y) * M1;

	return float(n) * (1.0/float(0xffffffffU));
}

// https://www.shadertoy.com/view/cscSW8
#define SQRT_3 1.7320508

vec2 closestHexCentre(vec2 p)
{
	vec2 grid_p = vec2(p.x, p.y * (1.0 / SQRT_3));

	// Alternating rows of hexagon centres form their own rectanglular lattices.
	// Find closest hexagon centre on each lattice.
	vec2 p_1 = (floor((grid_p + vec2(1,1)) * 0.5) * 2.0            ) * vec2(1, SQRT_3);
	vec2 p_2 = (floor( grid_p              * 0.5) * 2.0 + vec2(1,1)) * vec2(1, SQRT_3);

	// Now return the closest centre from the two lattices.
	float d_1 = length2(p - p_1);
	float d_2 = length2(p - p_2);
	return d_1 < d_2 ? p_1 : p_2;
}
#endif // MATERIALISE_EFFECT


void main()
{
#if ALPHA_TEST
	vec2 use_texture_coords = texture_coords; // TODO: Add GENERATE_PLANAR_UVS support
	vec2 main_tex_coords = MAT_UNIFORM.texture_upper_left_matrix_col0 * use_texture_coords.x + MAT_UNIFORM.texture_upper_left_matrix_col1 * use_texture_coords.y + MAT_UNIFORM.texture_matrix_translation;
	vec4 col = texture(DIFFUSE_TEX, main_tex_coords);
	if(col.a < 0.5f)
		discard;
#endif

#if MATERIALISE_EFFECT
	// box mapping
	vec2 materialise_coords;
	if(abs(normal_ws.x) > abs(normal_ws.y))
	{
		if(abs(normal_ws.x) > abs(normal_ws.z)) // |x| > |z| && |x| > |y|
			materialise_coords = pos_ws.yz;
		else // |z| >= |x| > |y|:
			materialise_coords = pos_ws.xy;
	}
	else // else |y| >= |x|:
	{
		if(abs(normal_ws.y) > abs(normal_ws.z))
			materialise_coords = pos_ws.xz;
		else // z >= y >= x
			materialise_coords = pos_ws.xy;
	}

	float sweep_speed_factor = 1.0;
	float sweep_frac = (pos_ws.z - MAT_UNIFORM.materialise_lower_z) / (MAT_UNIFORM.materialise_upper_z - MAT_UNIFORM.materialise_lower_z);
	float materialise_stage = fbmMix(materialise_coords * 0.2) * 0.4 + sweep_frac;
	float use_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor - materialise_stage - 0.3;

	float band_1_centre = 0.1;
	float band_2_centre = 0.8;

	if(use_frac < band_1_centre)
		discard;
	if(use_frac >= band_1_centre /*+ 0.05*/ && use_frac < band_2_centre)
	{
		vec2 hex_centre = closestHexCentre(materialise_coords * 40.0);

		float materialise_pixel_hash = hash(uvec2(hex_centre + vec2(100000.0,100000.0)));

		float hex_interior_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor;// - materialise_stage - 0.3;
		if(hex_interior_frac < materialise_pixel_hash + /*fill-in delay=*/0.7 + sweep_frac)
			discard;
	}
#endif // MATERIALISE_EFFECT
}
