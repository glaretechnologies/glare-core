
in vec3 pos_cs;
in vec2 texture_coords;
in vec3 dir_ws;

uniform vec4 diffuse_colour;
uniform int have_texture;
uniform sampler2D diffuse_tex;
//uniform sampler2D noise_tex;
uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;
uniform sampler2D cirrus_tex;
uniform mat3 texture_matrix;
uniform vec3 env_campos_ws;
uniform sampler2D aurora_tex;


out vec4 colour_out;




void main()
{
	// Col = spectral radiance * 1.0e-9
	vec4 col;
	if(have_texture != 0)
		// Multiply x coord by 2 since we just have one half of the sphere encoded in the env map.
		col = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x * 2.0, texture_coords.y, 1.0)).xy);
	else
		col = diffuse_colour * 1.0e-9f;

#if RENDER_SUN_AND_SKY
	// Render sun
	
	float sunscale = mix(2.0e-2f, 2.0e-3f, sundir_ws.z); // A hack to avoid having too extreme bloom from the sun, also to compensate for larger sun size due to the smoothstep below.
	const float sun_solid_angle = 0.00006780608; // See SkyModel2Generator::makeSkyEnvMap();
	vec4 suncol = sun_spec_rad_times_solid_angle * (1.0 / sun_solid_angle) * sunscale;
	float d = dot(sundir_cs.xyz, normalize(pos_cs));
	//col = mix(col, suncol, smoothstep(0.9999, 0.9999892083461507, d));
	col = mix(col, suncol, smoothstep(0.99997, 0.9999892083461507, d));



#if DRAW_AURORA
	const float MAX_AURORA_SUNDIR_Z = 0.1;
	if(sundir_ws.z < MAX_AURORA_SUNDIR_Z)
	{
		float aurora_factor = 1.0 - smoothstep(0.0, MAX_AURORA_SUNDIR_Z, sundir_ws.z);

		// NOTE: code duplicated in water_frag_shader
		float min_aurora_z = 1000.0;
		float max_aurora_z = 8000.0;
		float aurora_start_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, min_aurora_z);
		float aurora_end_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, max_aurora_z);

		int num_steps = 32;
		float t_step = min(600.0, (aurora_end_ray_t - aurora_start_ray_t) / float(num_steps));
		float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 64.f)).x;
		float t_offset = pixel_hash * t_step;

		vec3 aurora_up = normalize(vec3(0.3, 0.0, 1.0));
		vec3 aurora_forw = normalize(cross(aurora_up, vec3(0,0,1))); // vector along aurora surface
		vec3 aurora_right = cross(aurora_up, aurora_forw);

	
		vec4 green_col = vec4(0, pow(0.79, 2.2), pow(0.47, 2.2), 0);
		vec4 blue_col  = vec4(0, pow(0.1, 2.2),  pow(0.6, 2.2), 0);

		for(int i=0; i<num_steps; ++i)
		{
			float ray_t = aurora_start_ray_t + t_offset + t_step * float(i);
			vec3 p = env_campos_ws + dir_ws * ray_t;

			vec3 p_as = vec3(500.0 + dot(p, aurora_right), dot(p, aurora_forw), dot(p, aurora_up));

			vec2 st = p_as.xy * 0.0001;
			if(st.x > -1.0 && st.x <= 1.0 && st.y >= -1.0 && st.y <= 1.0)
			{
				vec4 aurora_val = texture(aurora_tex, st);

				float aurora_start_z = 1000.0 + aurora_val.y * 1000.0;
				if(p_as.z >= aurora_start_z)
				{
					// Smoothly start aurora above aurora_start_z
					float z_factor = smoothstep(aurora_start_z, aurora_start_z + 600.0, p_as.z);
				
					// Smoothly decrease intensity as z increases
					float z_ramp_intensity_factor = exp(-(p_as.z - 1200.0) * 0.001);
					float high_freq_intensity_factor = 1.0 + 3.0 * z_ramp_intensity_factor * (aurora_val.y - 0.5);//(1.0 + aurora_val.y * 2.0 * ramp_intensity_factor*ramp_intensity_factor);
					//float ramp_intensity_factor = max(0.0, 1000 / (p_as.z - 1100) - p_as.z * 0.001);
				
					vec4 col_for_height = mix(green_col, blue_col, min(1.0, (p_as.z - aurora_start_z) * (1.0 / 2000.0)));
				
					col += 0.001 * t_step * col_for_height * aurora_val.r * z_ramp_intensity_factor * high_freq_intensity_factor * z_factor * aurora_factor;
				}
			}
		}
	}
#endif


	vec2 cloudfrac_cumulus_edge = getCloudFrac(env_campos_ws, dir_ws, time, fbm_tex, cirrus_tex);
	float cloudfrac    = cloudfrac_cumulus_edge.x;
	float cumulus_edge = cloudfrac_cumulus_edge.y;
	vec4 cloudcol = sun_and_sky_av_spec_rad;
	col = mix(col, cloudcol, max(0.f, cloudfrac));
	vec4 suncloudcol = cloudcol * 2.5;
	float blend = max(0.f, cumulus_edge) * pow(max(0.0, d), 32.0);// smoothstep(0.9, 0.9999892083461507, d);
	col = mix(col, suncloudcol, blend);

	//col = mix(col, cumulus_col, cumulus_alpha);


#if DEPTH_FOG
	// Blend lower hemisphere into a colour that matches fogged ground quad in Substrata
	vec4 lower_hemis_col = sun_and_sky_av_spec_rad * 0.9;
	
	float lower_hemis_factor = smoothstep(1.52, 1.6, texture_coords.y);
	col = mix(col, lower_hemis_col, lower_hemis_factor);
#endif

#endif // RENDER_SUN_AND_SKY

#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, 1);
#else
	colour_out = vec4(toneMapToNonLinear(col.xyz), 1.0);
#endif
}
