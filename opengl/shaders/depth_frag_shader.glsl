
in vec2 texture_coords;
in vec3 normal_ws;
in vec3 pos_ws;

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
#endif

uniform sampler2D fbm_tex;
uniform sampler2D blue_noise_tex;

//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

flat in int material_index;

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

	int flags;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

	float padding_d0;
	float padding_d1;

} mat_data;

#define MAT_UNIFORM mat_data

#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX mat_data.diffuse_tex
#else
#define DIFFUSE_TEX diffuse_tex
#endif

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------




void main()
{
#if ALPHA_TEST
	vec2 use_texture_coords = texture_coords; // TODO: Add GENERATE_PLANAR_UVS support
	
	#if IMPOSTER
	if((MAT_UNIFORM.flags & IMPOSTER_TEX_HAS_MULTIPLE_ANGLES) != 0)
		use_texture_coords.x *= 0.25; // Pick one of the sprites from the sprite-sheet
	#endif

	vec2 main_tex_coords = MAT_UNIFORM.texture_upper_left_matrix_col0 * use_texture_coords.x + MAT_UNIFORM.texture_upper_left_matrix_col1 * use_texture_coords.y + MAT_UNIFORM.texture_matrix_translation;
	vec4 col = texture(DIFFUSE_TEX, main_tex_coords);
	if(col.a < 0.5f)
		discard;
#endif // ALPHA_TEST

#if SDF_TEXT
	// For rendering SDF text, don't bother with alpha-blending, just use alpha testing.
	vec4 col = texture(DIFFUSE_TEX, texture_coords);
	if(col.a < 0.5f)
		discard;
#endif


#if IMPOSTER || IMPOSTERABLE
	vec3 pos_cs = (frag_view_matrix * vec4(pos_ws, 1.0)).xyz;
	float depth = -pos_cs.z;
	
	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.f / 128.f)).x;

#if IMPOSTER
	float begin_fade_in_distance  = MAT_UNIFORM.materialise_lower_z;
	float end_fade_in_distance    = MAT_UNIFORM.materialise_upper_z;
	float dist_alpha_factor = (smoothstep(begin_fade_in_distance, end_fade_in_distance, depth) - smoothstep(MAT_UNIFORM.begin_fade_out_distance, MAT_UNIFORM.end_fade_out_distance, depth)) * 1.001; 
	if(dist_alpha_factor <= pixel_hash) // Draw imposter only when dist_alpha_factor > pixel_hash, draw real object when dist_alpha_factor <= pixel_hash
		discard;
#endif
#if IMPOSTERABLE
	float dist_alpha_factor = smoothstep(MAT_UNIFORM.begin_fade_out_distance, MAT_UNIFORM.end_fade_out_distance, depth);
	if(dist_alpha_factor > pixel_hash)
		discard;
#endif

#endif // endif IMPOSTER || IMPOSTERABLE


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

	float sweep_speed_factor = 3.0;
	float sweep_frac = (pos_ws.z - MAT_UNIFORM.materialise_lower_z) / (MAT_UNIFORM.materialise_upper_z - MAT_UNIFORM.materialise_lower_z);
	float materialise_stage = fbmMix(materialise_coords * 0.2, fbm_tex) * 0.4 + sweep_frac;
	float use_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor - materialise_stage - 0.0;

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
