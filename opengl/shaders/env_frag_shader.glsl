
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


float rayPlaneIntersect(vec3 raystart, vec3 ray_unitdir, float plane_h)
{
	float start_to_plane_dist = raystart.z - plane_h;

	return start_to_plane_dist / -ray_unitdir.z;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2.0;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

//float noise(vec2 p)
//{
//	return (texture(noise_tex, p).x - 0.5) * 2.f;
//}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2.0)) * 0.5;
}

float length2(vec2 v)
{
	return dot(v, v);
}


void main()
{
	// Col = spectral radiance * 1.0e-9
	vec4 col;
	if(have_texture != 0)
		col = texture(diffuse_tex, (texture_matrix * vec3(texture_coords.x, texture_coords.y, 1.0)).xy);
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
	// NOTE: code duplicated in water_frag_shader
	float min_aurora_z = 1000.0;
	float max_aurora_z = 8000.0;
	float aurora_start_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, min_aurora_z);
	float aurora_end_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, max_aurora_z);

	int num_steps = 32;
	float t_step = min(600.0, (aurora_end_ray_t - aurora_start_ray_t) / float(num_steps));
	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 128.f)).x;
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
				
				col += 0.001 * t_step * col_for_height * aurora_val.r * z_ramp_intensity_factor * high_freq_intensity_factor * z_factor;
			}
		}
	}
#endif


	// Get position ray hits cloud plane
	float cirrus_cloudfrac = 0.0;
	float cumulus_cloudfrac = 0.0;
	float ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, 6000.0);
	//vec4 cumulus_col = vec4(0,0,0,0);
	//float cumulus_alpha = 0;
	float cumulus_edge = 0.0;
	if(ray_t > 0.0)
	{
		vec3 hitpos = env_campos_ws + dir_ws * ray_t;
		vec2 p = hitpos.xy * 0.0001;
		p.x += time * 0.002;
	
		vec2 coarse_noise_coords = vec2(p.x * 0.16, p.y * 0.20);
		float course_detail = fbmMix(vec2(coarse_noise_coords));

		cirrus_cloudfrac = max(course_detail * 0.9, 0.f) * texture(cirrus_tex, p).x * 1.5;
	}
		
	{
		float cumulus_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, 1000.0);
		if(cumulus_ray_t > 0.0)
		{
			vec3 hitpos = env_campos_ws + dir_ws * cumulus_ray_t;
			vec2 p = hitpos.xy * 0.0001;
			p.x += time * 0.002;

			vec2 cumulus_coords = vec2(p.x * 2.0 + 2.3453, p.y * 2.0 + 1.4354);
			
			float cumulus_val = max(0.f, min(1.0, fbmMix(cumulus_coords) * 1.6 - 0.3f));
			//cumulus_alpha = max(0.f, cumulus_val - 0.7f);

			cumulus_edge = smoothstep(0.0001, 0.1, cumulus_val) - smoothstep(0.2, 0.6, cumulus_val) * 0.5;

			float dist_factor = 1.f - smoothstep(20000.0, 40000.0, cumulus_ray_t);

			//cumulus_col = vec4(cumulus_val, cumulus_val, cumulus_val, 1);
			cumulus_cloudfrac = dist_factor * cumulus_val;
		}
	}

	float cloudfrac = max(cirrus_cloudfrac, cumulus_cloudfrac);
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
	colour_out = vec4(toNonLinear(col.xyz), 1);
#endif
}
