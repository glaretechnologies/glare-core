#version 330 core

//#version 150

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


//uniform vec4 sundir_cs;

uniform sampler2D diffuse_tex;
uniform sampler2D dynamic_depth_tex;
uniform sampler2D static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;
#if LIGHTMAPPING
uniform sampler2D lightmap_tex;
#endif


layout (std140) uniform PhongUniforms
{
	//							// base alignment 	// aligned offset   // size
	vec4 sundir_cs;				// 16				// 0
	vec4 diffuse_colour;		// 16				// 0				//  16
	mat3 texture_matrix;		// 16				// 16				//  48
	int have_shading_normals;	// 4				// 64				// 4
	int have_texture;			// 4				// 68				// 4
	float roughness;			// 4				// 72				// 4
	float fresnel_scale;		// 4				// 76 				// 4
	float metallic_frac;		// 4				// 80				// 4
	// total:										// 84				
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
	vec2(327, 128),
	vec2(789, 168),
	vec2(507, 219),
	vec2(200, 439),
	vec2(409, 392),
	vec2(599, 401),
	vec2(490, 470),
	vec2(387, 574),
	vec2(546, 535),
	vec2(686, 530),
	vec2(814, 545),
	vec2(496, 648),
	vec2(42, 724),
	vec2(383, 802),
	vec2(599, 716),
	vec2(865, 367)
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
	const float border_w = 0.01;
	if(	fract(use_texture_coords.x) < border_w || fract(use_texture_coords.x) >= (1 - border_w) ||
		fract(use_texture_coords.y) < border_w || fract(use_texture_coords.y) >= (1 - border_w))
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

	// Shadow mapping
	float sun_vis_factor;
#if SHADOW_MAPPING
	
	int pixel_index = int((gl_FragCoord.y * 1920.0 + gl_FragCoord.x));
	float pattern_theta = float(float(ha(uint(pixel_index))) * (6.283185307179586 / 4294967296.0));
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta));

	sun_vis_factor = 0.0;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		// Select correct depth map
		int dynamic_depth_tex_index;
		if(dist < DEPTH_TEXTURE_SCALE_MULT)
			dynamic_depth_tex_index = 0;
		else
			dynamic_depth_tex_index = 1;

		vec3 shadow_cds = shadow_tex_coords[dynamic_depth_tex_index];
		vec2 depth_texcoords = shadow_cds.xy;

		float actual_depth = min(shadow_cds.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

		// Compute coords in cascaded map
		depth_texcoords.y = (float(dynamic_depth_tex_index) + depth_texcoords.y) * (1.0 / float(NUM_DYNAMIC_DEPTH_TEXTURES));

		for(int i=0; i<16; ++i)
		{
			vec2 st = depth_texcoords + R * ((samples[i] * 0.001) - vec2(0.5, 0.5)) * (4.0 / 2048.0);
			float light_depth = texture(dynamic_depth_tex, st).x;

			sun_vis_factor += (light_depth > actual_depth) ? (1.0 / 16.0) : 0.0;
		}

		//col.r = float(dynamic_depth_tex_index) / NUM_DYNAMIC_DEPTH_TEXTURES; // TEMP: visualise depth texture used.
	}
	else
	{
		float l1dist = dist;
	
		if(l1dist < 1024)
		{
			int static_depth_tex_index;
			if(l1dist < 64)
				static_depth_tex_index = 0;
			else if(l1dist < 256)
				static_depth_tex_index = 1;
			else
				static_depth_tex_index = 2;

			//col.g = float(static_depth_tex_index) / NUM_STATIC_DEPTH_TEXTURES; // TEMP: visualise depth texture used.

			vec3 shadow_cds = shadow_tex_coords[static_depth_tex_index + NUM_DYNAMIC_DEPTH_TEXTURES];
			vec2 depth_texcoords = shadow_cds.xy;

			float bias = 0.0005;// / abs(light_cos_theta);
			float actual_depth = min(shadow_cds.z, 0.999f) - bias; // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

			// Compute coords in cascaded map
			depth_texcoords.y = (float(static_depth_tex_index) + depth_texcoords.y) * (1.0 / float(NUM_STATIC_DEPTH_TEXTURES));

			for(int i=0; i<16; ++i)
			{
				vec2 st = depth_texcoords + R * ((samples[i] * 0.001) - vec2(0.5, 0.5)) * (4.0 / 2048.0);
				float light_depth = texture(static_depth_tex, st).x;

				sun_vis_factor += (light_depth > actual_depth) ? (1.0 / 16.0) : 0.0;
			}
		}
		else
			sun_vis_factor = 1.0;
	}

#else
	sun_vis_factor = 1.0;
#endif

	vec3 unit_normal_ws = normalize(use_normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0)
		unit_normal_ws = -unit_normal_ws;

	vec4 sky_irradiance;
#if LIGHTMAPPING
	// glTexImage2D expects the start of the texture data to be the lower left of the image, whereas it is actually the upper left.  So flip y coord to compensate.
	sky_irradiance = texture(lightmap_tex, vec2(lightmap_coords.x, -lightmap_coords.y)) * 1.0e9f;
#else
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz); // integral over hemisphere of cosine * incoming radiance from sky.
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

	vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, map_lower  * (1.0/8) + refl_map_coords.y * (1.0/8))); //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
	vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, map_higher * (1.0/8) + refl_map_coords.y * (1.0/8)));
	vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t;


	float fresnel_cos_theta = max(0.0, dot(reflected_dir_ws, unit_normal_ws));
	vec4 dielectric_refl_fresnel = vec4(fresnelApprox(fresnel_cos_theta, 1.5) * fresnel_scale);
	vec4 metallic_refl_fresnel = vec4(
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.r),
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.g),
		metallicFresnelApprox(fresnel_cos_theta, diffuse_col.b),
		1)/* * fresnel_scale*/;

	vec4 refl_fresnel = metallic_refl_fresnel * metallic_frac + dielectric_refl_fresnel * (1.0f - metallic_frac);

	vec4 sun_light = vec4(9124154304.569067, 8038831044.193394, 7154376815.37873, 1) * sun_vis_factor;

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
		
	col *= 0.0000000005; // tone-map
	
	colour_out = vec4(toNonLinear(col.xyz), 1);
}
