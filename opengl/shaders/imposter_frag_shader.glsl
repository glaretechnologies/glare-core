
in vec3 normal_cs;
in vec3 normal_ws;
in vec3 pos_cs;
in vec3 pos_ws;
in vec2 texture_coords;
in float imposter_rot;
#if NUM_DEPTH_TEXTURES > 0
in vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
in vec3 cam_to_pos_ws;

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D normal_map;
#endif
uniform sampler2DShadow dynamic_depth_tex;
uniform sampler2DShadow static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;


layout (std140) uniform PhongUniforms
{
	MaterialData matdata;
};


#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX					matdata.diffuse_tex
#define NORMAL_MAP					matdata.normal_map

#else
#define DIFFUSE_TEX					diffuse_tex
#define NORMAL_MAP					normal_map
#endif


layout(location = 0) out vec4 colour_out;
#if NORMAL_TEXTURE_IS_UINT
layout(location = 1) out uvec4 normal_out;
#else
layout(location = 1) out vec3 normal_out;
#endif


float fresnelApproxCheap(float cos_theta_i, float n2)
{
	float r_0 = square((1.0 - n2) / (1.0 + n2));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i); // https://en.wikipedia.org/wiki/Schlick%27s_approximation
}




void main()
{
	vec3 use_normal_ws;
	vec2 use_texture_coords = texture_coords;
	if((matdata.flags & HAVE_SHADING_NORMALS_FLAG) != 0)
	{
		use_normal_ws = normal_ws;
	}
	else
	{
		// Compute world-space geometric normal.
		vec3 dp_dx = dFdx(pos_ws);
		vec3 dp_dy = dFdy(pos_ws);
		vec3 N_g = cross(dp_dx, dp_dy);
		use_normal_ws = N_g;
	}

	// TEMP: get normals from normal map
	if((matdata.flags & HAVE_NORMAL_MAP_FLAG) != 0)
		use_normal_ws = texture(NORMAL_MAP,  matdata.texture_upper_left_matrix_col0 * use_texture_coords.x + matdata.texture_upper_left_matrix_col1 * use_texture_coords.y + matdata.texture_matrix_translation).xyz * 2.0 - 
			vec3(1,1,1);

	// Rotate normals vector around z-axis:
	float phi = imposter_rot;

	float new_x = cos(phi) * use_normal_ws.x - sin(phi) * use_normal_ws.y;
	float new_y = sin(phi) * use_normal_ws.x + cos(phi) * use_normal_ws.y;
	use_normal_ws.xy = vec2(new_x, new_y);

	use_normal_ws = normalize(use_normal_ws);

	float sun_light_cos_theta_factor = 1.0;

	vec4 specular = vec4(0.0);
	if((matdata.flags & HAVE_NORMAL_MAP_FLAG) != 0) // If grass (and so if have normal map with decent normals):
	{
		float final_fresnel_scale = 0.6;
		float grass_IOR = 1.33;
		float final_roughness = 0.4;

		vec3 frag_to_cam = normalize(-cam_to_pos_ws);
		vec3 h_ws = normalize(frag_to_cam + sundir_ws.xyz);
		float h_cos_theta = abs(dot(h_ws, use_normal_ws));
		vec4 dielectric_fresnel = vec4(fresnelApproxCheap(h_cos_theta, grass_IOR)) * final_fresnel_scale;
		specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(final_roughness))) * dielectric_fresnel;


		sun_light_cos_theta_factor = abs(dot(use_normal_ws, sundir_ws.xyz));
	}

	// Work out which imposter sprite to use
	vec3 to_frag_proj = normalize(vec3(pos_cs.x, 0.f, pos_cs.z)); // camera to fragment vector, projected onto ground plane
	vec3 to_sun_proj = normalize(vec3(sundir_cs.x, 0.f, sundir_cs.z)); // fragment to sun vector, projected onto ground plane
	vec3 right_cs = cross(to_frag_proj, vec3(0,1,0)); // right vector, projected onto ground plane

	float r = dot(right_cs, to_sun_proj);

	int sprite_a, sprite_b;
	float sprite_blend_frac;
	if(dot(pos_cs, sundir_cs.xyz) > 0.0) // Backlighting
	{
		if(r > 0.0)
		{
			sprite_a = 3; // rightlit
			sprite_b = 0; // backlit

			sprite_blend_frac = 1.0 - r;
		}
		else
		{
			sprite_a = 2; // leftlit
			sprite_b = 0; // backlit

			sprite_blend_frac = 1.0 + r; // 1 - -r
		}
	}
	else // Else frontlighting
	{
		if(r > 0.0)
		{
			sprite_a = 3; // rightlit
			sprite_b = 1; // frontlit
			
			sprite_blend_frac = 1.0 - r;
		}
		else
		{
			sprite_a = 2; // leftlit
			sprite_b = 1; // frontlit

			sprite_blend_frac = 1.0 + r; // 1 - -r
		}
	}

	float sprite_a_x = 0.25f * float(sprite_a) + use_texture_coords.x * 0.25f;
	float sprite_b_x = 0.25f * float(sprite_b) + use_texture_coords.x * 0.25f;

	vec4 texture_diffuse_col;
	if((matdata.flags & HAVE_TEXTURE_FLAG) != 0) //if(have_texture != 0)
	{
		if((matdata.flags & IMPOSTER_TEX_HAS_MULTIPLE_ANGLES) != 0)
		{
			vec2 tex_coords_a = matdata.texture_upper_left_matrix_col0 * sprite_a_x + matdata.texture_upper_left_matrix_col1 * use_texture_coords.y + matdata.texture_matrix_translation;
			vec2 tex_coords_b = matdata.texture_upper_left_matrix_col0 * sprite_b_x + matdata.texture_upper_left_matrix_col1 * use_texture_coords.y + matdata.texture_matrix_translation;
			vec4 col_a = texture(DIFFUSE_TEX, tex_coords_a);
			vec4 col_b = texture(DIFFUSE_TEX, tex_coords_b);

			texture_diffuse_col = mix(col_a, col_b, sprite_blend_frac);
		}
		else
			texture_diffuse_col = texture(DIFFUSE_TEX, matdata.texture_upper_left_matrix_col0 * use_texture_coords.x + matdata.texture_upper_left_matrix_col1 * use_texture_coords.y + matdata.texture_matrix_translation);

		if((matdata.flags & CONVERT_ALBEDO_FROM_SRGB_FLAG) != 0)
			texture_diffuse_col.xyz = fastApproxNonLinearSRGBToLinearSRGB(texture_diffuse_col.xyz);
	}
	else
		texture_diffuse_col = vec4(1.f);

	vec4 diffuse_col = texture_diffuse_col * matdata.diffuse_colour; // diffuse_colour is linear sRGB already.
	diffuse_col.xyz *= 0.8f; // Just a hack scale to make the brightnesses look similar

	vec4 refl_diffuse_col = diffuse_col;
	if((matdata.flags & IMPOSTER_TEX_HAS_MULTIPLE_ANGLES) == 0) // TEMP HACK If grass:
	{
		vec3 frag_to_cam_vec_ws = -cam_to_pos_ws;
		vec3 frag_to_sun_vec_ws = sundir_ws.xyz;

		if(dot(use_normal_ws, frag_to_cam_vec_ws) * dot(use_normal_ws, frag_to_sun_vec_ws) < 0.0)//dot(pos_cs, sundir_cs.xyz) > 0) // Backlighting
		{
			diffuse_col.xyz = vec3(0.21756086,0.35085827,0.0013999827); // grass_trans_linear_actual
		}
		else // Else frontlighting
		{
			diffuse_col.xyz = vec3(0.04895491,0.10686976,0.010306382); // grass_refl_linear_actual
		}

		refl_diffuse_col = vec4(0.04895491,0.10686976,0.010306382, 0.0);
	}

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.f / 128.f)).x;

	float begin_fade_in_distance  = matdata.materialise_lower_z;
	float end_fade_in_distance    = matdata.materialise_upper_z;
	// 1.001 factor is to make sure is > 1 when should be visible.
	float dist_alpha_factor = (smoothstep(begin_fade_in_distance, end_fade_in_distance, /*dist=*/-pos_cs.z) - smoothstep(matdata.begin_fade_out_distance, matdata.end_fade_out_distance, /*dist=*/-pos_cs.z)) * 1.001; 
	if(pixel_hash > dist_alpha_factor)
	 	discard;

#if ALPHA_TEST
	if(diffuse_col.a < 0.5f)
		discard;
#endif

	// Shadow mapping
#if SHADOW_MAPPING
	float sun_vis_factor = getShadowMappingSunVisFactor(shadow_tex_coords, dynamic_depth_tex, static_depth_tex, pixel_hash, pos_cs, shadow_map_samples_xy_scale);
#else
	float sun_vis_factor = 1.0;
#endif

	// Apply cloud shadows
#if RENDER_CLOUD_SHADOWS
	if(((mat_common_flags & CLOUD_SHADOWS_FLAG) != 0) && (pos_ws.z < 1000.f))
	{
		// Compute position on cumulus cloud layer
		vec3 cum_layer_pos = pos_ws + sundir_ws.xyz * (1000.f - pos_ws.z) / sundir_ws.z;

		vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
		cum_tex_coords.x += time * 0.002;

		vec2 cumulus_coords = vec2(cum_tex_coords.x * 2.0 + 2.3453, cum_tex_coords.y * 2.0 + 1.4354);
		float cumulus_val = max(0.f, fbmMix(cumulus_coords, fbm_tex) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}
#endif

	vec3 unit_normal_ws = normalize(use_normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0.0)
		unit_normal_ws = -unit_normal_ws;

	vec4 sky_irradiance;
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz); // integral over hemisphere of cosine * incoming radiance from sky * 1.0e-9.


	vec4 sun_light = sun_spec_rad_times_solid_angle * sun_vis_factor; // Sun spectral radiance multiplied by solid angle, see SkyModel2Generator::makeSkyEnvMap().

	vec4 col =
		sky_irradiance * refl_diffuse_col * (1.0 / 3.141592653589793) +  // Diffuse substrate part of BRDF * incoming radiance from sky
		sun_light * diffuse_col * (1.0 / 3.141592653589793) * sun_light_cos_theta_factor + //  Diffuse substrate part of BRDF * sun light
		sun_light * specular; // sun light * specular microfacet terms

#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	vec3 transmission = exp(air_scattering_coeffs.xyz * -dist_);

	col.xyz *= transmission;
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1.0 - transmission);
#endif

#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, 1.0);
#else
	colour_out = vec4(toneMapToNonLinear(col.xyz), 1.0);
#endif
	colour_out.w = 1.0; // Imposters aren't rendered with alpha blending, so just use alpha=1.

	// TODO: use normal_ws and snorm12x2_to_unorm8x3 etc.
#if NORMAL_TEXTURE_IS_UINT
	normal_out = uvec4(0, 0, 0, 0);
#else
	normal_out = vec3(0.0, 0.0, 0.0);
#endif
}
