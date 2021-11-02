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
#if DOUBLE_SIDED
uniform sampler2D backface_diffuse_tex;
uniform sampler2D transmission_tex;
#endif
uniform sampler2DShadow dynamic_depth_tex;
uniform sampler2DShadow static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;
uniform sampler2D blue_noise_tex;
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
	float begin_fade_out_distance;
	float end_fade_out_distance;
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


float sampleDynamicDepthMap(mat2 R, vec3 shadow_coords)
{
	// float actual_depth_0 = min(shadow_cds_0.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

	// This technique blurs the shadows a bit, but works well to reduce swimming aliasing artifacts on shadow edges etc..
	/*
	float offset_scale = 1.f / 2048; // 2048 = tex res
	float sum = 0;
	for(int x=-1; x<=2; ++x)
		for(int y=-1; y<=2; ++y)
		{
			vec2 st = shadow_coords.xy + R * (vec2(x - 0.5f, y - 0.5f) * offset_scale);
			sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z));
		}
	return sum * (1.f / 16);*/

	// This technique is a bit sharper:
	float sum = 0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z));
	}
	return sum * (1.f / 16);
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords)
{
	// This technique gives sharper shadows, so will use for static depth maps to avoid shadows on smaller objects being blurred away.
	float sum = 0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(static_depth_tex, vec3(st.x, st.y, shadow_coords.z));
	}
	return sum * (1.f / 16);
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

	float light_cos_theta = dot(unit_normal_cs, sundir_cs.xyz);

#if DOUBLE_SIDED
	float sun_light_cos_theta_factor = abs(light_cos_theta);
#else
	float sun_light_cos_theta_factor = max(0.f, light_cos_theta);
#endif

	vec3 frag_to_cam = normalize(pos_cs * -1.0);

	vec3 h = normalize(frag_to_cam + sundir_cs.xyz);

	// We will get two diffuse colours - one for the sun contribution, and one for reflected light from the same hemisphere the camera is in.
	// The sun contribution diffuse colour may use the transmission texture.  The reflected light colour may be the front or backface albedo texture.

	vec4 sun_texture_diffuse_col;
	vec4 refl_texture_diffuse_col;
	if(have_texture != 0)
	{
#if DOUBLE_SIDED
		// Work out if we are seeing the front or back face of the material
		float frag_to_cam_dot_normal = dot(frag_to_cam, unit_normal_cs);
		if(frag_to_cam_dot_normal < 0.f)
		{
			refl_texture_diffuse_col = texture(backface_diffuse_tex, (texture_matrix * vec3(use_texture_coords.x, use_texture_coords.y, 1.0)).xy); // backface
			//refl_texture_diffuse_col.xyz = vec3(0,0,1);//TEMP
		}
		else
			refl_texture_diffuse_col = texture(diffuse_tex,          (texture_matrix * vec3(use_texture_coords.x, use_texture_coords.y, 1.0)).xy); // frontface


		if(frag_to_cam_dot_normal * light_cos_theta <= 0.f) // If frag_to_cam and sundir_cs in different geometric hemispheres:
		{
			sun_texture_diffuse_col = texture(transmission_tex, (texture_matrix * vec3(use_texture_coords.x, use_texture_coords.y, 1.0)).xy);
			//sun_texture_diffuse_col.xyz = vec3(1,0,0);//TEMP
		}
		else
			sun_texture_diffuse_col = refl_texture_diffuse_col; // Else sun is illuminating the face facing the camera.
#else
		sun_texture_diffuse_col = texture(diffuse_tex, (texture_matrix * vec3(use_texture_coords.x, use_texture_coords.y, 1.0)).xy);
		refl_texture_diffuse_col = sun_texture_diffuse_col;
#endif

#if CONVERT_ALBEDO_FROM_SRGB
		// Texture value is in non-linear sRGB, convert to linear sRGB.
		// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
		vec4 c = sun_texture_diffuse_col;
		vec4 c2 = c * c;
		sun_texture_diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;

		c = refl_texture_diffuse_col;
		c2 = c * c;
		refl_texture_diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
#endif
	}
	else
	{
		sun_texture_diffuse_col = vec4(1.f);
		refl_texture_diffuse_col = vec4(1.f);
	}

	// Final diffuse colour = texture diffuse colour * constant diffuse colour
	vec4 sun_diffuse_col  = sun_texture_diffuse_col  * diffuse_colour; // diffuse_colour is linear sRGB already.
	vec4 refl_diffuse_col = refl_texture_diffuse_col * diffuse_colour;

#if VERT_COLOURS
	sun_diffuse_col.xyz *= vert_colour;
	refl_diffuse_col.xyz *= vert_colour;
#endif

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1 / 128.f)).x;
#if IMPOSTERABLE
	float dist_alpha_factor = smoothstep(begin_fade_out_distance, end_fade_out_distance,  /*dist=*/-pos_cs.z);
	if(dist_alpha_factor > pixel_hash)
		discard;
#endif

#if ALPHA_TEST
	if(refl_diffuse_col.a < 0.5f)
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
		refl_diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
#endif

	//------------- Compute specular microfacet terms --------------
	//float h_cos_theta = max(0.0, dot(h, unit_normal_cs));
	float h_cos_theta = abs(dot(h, unit_normal_cs));
	vec4 specular_fresnel;
	{
		vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * fresnel_scale;
		vec4 metal_fresnel = vec4(
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.r),
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.g),
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.b),
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

	float pattern_theta = pixel_hash * 6.283185307179586f;
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
			refl_diffuse_col.yz *= 0.5f;
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
			refl_diffuse_col.yz *= 0.75f;
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
			sun_vis_factor = sampleStaticDepthMap(R, shadow_cds); // NOTE: had cap and bias

			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					int next_tex_index = static_depth_tex_index + 1;
					vec3 next_shadow_cds = shadow_tex_coords[next_tex_index + NUM_DYNAMIC_DEPTH_TEXTURES];

					float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds); // NOTE: had cap and bias

					float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
					sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
				}
			}

#if VISUALISE_CASCADES
			refl_diffuse_col.xz *= float(static_depth_tex_index) / NUM_STATIC_DEPTH_TEXTURES;
#endif
		}
		else
			sun_vis_factor = 1.0;
	}

#else // else if !SHADOW_MAPPING:
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
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.r),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.g),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.b),
		1)/* * fresnel_scale*/;

	vec4 refl_fresnel = metallic_refl_fresnel * metallic_frac + dielectric_refl_fresnel * (1.0f - metallic_frac);

	vec4 sun_light = vec4(1662102582.6479533,1499657101.1924045,1314152016.0871031, 1) * sun_vis_factor; // Sun spectral radiance multiplied by solid angle, see SkyModel2Generator::makeSkyEnvMap().

	vec4 col =
		sky_irradiance * sun_diffuse_col * (1.0 / 3.141592653589793) * (1.0 - refl_fresnel) * (1.0 - metallic_frac) +  // Diffuse substrate part of BRDF * incoming radiance from sky
		refl_fresnel * spec_refl_light + // Specular reflection of sky
		sun_light * (1.0 - refl_fresnel) * (1.0 - metallic_frac) * refl_diffuse_col * (1.0 / 3.141592653589793) * sun_light_cos_theta_factor + //  Diffuse substrate part of BRDF * sun light
		sun_light * specular; // sun light * specular microfacet terms
	//vec4 col = (sun_light + 3000000000.0)  * diffuse_col;

#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = -pos_cs.z;
	float fog_factor = 1.0f - exp(dist_ * -0.00015);
	vec4 sky_col = vec4(1.8, 4.7, 8.0, 1) * 2.0e7; // Bluish grey
	col = mix(col, sky_col, fog_factor);
#endif
		
	col *= 0.000000003; // tone-map
	
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
