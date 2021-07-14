#version 330 core

in vec3 normal_cs;
in vec3 normal_ws;
in vec3 pos_cs;
#if GENERATE_PLANAR_UVS
in vec3 pos_os;
#endif
in vec3 pos_ws;
in vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
in vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
in vec3 cam_to_pos_ws;
#if VERT_COLOURS
in vec3 vert_colour;
#endif
#if LIGHTMAPPING
in vec2 lightmap_coords;
#endif


uniform sampler2D diffuse_tex;
uniform sampler2D dynamic_depth_tex;
uniform sampler2D static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;
uniform sampler2D fbm_tex;
#if LIGHTMAPPING
uniform sampler2D lightmap_tex;
#endif

layout (std140) uniform PhongUniforms
{
	vec4 sundir_cs;
	vec4 diffuse_colour;
	mat3 texture_matrix;
	int have_shading_normals;
	int have_texture;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float time;
};


out vec4 colour_out;


float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

float alpha2ForRoughness(float r)
{
	return pow6(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float fresnelApprox(float cos_theta_i, float n2)
{
	//float r_0 = square((1.0 - n2) / (1.0 + n2));
	//return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i);

	float sintheta_i = sqrt(1 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	float sintheta_t = sintheta_i / n2; // Use Snell's law to get sin(theta_t)

	float costheta_t = sqrt(1 - sintheta_t*sintheta_t); // Get cos(theta_t)

	float a2 = square(cos_theta_i - n2*costheta_t);
	float b2 = square(cos_theta_i + n2*costheta_t);

	float c2 = square(n2*cos_theta_i - costheta_t);
	float d2 = square(costheta_t + n2*cos_theta_i);

	return 0.5 * (a2*d2 + b2*c2) / (b2*d2);
}


float metallicFresnelApprox(float cos_theta, float r_0)
{
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}


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


uint ha(uint x)
{
	x  = (x ^ 12345391u) * 2654435769u;
	x ^= (x << 6) ^ (x >> 26);
	x *= 2654435769u;
	x += (x << 5) ^ (x >> 12);

	return x;
}


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


void main()
{
	vec3 use_normal_cs;
	vec3 use_normal_ws;
	vec2 use_texture_coords = texture_coords;
	if(have_shading_normals != 0)
	{
		use_normal_cs = normal_cs;
		use_normal_ws = normal_ws;
	}
	else
	{
		// Compute camera-space geometric normal.
		vec3 dp_dx = dFdx(pos_cs);
		vec3 dp_dy = dFdy(pos_cs);
		vec3 N_g = cross(dp_dx, dp_dy);
		use_normal_cs = N_g;

#if GENERATE_PLANAR_UVS
		// For voxels: Compute texture coords based on object-space geometric normal.
		dp_dx = dFdx(pos_os);
		dp_dy = dFdy(pos_os);
		vec3 N_g_os = cross(dp_dx, dp_dy);

		if(abs(N_g_os.x) > abs(N_g_os.y) && abs(N_g_os.x) > abs(N_g_os.z))
		{
			use_texture_coords.x = pos_os.y;
			use_texture_coords.y = pos_os.z;
			if(N_g_os.x < 0)
				use_texture_coords.x = -use_texture_coords.x;
		}
		else if(abs(N_g_os.y) > abs(N_g_os.x) && abs(N_g_os.y) > abs(N_g_os.z))
		{
			use_texture_coords.x = pos_os.x;
			use_texture_coords.y = pos_os.z;
			if(N_g_os.y > 0)
				use_texture_coords.x = -use_texture_coords.x;
		}
		else
		{
			use_texture_coords.x = pos_os.x;
			use_texture_coords.y = pos_os.y;
			if(N_g_os.z < 0)
				use_texture_coords.x = -use_texture_coords.x;
		}
#endif

		// Compute world-space geometric normal.
		dp_dx = dFdx(pos_ws);
		dp_dy = dFdy(pos_ws);
		N_g = cross(dp_dx, dp_dy);
		use_normal_ws = N_g;
	}
	vec3 unit_normal_cs = normalize(use_normal_cs);

	float light_cos_theta = max(dot(unit_normal_cs, sundir_cs.xyz), 0.0);

	vec3 frag_to_cam = normalize(pos_cs * -1.0);

	vec3 h = normalize(frag_to_cam + sundir_cs.xyz);

	vec4 diffuse_col;
	if(have_texture != 0)
		diffuse_col = texture(diffuse_tex, (texture_matrix * vec3(use_texture_coords.x, use_texture_coords.y, 1.0)).xy) * diffuse_colour;
	else
		diffuse_col = diffuse_colour;

#if CONVERT_ALBEDO_FROM_SRGB
	// Unfortunately needed for GPU-decoded video frame textures, which are non-linear sRGB but not marked as sRGB.
	// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
	vec4 c = diffuse_col;
	vec4 c2 = c * c;
	diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
#endif

#if VERT_COLOURS
	diffuse_col.xyz *= vert_colour;
#endif

#if ALPHA_TEST
	if(diffuse_col.a < 0.5f)
		discard;
#endif

#if DRAW_PLANAR_UV_GRID
	float du_dx = abs(dFdx(use_texture_coords.x));
	float du_dy = abs(dFdy(use_texture_coords.x));
	
	float dv_dx = abs(dFdx(use_texture_coords.y));
	float dv_dy = abs(dFdy(use_texture_coords.y));
	
	float a = max(du_dx, du_dy);
	float b = max(dv_dx, dv_dy);

	float border_w_u = max(0.01f, a * 0.5f);
	float border_w_v = max(0.01f, b * 0.5f);
	if(	fract(use_texture_coords.x) < border_w_u || fract(use_texture_coords.x) >= (1 - border_w_u) ||
		fract(use_texture_coords.y) < border_w_v || fract(use_texture_coords.y) >= (1 - border_w_v))
		diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
#endif

	//------------- Compute specular microfacet terms --------------
	//float h_cos_theta = max(0.0, dot(h, unit_normal_cs));
	float h_cos_theta = abs(dot(h, unit_normal_cs));
	vec4 specular_fresnel;
	{
		vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * fresnel_scale;
		vec4 metal_fresnel = vec4(
			metallicFresnelApprox(h_cos_theta, diffuse_col.r),
			metallicFresnelApprox(h_cos_theta, diffuse_col.g),
			metallicFresnelApprox(h_cos_theta, diffuse_col.b),
			1);

		// Blend between metal_fresnel and dielectric_fresnel based on metallic_frac.
		specular_fresnel = metal_fresnel * metallic_frac + dielectric_fresnel * (1.0 - metallic_frac);
	}
	vec4 specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
		specular_fresnel;

	float shadow_factor = smoothstep(-0.3, 0, dot(sundir_cs.xyz, unit_normal_cs));
	specular *= shadow_factor;

	// Shadow mapping
	float sun_vis_factor;
#if SHADOW_MAPPING

#define VISUALISE_CASCADES 0

	float samples_scale = 1.f;

	int pixel_index = int((gl_FragCoord.y * 1920.0 + gl_FragCoord.x));
	float pattern_theta = float(float(ha(uint(pixel_index))) * (6.283185307179586 / 4294967296.0));
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta));

	sun_vis_factor = 0.0;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
		{
			vec3 shadow_cds_0 = shadow_tex_coords[0];
			float actual_depth_0 = min(shadow_cds_0.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

			float tex_0_vis = 0;
			for(int i = 0; i < 16; ++i)
			{
				vec2 st = shadow_cds_0.xy + R * samples[i] * samples_scale;
				float light_depth = texture(dynamic_depth_tex, st).x;
				tex_0_vis += (light_depth > actual_depth_0) ? (1.0f / 16) : 0.0;
			}

			float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
			if(dist > edge_dist)
			{
				int next_tex_index = 1;
				vec3 shadow_cds_1 = shadow_tex_coords[next_tex_index];
				float actual_depth_1 = min(shadow_cds_1.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

				float tex_1_vis = 0;
				for(int i = 0; i < 16; ++i)
				{
					vec2 st = shadow_cds_1.xy + R * samples[i] * samples_scale;
					float light_depth = texture(dynamic_depth_tex, st).x;
					tex_1_vis += (light_depth > actual_depth_1) ? (1.0f / 16) : 0.0;
				}

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
			int dynamic_depth_tex_index = 1;
			vec3 shadow_cds = shadow_tex_coords[dynamic_depth_tex_index];
			float actual_depth = min(shadow_cds.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

			for(int i = 0; i < 16; ++i)
			{
				vec2 st = shadow_cds.xy + R * samples[i] * samples_scale;
				float light_depth = texture(dynamic_depth_tex, st).x;
				sun_vis_factor += (light_depth > actual_depth) ? (1.0f / 16) : 0.0;
			}

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float bias = 0.0005;// / abs(light_cos_theta);
				float static_actual_depth = min(static_shadow_cds.z, 0.999f) - bias; // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

				float static_sun_vis_factor = 0;
				for(int i = 0; i < 16; ++i)
				{
					vec2 st = static_shadow_cds.xy + R * samples[i] * samples_scale;
					float light_depth = texture(static_depth_tex, st).x;
					static_sun_vis_factor += (light_depth > static_actual_depth) ? (1.0 / 16.0) : 0.0;
				}

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
	
		if(l1dist < 1024)
		{
			int static_depth_tex_index;
			float cascade_end_dist;
			if(l1dist < 64)
			{
				static_depth_tex_index = 0;
				cascade_end_dist = 64;
			}
			else if(l1dist < 256)
			{
				static_depth_tex_index = 1;
				cascade_end_dist = 256;
			}
			else
			{
				static_depth_tex_index = 2;
				cascade_end_dist = 1024;
			}

			vec3 shadow_cds = shadow_tex_coords[static_depth_tex_index + NUM_DYNAMIC_DEPTH_TEXTURES];

			float bias = 0.0005;// / abs(light_cos_theta);
			float actual_depth = min(shadow_cds.z, 0.999f) - bias; // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

			for(int i=0; i<16; ++i)
			{
				vec2 st = shadow_cds.xy + R * samples[i] * samples_scale;
				float light_depth = texture(static_depth_tex, st).x;
				sun_vis_factor += (light_depth > actual_depth) ? (1.0f / 16) : 0.0;
			}

			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					int next_tex_index = static_depth_tex_index + 1;
					vec3 next_shadow_cds = shadow_tex_coords[next_tex_index + NUM_DYNAMIC_DEPTH_TEXTURES];

					float bias = 0.0005;// / abs(light_cos_theta);
					float next_actual_depth = min(next_shadow_cds.z, 0.999f) - bias; // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

					float next_sun_vis_factor = 0;
					for(int i = 0; i < 16; ++i)
					{
						vec2 st = next_shadow_cds.xy + R * samples[i] * samples_scale;
						float light_depth = texture(static_depth_tex, st).x;
						next_sun_vis_factor += (light_depth > next_actual_depth) ? (1.0f / 16) : 0.0;
					}

					float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
					sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
				}
			}

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
	// Compute position on cumulus cloud layer
	if(pos_ws.z < 1000.f)
	{
		vec3 sundir_ws = vec3(3.6716393E-01, 6.3513672E-01, 6.7955279E-01); // TEMP HACK
		vec3 cum_layer_pos = pos_ws + sundir_ws * (1000.f - pos_ws.z) / sundir_ws.z;

		vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
		cum_tex_coords.x += time * 0.002;

		vec2 cumulus_coords = vec2(cum_tex_coords.x * 2 + 2.3453, cum_tex_coords.y * 2 + 1.4354);
		float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}



	vec3 unit_normal_ws = normalize(use_normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0)
		unit_normal_ws = -unit_normal_ws;

	vec4 sky_irradiance;
#if LIGHTMAPPING
	// glTexImage2D expects the start of the texture data to be the lower left of the image, whereas it is actually the upper left.  So flip y coord to compensate.
	sky_irradiance = texture(lightmap_tex, vec2(lightmap_coords.x, -lightmap_coords.y)) * 1.0e9f;
#else
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz) * 1.0e9f; // integral over hemisphere of cosine * incoming radiance from sky.
#endif


	vec3 unit_cam_to_pos_ws = normalize(cam_to_pos_ws);
	// Reflect cam-to-fragment vector in ws normal
	vec3 reflected_dir_ws = unit_cam_to_pos_ws - unit_normal_ws * (2.0 * dot(unit_normal_ws, unit_cam_to_pos_ws));

	//========================= Look up env map for reflected dir ============================
	
	int map_lower = int(roughness * 6.9999);
	int map_higher = map_lower + 1;
	float map_t = roughness * 6.9999 - float(map_lower);

	float refl_theta = acos(reflected_dir_ws.z);
	float refl_phi = atan(reflected_dir_ws.y, reflected_dir_ws.x) - 1.f; // -1.f is to rotate reflection so it aligns with env rotation.
	vec2 refl_map_coords = vec2(refl_phi * (1.0 / 6.283185307179586), clamp(refl_theta * (1.0 / 3.141592653589793), 1.0 / 64, 1 - 1.0 / 64)); // Clamp to avoid texture coord wrapping artifacts.

	vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, map_lower  * (1.0/8) + refl_map_coords.y * (1.0/8))) * 1.0e9f; //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
	vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, map_higher * (1.0/8) + refl_map_coords.y * (1.0/8))) * 1.0e9f;
	vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t;


	float fresnel_cos_theta = max(0.0, dot(reflected_dir_ws, unit_normal_ws));
	vec4 dielectric_refl_fresnel = vec4(fresnelApprox(fresnel_cos_theta, 1.5) * fresnel_scale);
	vec4 metallic_refl_fresnel = vec4(
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.r),
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.g),
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.b),
		1)/* * fresnel_scale*/;

	vec4 refl_fresnel = metallic_refl_fresnel * metallic_frac + dielectric_refl_fresnel * (1.0f - metallic_frac);

	vec4 sun_light = vec4(18333573286.57627,16541737714.860512,14495551899.203238, 1) * sun_vis_factor;

	vec4 col =
		sky_irradiance * diffuse_col * (1.0 / 3.141592653589793) * (1.0 - refl_fresnel) * (1.0 - metallic_frac) +  // Diffuse substrate part of BRDF * incoming radiance from sky
		refl_fresnel * spec_refl_light + // Specular reflection of sky
		sun_light * (1.0 - refl_fresnel) * (1.0 - metallic_frac) * diffuse_col * (1.0 / 3.141592653589793) * light_cos_theta + //  Diffuse substrate part of BRDF * sun light
		sun_light * specular; // sun light * specular microfacet terms
	//vec4 col = (sun_light + 3000000000.0)  * diffuse_col;

#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = -pos_cs.z;
	float fog_factor = 1.0f - exp(dist_ * -0.00015);
	vec4 sky_col = vec4(1.8, 4.7, 8.0, 1) * 2.0e8; // Bluish grey
	col = mix(col, sky_col, fog_factor);
#endif
		
	col *= 0.0000000003; // tone-map
	
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
