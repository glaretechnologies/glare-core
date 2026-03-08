

// Should match FogGPUSettings struct in OpenGLEngine.h
layout (std140) uniform FogSettings
{
	float layer_0_A;
	float layer_0_B;
	float layer_1_A;
	float layer_1_B;

} fog_settings;


uniform sampler2D albedo_texture; // main colour buffer

uniform sampler2D depth_tex;

uniform sampler2DShadow dynamic_depth_tex;
uniform sampler2DShadow static_depth_tex;

uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


vec3 camSpaceFromScreenSpaceDir(vec2 normed_pos_ss)
{
	return vec3(
		(normed_pos_ss.x - 0.5) / l_over_w,
		(normed_pos_ss.y - 0.5) / l_over_h,
		-1
	);
}


float shadowMapSunFactorAtPoint(vec3 pos_cs, vec3 pos_ws)
{
#if NUM_DEPTH_TEXTURES > 0
	vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
	for(int i = 0; i < NUM_DEPTH_TEXTURES; ++i)
		shadow_tex_coords[i] = (frag_shadow_texture_matrix[i] * vec4(pos_ws, 1.0)).xyz;
	
	return getShadowMappingSunVisFactorFast(shadow_tex_coords, dynamic_depth_tex, static_depth_tex, pos_cs, shadow_map_samples_xy_scale);
#else
	return 1.f;
#endif
}


void main()
{
	ivec2 tex_res  = textureSize(albedo_texture, /*mip level=*/0);
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	vec4 scene_colour = texelFetch(albedo_texture, px_coords, /*mip level=*/0);

	// Reverse-Z stores the far plane as 0, which causes a divide-by-zero in
	// getDepthFromDepthTextureValue.  Clamp away from 0 so sky pixels get
	// treated as very distant (large depth => full fog).
	float depth_val = texture(depth_tex, pos).x;
	float linear_depth = getDepthFromDepthTextureValue(near_clip_dist, depth_val); // defined in frag_utils.glsl

	vec3 dir_cs = normalize(camSpaceFromScreenSpaceDir(pos));

	float linear_dist = min(max(0.01, linear_depth / -dir_cs.z), 40000.0);

	vec3 dir_ws = normalize((vec4(dir_cs, 0.0) * frag_view_matrix).xyz);// Note that right multiplying by a matrix is the same as left multiplying by its transpose in GLSL.

	float o_z = mat_common_campos_ws.z; // z-coord (height) of ray origin
	float d_z = dir_ws.z;

	float A_0 = fog_settings.layer_0_A; // Overall density factor
	float B_0 = fog_settings.layer_0_B; // Speed of falloff with height
	float A_1 = fog_settings.layer_1_A; // Overall density factor
	float B_1 = fog_settings.layer_1_B; // Speed of falloff with height

	float sun_dir_cos_theta = dot(sundir_ws.xyz, dir_ws);
	float g = 0.7;
	float hg_phase_func_val = (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0*g*sun_dir_cos_theta, 1.5));

	// Use a blend between uniform (to capture multiple scattering) and H.G. phase function.
	float sun_phase_function_val = mix(1.0 / (4.0 * PI), hg_phase_func_val, 0.5f);

#if 1 // Raymarch

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.f)).x;

	float delta_d = 0.1;

	float last_delta_d = delta_d;
	float cur_end_d = delta_d;
	float cur_d = 0.0;
	float T = 1.0;
	vec3 R = vec3(0.0); // in-scattered radiance
	
	for(int i=0; i<160; ++i)
	{
		cur_d = cur_end_d - last_delta_d * pixel_hash;
		if(cur_d >= linear_dist)
			break;

		vec3 cur_pos_ws = mat_common_campos_ws.xyz + dir_ws * cur_d;

		// Eval incoming spectral radiance at point
		vec3 cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;

		float sun_vis_factor = shadowMapSunFactorAtPoint(cur_pos_cs, cur_pos_ws); // TODO: sun T factor as well

		vec3 incoming_sun_spec_rad = sun_spec_rad_times_solid_angle.xyz * sun_vis_factor;
		vec3 sun_sky_weighted_incoming_rad = (incoming_sun_spec_rad * sun_phase_function_val + sky_av_spec_rad.xyz);

		//float fbm_env = max(0.0, texture(fbm_tex, cur_pos_ws.xy * 0.001).x);
		float sigma_t = (A_0 * exp(-B_0 * cur_pos_ws.z) + A_1 * exp(-B_1 * cur_pos_ws.z));// * fbm_env; // Height fog
		float sigma_s = sigma_t * 0.8;

		R += sun_sky_weighted_incoming_rad * sigma_s * T * delta_d;

		cur_end_d += delta_d;

		T *= exp(-sigma_t * delta_d);

		last_delta_d = delta_d;
		delta_d *= 1.04;
	}

	float start_T = T;
	// Add remaining distance without raymarching
	if(cur_d < linear_dist)
	{
		float start_dist = cur_d;
		float end_dist = linear_dist;

		// See https://forwardscattering.org/post/72
		float total_optical_depth;
		if(abs(d_z) < 1.0e-6)
		{
			total_optical_depth = A_0 * exp(-B_0 * (start_dist*d_z + o_z)) * (linear_dist - start_dist) +
			                      A_1 * exp(-B_1 * (start_dist*d_z + o_z)) * (linear_dist - start_dist);
		}
		else
		{
			total_optical_depth = (A_0 / (B_0 * d_z)) * (exp(-B_0 * (start_dist*d_z + o_z)) - exp(-B_0 * (end_dist*d_z + o_z))) +
			                      (A_1 / (B_1 * d_z)) * (exp(-B_1 * (start_dist*d_z + o_z)) - exp(-B_1 * (end_dist*d_z + o_z)));
		}

		float segment_T = exp(-total_optical_depth);
		T *= segment_T;

		vec3 incoming_sun_spec_rad = sun_spec_rad_times_solid_angle.xyz; // NOTE: assuming sun vis factor = 1 here
		vec3 sun_sky_weighted_incoming_rad = (incoming_sun_spec_rad * sun_phase_function_val + sky_av_spec_rad.xyz);

		R += start_T * (1.0 - segment_T) * sun_sky_weighted_incoming_rad;
	}

	colour_out = vec4(scene_colour.rgb * T + R, 1.0);

#else // Else if not raymarching:

	// See https://forwardscattering.org/post/72
	float total_optical_depth;
	if(abs(d_z) < 1.0e-6)
	{
		total_optical_depth = A_0 * exp(-B_0 * o_z) * linear_dist +
			                  A_1 * exp(-B_1 * o_z) * linear_dist;
	}
	else
	{
		total_optical_depth = (A_0 / (B_0 * d_z)) * (exp(-B_0 * o_z) - exp(-B_0 * (linear_dist*d_z + o_z))) +
			                  (A_1 / (B_1 * d_z)) * (exp(-B_1 * o_z) - exp(-B_1 * (linear_dist*d_z + o_z)));
	}

	float T = exp(-total_optical_depth);
	
	vec3 sun_sky_weighted_incoming_rad = (sun_spec_rad_times_solid_angle.xyz * sun_phase_function_val + sky_av_spec_rad.xyz);

	vec3 use_fog_colour = 0.8f * sun_sky_weighted_incoming_rad;
	colour_out = vec4(mix(use_fog_colour, scene_colour.rgb, T), scene_colour.a);

#endif
}
