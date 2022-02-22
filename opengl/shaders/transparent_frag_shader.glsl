
in vec3 normal_cs;
in vec3 normal_ws;
in vec3 pos_cs;
#if GENERATE_PLANAR_UVS
in vec3 pos_os;
#endif
in vec3 cam_to_pos_ws;
#if USE_LOGARITHMIC_DEPTH_BUFFER
in float flogz;
#endif

uniform vec4 sundir_cs;
uniform vec4 diffuse_colour;
uniform int have_shading_normals;
uniform sampler2D specular_env_tex;

out vec4 colour_out;



float square(float x) { return x*x; }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }

float fresnellApprox(float cos_theta, float ior)
{
	float r_0 = square((1.0 - ior) / (1.0 + ior));
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}

float alpha2ForRoughness(float r)
{
	return pow6(r);
}


vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}


void main()
{
	vec3 use_normal_cs;
	vec2 use_texture_coords = vec2(0, 0);
	if(have_shading_normals != 0)
	{
		use_normal_cs = normal_cs;
	}
	else
	{
		vec3 dp_dx = dFdx(pos_cs);    
		vec3 dp_dy = dFdy(pos_cs);  
		vec3 N_g = normalize(cross(dp_dx, dp_dy)); 
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
	}
	vec3 unit_normal_cs = normalize(use_normal_cs);

	vec3 frag_to_cam = normalize(-pos_cs);


	const float ior = 2.5f;

	vec3 sunrefl_h = normalize(frag_to_cam + sundir_cs.xyz);
	float sunrefl_h_cos_theta = abs(dot(sunrefl_h, unit_normal_cs));
	float roughness = 0.2;
	float fresnel_scale = 1.0;
	float sun_specular = trowbridgeReitzPDF(sunrefl_h_cos_theta, max(1.0e-8f, alpha2ForRoughness(roughness))) * 
		fresnel_scale * fresnellApprox(sunrefl_h_cos_theta, ior);


	vec3 unit_normal_ws = normalize(normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0)
		unit_normal_ws = -unit_normal_ws;

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

	vec4 transmission_col = diffuse_colour;

	float spec_refl_cos_theta = abs(dot(frag_to_cam, unit_normal_cs));
	float spec_refl_fresnel = fresnellApprox(spec_refl_cos_theta, ior);

	float sun_vis_factor = 1.0f;//TODO: use shadow mapping to compute this.
	vec4 sun_light = vec4(1662102582.6479533,1499657101.1924045,1314152016.0871031, 1) * sun_vis_factor;

	vec4 col = transmission_col*80000000 + spec_refl_light * spec_refl_fresnel + sun_light * sun_specular;

	float alpha = spec_refl_fresnel + sun_specular;

	col *= 0.000000003; // tone-map
	colour_out = vec4(toNonLinear(col.xyz), alpha);


#if DRAW_PLANAR_UV_GRID
	float du_dx = abs(dFdx(use_texture_coords.x));
	float du_dy = abs(dFdy(use_texture_coords.x));

	float dv_dx = abs(dFdx(use_texture_coords.y));
	float dv_dy = abs(dFdy(use_texture_coords.y));

	float a = max(du_dx, du_dy);
	float b = max(dv_dx, dv_dy);

	float border_w_u = max(0.01f, a * 0.5f);
	float border_w_v = max(0.01f, b * 0.5f);
	if(fract(use_texture_coords.x) < border_w_u || fract(use_texture_coords.x) >= (1 - border_w_u) ||
		fract(use_texture_coords.y) < border_w_v || fract(use_texture_coords.y) >= (1 - border_w_v))
		colour_out = vec4(0.2f, 0.8f, 0.54f, 1.f);
#endif

#if USE_LOGARITHMIC_DEPTH_BUFFER
	float farplane = 10000.0;
	float Fcoef = 2.0 / log2(farplane + 1.0);
	float Fcoef_half = 0.5 * Fcoef;
	gl_FragDepth = log2(flogz) * Fcoef_half;
#endif
}
