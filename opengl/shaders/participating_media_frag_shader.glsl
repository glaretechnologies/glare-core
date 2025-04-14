
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


//#if USE_SSBOS
//layout (std430) buffer LightDataStorage
//{
//	LightData light_data[];
//};
//#else
//layout (std140) uniform LightDataStorage
//{
//	LightData light_data[256];
//};
//#endif


out vec4 colour_out;


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
#if SHADOW_MAPPING
	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.0 / 128.f)).x;

	float sun_vis_factor = getShadowMappingSunVisFactor(shadow_tex_coords, dynamic_depth_tex, static_depth_tex, pixel_hash, pos_cs, shadow_map_samples_xy_scale);
#else
	float sun_vis_factor = 1.0;
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
		float cumulus_val = max(0.f, fbmMix(cumulus_coords, fbm_tex) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}
#endif


	vec3 sun_light = sun_spec_rad_times_solid_angle.xyz * sun_vis_factor;

	vec3 up_factor    = sun_up_factor    * sun_light;
	vec3 bot_factor   = sun_bot_factor   * sun_light;
	vec3 left_factor  = sun_left_factor  * sun_light;
	vec3 right_factor = sun_right_factor * sun_light;
	vec3 rear_factor  = sun_rear_factor  * sun_light;
	vec3 front_factor = sun_front_factor * sun_light;


	vec3 up_sky_irradiance    = texture(cosine_env_tex, up_ws).xyz    ; // integral over hemisphere of cosine * incoming radiance from sky * 1.0e-9.
	vec3 down_sky_irradiance  = texture(cosine_env_tex, -up_ws).xyz   ;
	vec3 left_sky_irradiance  = texture(cosine_env_tex, -right_ws).xyz;
	vec3 right_sky_irradiance = texture(cosine_env_tex, right_ws).xyz ;
	vec3 rear_sky_irradiance  = texture(cosine_env_tex, forw_ws).xyz  ;
	vec3 front_sky_irradiance = texture(cosine_env_tex, -forw_ws).xyz ;

	up_factor    += up_sky_irradiance    * (1.0 / 3.141592653589793) * (1.0 / 2.0); // Is 1/6 the correct factor here?
	bot_factor   += down_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 2.0);
	left_factor  += left_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 2.0);
	right_factor += right_sky_irradiance * (1.0 / 3.141592653589793) * (1.0 / 2.0);
	rear_factor  += rear_sky_irradiance  * (1.0 / 3.141592653589793) * (1.0 / 2.0);
	front_factor += front_sky_irradiance * (1.0 / 3.141592653589793) * (1.0 / 2.0);

	// Sample sprite textures for the different light directions
	vec4 up_tex_col    = texture(DIFFUSE_TEX, main_tex_coords);
	vec4 bot_tex_col   = texture(METALLIC_ROUGHNESS_TEX, main_tex_coords);
	vec4 left_tex_col  = texture(LIGHTMAP_TEX, main_tex_coords);
	vec4 right_tex_col = texture(EMISSION_TEX, main_tex_coords);
	vec4 rear_tex_col  = texture(BACKFACE_ALBEDO_TEX, main_tex_coords);
	vec4 front_tex_col = texture(TRANSMISSION_TEX, main_tex_coords);

	// materialise_start_time = particle spawn time
	float life_time = time - MAT_UNIFORM.materialise_start_time;
	float overall_alpha_factor = max(0.0, min(1.0, MAT_UNIFORM.diffuse_colour.w + life_time * MAT_UNIFORM.dopacity_dt));

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
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1.0 - transmission);
#endif

#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, alpha);
#else
	colour_out = vec4(toNonLinear(col.xyz), alpha);
#endif
}
