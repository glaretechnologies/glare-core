
in vec3 pos_cs;
in vec3 pos_ws;
in vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
in vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
//in vec3 cam_to_pos_ws; // Camera to fragment position vector
in vec3 rotated_right_ws;
in vec3 rotated_up_ws;

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D metallic_roughness_tex;
uniform sampler2D lightmap_tex;
uniform sampler2D emission_tex;
uniform sampler2D backface_albedo_tex;
uniform sampler2D transmission_tex;
#endif
uniform sampler2DShadow dynamic_depth_tex;
uniform sampler2DShadow static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;


//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

flat in int material_index;


layout(std430) buffer PhongUniforms
{
	MaterialData material_data[];
};

#define MAT_UNIFORM					material_data[material_index]

#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define METALLIC_ROUGHNESS_TEX		MAT_UNIFORM.metallic_roughness_tex
#define LIGHTMAP_TEX				MAT_UNIFORM.lightmap_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex
#define BACKFACE_ALBEDO_TEX			MAT_UNIFORM.backface_albedo_tex
#define TRANSMISSION_TEX			MAT_UNIFORM.transmission_tex

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:


layout (std140) uniform PhongUniforms
{
	MaterialData matdata;

} mat_data;

#define MAT_UNIFORM mat_data.matdata


#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define METALLIC_ROUGHNESS_TEX		MAT_UNIFORM.metallic_roughness_tex
#define LIGHTMAP_TEX				MAT_UNIFORM.lightmap_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex
#define BACKFACE_ALBEDO_TEX			MAT_UNIFORM.backface_albedo_tex
#define TRANSMISSION_TEX			MAT_UNIFORM.transmission_tex

#else
#define DIFFUSE_TEX					diffuse_tex
#define METALLIC_ROUGHNESS_TEX		metallic_roughness_tex
#define LIGHTMAP_TEX				lightmap_tex
#define EMISSION_TEX				emission_tex
#define BACKFACE_ALBEDO_TEX			backface_albedo_tex
#define TRANSMISSION_TEX			transmission_tex
#endif

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------


#if USE_SSBOS
layout (std430) buffer LightDataStorage
{
	LightData light_data[];
};
#else
layout (std140) uniform LightDataStorage
{
	LightData light_data[256];
};
#endif


out vec4 colour_out;


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
	float sum = 0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(static_depth_tex, vec3(st.x, st.y, shadow_coords.z));
	}
	return sum * (1.f / 16);
}

float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }



void main()
{
	vec2 main_tex_coords = texture_coords;
	main_tex_coords.y = 1.0 - main_tex_coords.y; // TODO: use tex matrix?

	// rotated_right_ws, rotated_up_ws may not be unit length and orthogonal now due to interpolation.
	vec3 right_ws = normalize(rotated_right_ws);
	vec3 up_ws    = normalize(rotated_up_ws);
	vec3 forw_ws  = normalize(cross(up_ws, right_ws));
	

	float sun_up_factor    = max(0.0, dot(sundir_ws.xyz,  up_ws));
	float sun_bot_factor   = max(0.0, dot(sundir_ws.xyz, -up_ws));
	float sun_left_factor  = max(0.0, dot(sundir_ws.xyz, -right_ws));
	float sun_right_factor = max(0.0, dot(sundir_ws.xyz,  right_ws));
	float sun_rear_factor  = max(0.0, dot(sundir_ws.xyz,  forw_ws));
	float sun_front_factor = max(0.0, dot(sundir_ws.xyz, -forw_ws));

	// normalise
	float sum = 
		sun_up_factor    +
		sun_bot_factor   +
		sun_left_factor  +
		sun_right_factor +
		sun_rear_factor  +
		sun_front_factor;

	float norm_factor = 1.0 / sum;
	sun_up_factor    *= norm_factor;
	sun_bot_factor   *= norm_factor;
	sun_left_factor  *= norm_factor;
	sun_right_factor *= norm_factor;
	sun_rear_factor  *= norm_factor;
	sun_front_factor *= norm_factor;

	// Shadow mapping
	float sun_vis_factor;
#if SHADOW_MAPPING

	float samples_scale = 1.f;

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1 / 128.f)).x;

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

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
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
		vec3 cum_layer_pos = pos_ws + sundir_ws.xyz * (1000.f - pos_ws.z) / sundir_ws.z;

		vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
		cum_tex_coords.x += time * 0.002;

		vec2 cumulus_coords = vec2(cum_tex_coords.x * 2 + 2.3453, cum_tex_coords.y * 2 + 1.4354);
		float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}


	vec3 sun_light = sun_spec_rad_times_solid_angle.xyz * sun_vis_factor;

	vec3 up_factor    = sun_up_factor    * sun_light;
	vec3 bot_factor   = sun_bot_factor   * sun_light;
	vec3 left_factor  = sun_left_factor  * sun_light;
	vec3 right_factor = sun_right_factor * sun_light;
	vec3 rear_factor  = sun_rear_factor  * sun_light;
	vec3 front_factor = sun_front_factor * sun_light;


	vec3 up_sky_irradiance    = texture(cosine_env_tex, up_ws).xyz     * 1.0e9f; // integral over hemisphere of cosine * incoming radiance from sky.
	vec3 down_sky_irradiance  = texture(cosine_env_tex, -up_ws).xyz    * 1.0e9f;
	vec3 left_sky_irradiance  = texture(cosine_env_tex, -right_ws).xyz * 1.0e9f;
	vec3 right_sky_irradiance = texture(cosine_env_tex, right_ws).xyz  * 1.0e9f;
	vec3 rear_sky_irradiance  = texture(cosine_env_tex, forw_ws).xyz   * 1.0e9f;
	vec3 front_sky_irradiance = texture(cosine_env_tex, -forw_ws).xyz  * 1.0e9f;

	up_factor    += up_sky_irradiance    * (1.0 / 3.141592653589793) * (1.0 / 6.0); // Is 1/6 the correct factor here?
	bot_factor   += down_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 6.0);
	left_factor  += left_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 6.0);
	right_factor += right_sky_irradiance * (1.0 / 3.141592653589793) * (1.0 / 6.0);
	rear_factor  += rear_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 6.0);
	front_factor += front_sky_irradiance * (1.0 / 3.141592653589793) * (1.0 / 6.0);

	// Sample sprite textures for the different light directions
	vec4 up_tex_col    = texture(DIFFUSE_TEX, main_tex_coords);
	vec4 bot_tex_col   = texture(METALLIC_ROUGHNESS_TEX, main_tex_coords);
	vec4 left_tex_col  = texture(LIGHTMAP_TEX, main_tex_coords);
	vec4 right_tex_col = texture(EMISSION_TEX, main_tex_coords);
	vec4 rear_tex_col  = texture(BACKFACE_ALBEDO_TEX, main_tex_coords);
	vec4 front_tex_col = texture(TRANSMISSION_TEX, main_tex_coords);

	// materialise_start_time = particle spawn time
	// materialise_upper_z = dopacity/dt
	float life_time = time - MAT_UNIFORM.materialise_start_time;
	float overall_alpha_factor = max(0.0, min(1.0, MAT_UNIFORM.diffuse_colour.w + life_time * MAT_UNIFORM.materialise_upper_z));

	float alpha = up_tex_col.a * overall_alpha_factor;

	vec3 col =
		(up_tex_col  .xyz * up_factor + 
		bot_tex_col  .xyz * bot_factor + 
		left_tex_col .xyz * left_factor + 
		right_tex_col.xyz * right_factor + 
		rear_tex_col .xyz * rear_factor + 
		front_tex_col.xyz * front_factor) * MAT_UNIFORM.diffuse_colour.xyz;

#if DEPTH_FOG
	// Blend with background/fog colourbbb
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	vec3 transmission = exp(air_scattering_coeffs.xyz * -dist_);

	col.xyz *= transmission;
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1 - transmission);
#endif

	col *= 0.000000003; // tone-map

#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, alpha);
#else
	colour_out = vec4(toNonLinear(col.xyz), alpha);
#endif
}
