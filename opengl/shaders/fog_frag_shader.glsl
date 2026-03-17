

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


// Transmittance between distances start_d and end_d along a ray.
// See https://forwardscattering.org/post/72
float segmentT(float o_z, float d_z, float start_d, float end_d, float A_0, float B_0, float A_1, float B_1)
{
	float optical_depth;
	if(abs(d_z) < 1.0e-6)
	{
		optical_depth = A_0 * exp(-B_0 * (start_d*d_z + o_z)) * (end_d - start_d) +
		                A_1 * exp(-B_1 * (start_d*d_z + o_z)) * (end_d - start_d);
	}
	else
	{
		optical_depth = (A_0 / (B_0 * d_z)) * (exp(-B_0 * (start_d*d_z + o_z)) - exp(-B_0 * (end_d*d_z + o_z))) +
		                (A_1 / (B_1 * d_z)) * (exp(-B_1 * (start_d*d_z + o_z)) - exp(-B_1 * (end_d*d_z + o_z)));
	}

	return exp(-optical_depth);
}


void main()
{
	ivec2 tex_res  = textureSize(albedo_texture, /*mip level=*/0);
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	vec4 scene_colour = texelFetch(albedo_texture, px_coords, /*mip level=*/0);

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

	const float albedo = 0.8; // sigma_s / sigma_T
#if 1 // Raymarch

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.f)).x;

	/*
	Suppose MAX_NUM_ITERS = 4:
	                                       last_sample_d
	x                                          |
	|\                                         v
	|  \__         _---x- _            _-------x   last_segment_end_d
	|     \    _--/    |   \------x---/        |   |
	|      \x-/        |          |            |   v
	|x------x--|-------x--|-------x---|--------x---|------------------|-------------
	                                                                  |
	|<-seg 0-->|<--seg 1->|<--seg 2-->|<--seg 3--->|              linear_dist (dist to object surface)
	
	                                               |<---------------->|
	                                           Analytically integrated region
	*/
	
	float cur_d = 0.0;
	float T = 1.0;
	vec3 R = vec3(0.0); // in-scattered radiance
	float segment_start_d  = 0.0;
	float segment_delta_d  = 0.1;
	float last_sample_d = 0.0; // Last distance sample in [0, linear_dist)
	float last_sigma_t = (A_0 * exp(-B_0 * o_z) + A_1 * exp(-B_1 * o_z)); // sigma_t at last_sample_d.
	const int MAX_NUM_ITERS = 100;
	bool done = false;
	for(int i=0; i<MAX_NUM_ITERS && !done; ++i)
	{
		if(segment_start_d + segment_delta_d > linear_dist)
		{
			segment_delta_d = linear_dist - segment_start_d;
			done = true; // break loop after this iteration
		}
		cur_d = segment_start_d + segment_delta_d * pixel_hash;

		vec3 cur_pos_ws = mat_common_campos_ws.xyz + dir_ws * cur_d;

		// Eval incoming spectral radiance at point
		vec3 cur_pos_cs = (frag_view_matrix * vec4(cur_pos_ws, 1.0)).xyz;

		float sun_vis_factor = shadowMapSunFactorAtPoint(cur_pos_cs, cur_pos_ws); // TODO: sun T factor as well

		vec3 incoming_sun_spec_rad = sun_spec_rad_times_solid_angle.xyz * sun_vis_factor;
		vec3 sun_sky_weighted_incoming_rad = (incoming_sun_spec_rad * sun_phase_function_val + sky_av_spec_rad.xyz);

		//float fbm_env = max(0.0, texture(fbm_tex, cur_pos_ws.xy * 0.001).x);
		float sigma_t = (A_0 * exp(-B_0 * cur_pos_ws.z) + A_1 * exp(-B_1 * cur_pos_ws.z));// * fbm_env; // Height fog
		float sigma_s = sigma_t * albedo;

		// Compute transmittance from last_sample_d to cur_d.
		if(abs(sigma_t - last_sigma_t) > 0.5 * sigma_t) // If sigma_t differs a lot from last_sigma_t:
		{
			// Use the completely accurate but slower transmittance computation for exponential height fog:
			T *= segmentT(o_z, d_z, /*start_d=*/last_sample_d, /*end_d=*/cur_d, A_0, B_0, A_1, B_1);
		}
		else
		{
			// Estimate transmittance using piecewise linear sigma_t.
			float av_sigma_t = 0.5f * (last_sigma_t + sigma_t);
			float segment_tau = av_sigma_t * (cur_d - last_sample_d); // Optical depth estimate from last sample to this sample.
			T *= exp(-segment_tau);
		}
		
		R += sun_sky_weighted_incoming_rad * sigma_s * T * segment_delta_d;

		segment_start_d += segment_delta_d;
		segment_delta_d *= 1.06;
		last_sample_d = cur_d;
		last_sigma_t = sigma_t;

		if((d_z > 0.0) && (sigma_t < 1.0e-3))
			break;
	}

	float last_segment_end_d = segment_start_d;


	// Apply transmittance from last_sample_d to last_segment_end_d
	T *= segmentT(o_z, d_z, /*start_d=*/last_sample_d, /*end_d=*/last_segment_end_d, A_0, B_0, A_1, B_1);

	float start_T = T;

	// Add remaining distance without raymarching
	if(last_segment_end_d < (linear_dist - 0.1))
	{
		float segment_T = segmentT(o_z, d_z, /*start_d=*/last_segment_end_d, /*end_d=*/linear_dist, A_0, B_0, A_1, B_1);
		T *= segment_T;

		vec3 incoming_sun_spec_rad = sun_spec_rad_times_solid_angle.xyz; // NOTE: assuming sun vis factor = 1 here
		vec3 sun_sky_weighted_incoming_rad = (incoming_sun_spec_rad * sun_phase_function_val + sky_av_spec_rad.xyz);

		R += start_T * (1.0 - segment_T) * albedo * sun_sky_weighted_incoming_rad;
	}

	colour_out = vec4(scene_colour.rgb * T + R, 1.0);

#else // Else if not raymarching:

	float T = segmentT(o_z, d_z, /*start_d=*/0.0, /*end_d=*/linear_dist, A_0, B_0, A_1, B_1);
	
	vec3 sun_sky_weighted_incoming_rad = (sun_spec_rad_times_solid_angle.xyz * sun_phase_function_val + sky_av_spec_rad.xyz);

	vec3 use_fog_colour = albedo * sun_sky_weighted_incoming_rad;
	colour_out = vec4(mix(use_fog_colour, scene_colour.rgb, T), scene_colour.a);

#endif
}
