
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
layout(location = 1) out vec3 normal_out;

vec2 samples[16] = vec2[](
	vec2(-0.00033789, -0.00072656),
	vec2(0.00056445, -0.00064844),
	vec2(0.000013672, -0.00054883),
	vec2(-0.00058594, -0.00011914),
	vec2(-0.00017773, -0.00021094),
	vec2(0.00019336, -0.00019336),
	vec2(-0.000019531, -0.000058594),
	vec2(-0.00022070, 0.00014453),
	vec2(0.000089844, 0.000068359),
	vec2(0.00036328, 0.000058594),
	vec2(0.00061328, 0.000087891),
	vec2(-0.0000078125, 0.00028906),
	vec2(-0.00089453, 0.00043750),
	vec2(-0.00022852, 0.00058984),
	vec2(0.00019336, 0.00042188),
	vec2(0.00071289, -0.00025977)
	);

vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}


float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2.0;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2.0)) * 0.5;
}

float sampleDynamicDepthMap(mat2 R, vec3 shadow_coords)
{
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z));
	}
	return sum * (1.f / 16.f);
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords)
{
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(static_depth_tex, vec3(st.x, st.y, shadow_coords.z));
	}
	return sum * (1.f / 16.f);
}

float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

float fresnelApprox(float cos_theta_i, float n2)
{
	float r_0 = square((1.0 - n2) / (1.0 + n2));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i); // https://en.wikipedia.org/wiki/Schlick%27s_approximation

	//float sintheta_i = sqrt(1 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	//float sintheta_t = sintheta_i / n2; // Use Snell's law to get sin(theta_t)
	//
	//float costheta_t = sqrt(1 - sintheta_t*sintheta_t); // Get cos(theta_t)
	//
	//float a2 = square(cos_theta_i - n2*costheta_t);
	//float b2 = square(cos_theta_i + n2*costheta_t);
	//
	//float c2 = square(n2*cos_theta_i - costheta_t);
	//float d2 = square(costheta_t + n2*cos_theta_i);
	//
	//return 0.5 * (a2*d2 + b2*c2) / (b2*d2);
}



float alpha2ForRoughness(float r)
{
	return pow4(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
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
		vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, grass_IOR)) * final_fresnel_scale;
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

#if CONVERT_ALBEDO_FROM_SRGB
		// Texture value is in non-linear sRGB, convert to linear sRGB.
		// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
		vec4 c = texture_diffuse_col;
		vec4 c2 = c * c;
		texture_diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
#endif
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
	float sun_vis_factor;
#if SHADOW_MAPPING

#define VISUALISE_CASCADES 0

	float samples_scale = 1.f;

	float pattern_theta = pixel_hash * 6.283185307179586;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta));

	sun_vis_factor = 0.0;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
		{
			float tex_0_vis = sampleDynamicDepthMap(R, shadow_tex_coords[0]);

			float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
			if(dist > edge_dist)
			{
				float tex_1_vis = sampleDynamicDepthMap(R, shadow_tex_coords[1]);

				float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT, dist);
				sun_vis_factor = mix(tex_0_vis, tex_1_vis, blend_factor);
			}
			else
				sun_vis_factor = tex_0_vis;

#if VISUALISE_CASCADES
			diffuse_col.yz *= 0.5f;
#endif
		}
		else
		{
			sun_vis_factor = sampleDynamicDepthMap(R, shadow_tex_coords[1]);

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float static_sun_vis_factor = sampleStaticDepthMap(R, static_shadow_cds); // NOTE: had 0.999f cap and bias of 0.0005: min(static_shadow_cds.z, 0.999f) - bias

				float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT, dist);
				sun_vis_factor = mix(sun_vis_factor, static_sun_vis_factor, blend_factor);
			}

#if VISUALISE_CASCADES
			diffuse_col.yz *= 0.75f;
#endif
		}
	}
	else
	{
		float l1dist = dist;

		if(l1dist < 1024.0)
		{
			int static_depth_tex_index;
			float cascade_end_dist;
			vec3 shadow_cds, next_shadow_cds;
			if(l1dist < 64.0)
			{
				static_depth_tex_index = 0;
				cascade_end_dist = 64.0;
				shadow_cds      = shadow_tex_coords[0 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
			}
			else if(l1dist < 256.0)
			{
				static_depth_tex_index = 1;
				cascade_end_dist = 256.0;
				shadow_cds      = shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
			}
			else
			{
				static_depth_tex_index = 2;
				cascade_end_dist = 1024.0;
				shadow_cds = shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = vec3(0.0); // Suppress warning about being possibly uninitialised.
			}

			sun_vis_factor = sampleStaticDepthMap(R, shadow_cds); // NOTE: had cap and bias

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds); // NOTE: had cap and bias

					float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
					sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
				}
			}
#endif

#if VISUALISE_CASCADES
			diffuse_col.xz *= float(static_depth_tex_index) / NUM_STATIC_DEPTH_TEXTURES;
#endif
		}
		else
			sun_vis_factor = 1.0;
	}

#else
	sun_vis_factor = 1.0;
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
		float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}
#endif

	vec3 unit_normal_ws = normalize(use_normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0.0)
		unit_normal_ws = -unit_normal_ws;

	vec4 sky_irradiance;
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz) * 1.0e9f; // integral over hemisphere of cosine * incoming radiance from sky.


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

	col *= 0.000000003; // tone-map

#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, 1.0);
#else
	colour_out = vec4(toNonLinear(col.xyz), 1.0);
#endif
	colour_out.w = diffuse_col.a;

	normal_out = vec3(0.0, 0.0, 0.0); // TODO: use normal_ws and snorm12x2_to_unorm8x3 etc.
}
