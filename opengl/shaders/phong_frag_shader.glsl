
#if USE_BINDLESS_TEXTURES
#extension GL_ARB_bindless_texture : require
#endif

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

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D metallic_roughness_tex;
uniform sampler2D emission_tex;
#endif

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
#if !USE_BINDLESS_TEXTURES
uniform sampler2D lightmap_tex;
#endif
#endif


layout (std140) uniform MaterialCommonUniforms
{
	vec4 sundir_cs;
	float time;
};

#define HAVE_SHADING_NORMALS_FLAG			1
#define HAVE_TEXTURE_FLAG					2
#define HAVE_METALLIC_ROUGHNESS_TEX_FLAG	4
#define HAVE_EMISSION_TEX_FLAG				8

#if USE_MULTIDRAW_ELEMENTS_INDIRECT

in flat int material_index;

struct MaterialData
{
	vec4 diffuse_colour;
	vec4 emission_colour;
	vec2 texture_upper_left_matrix_col0;
	vec2 texture_upper_left_matrix_col1;
	vec2 texture_matrix_translation;

#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
	sampler2D metallic_roughness_tex;
	sampler2D lightmap_tex;
	sampler2D emission_tex;
#else
	float padding0;
	float padding1;
	float padding2;
	float padding3;
	float padding4;
	float padding5;
	float padding6;
	float padding7;	
#endif

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance;

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;
};


layout(std140) uniform PhongUniforms // MaterialDataUBO
{
	MaterialData material_data[256];
};

#define MAT_UNIFORM mat_data

#define DIFFUSE_TEX mat_data.diffuse_tex
#define METALLIC_ROUGHNESS_TEX mat_data.metallic_roughness_tex
#define LIGHTMAP_TEX mat_data.lightmap_tex
#define EMISSION_TEX mat_data.emission_tex

#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PhongUniforms
{
	vec4 diffuse_colour;		// 4
	vec4 emission_colour;
	vec2 texture_upper_left_matrix_col0;
	vec2 texture_upper_left_matrix_col1;
	vec2 texture_matrix_translation;

#if USE_BINDLESS_TEXTURES
	sampler2D diffuse_tex;
	sampler2D metallic_roughness_tex;
	sampler2D lightmap_tex;
	sampler2D emission_tex;
#else
	float padding0;
	float padding1;
	float padding2;
	float padding3;
	float padding4;
	float padding5;				// 6
	float padding6;
	float padding7;	
#endif

	int flags;
	float roughness;
	float fresnel_scale;
	float metallic_frac;
	float begin_fade_out_distance;
	float end_fade_out_distance; // 9

	float materialise_lower_z;
	float materialise_upper_z;
	float materialise_start_time;

	ivec4 light_indices_0; // Can't use light_indices[8] here because of std140's retarded array layout rules.
	ivec4 light_indices_1;
} mat_data;

#define MAT_UNIFORM mat_data

#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX mat_data.diffuse_tex
#define METALLIC_ROUGHNESS_TEX mat_data.metallic_roughness_tex
#define LIGHTMAP_TEX mat_data.lightmap_tex
#define EMISSION_TEX mat_data.emission_tex
#else
#define DIFFUSE_TEX diffuse_tex
#define METALLIC_ROUGHNESS_TEX metallic_roughness_tex
#define LIGHTMAP_TEX lightmap_tex
#define EMISSION_TEX emission_tex
#endif

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT


#if BLOB_SHADOWS
uniform int num_blob_positions;
uniform vec4 blob_positions[8];
#endif


struct LightData
{
	vec4 pos;
	vec4 dir;
	vec4 col;
	int light_type; // 0 = point light, 1 = spotlight
	float cone_cos_angle_start;
	float cone_cos_angle_end;
};

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


float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }


float length2(vec2 v) { return dot(v, v); }

float alpha2ForRoughness(float r)
{
	//return pow6(r);
	return pow4(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}


float fresnelApprox(float cos_theta_i, float n2)
{
	//float r_0 = square((1.0 - n2) / (1.0 + n2));
	//return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i); // https://en.wikipedia.org/wiki/Schlick%27s_approximation

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


float sampleDynamicDepthMap(mat2 R, vec3 shadow_coords, float bias)
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
		sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias));
	}
	return sum * (1.f / 16);
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords, float bias)
{
	// This technique gives sharper shadows, so will use for static depth maps to avoid shadows on smaller objects being blurred away.
	float sum = 0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(static_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias));
	}
	return sum * (1.f / 16);
}


// Convert a non-linear sRGB colour to linear sRGB.
// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
vec3 toLinearSRGB(vec3 c)
{
	vec3 c2 = c * c;
	return c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
}


#if MATERIALISE_EFFECT

// https://www.shadertoy.com/view/MdcfDj
#define M1 1597334677U     //1719413*929
#define M2 3812015801U     //140473*2467*11
float hash( uvec2 q )
{
	q *= uvec2(M1, M2); 

	uint n = (q.x ^ q.y) * M1;

	return float(n) * (1.0/float(0xffffffffU));
}

// https://iquilezles.org/articles/functions/
float cubicPulse( float c, float w, float x )
{
	x = abs(x - c);
	if( x>w ) return 0.0;
	x /= w;
	return 1.0 - x*x*(3.0-2.0*x);
}

// https://www.shadertoy.com/view/cscSW8
#define SQRT_3 1.7320508

vec2 closestHexCentre(vec2 p)
{
	vec2 grid_p = vec2(p.x, p.y * (1.0 / SQRT_3));

	// Alternating rows of hexagon centres form their own rectanglular lattices.
	// Find closest hexagon centre on each lattice.
	vec2 p_1 = (floor((grid_p + vec2(1,1)) * 0.5) * 2.0            ) * vec2(1, SQRT_3);
	vec2 p_2 = (floor( grid_p              * 0.5) * 2.0 + vec2(1,1)) * vec2(1, SQRT_3);

	// Now return the closest centre from the two lattices.
	float d_1 = length2(p - p_1);
	float d_2 = length2(p - p_2);
	return d_1 < d_2 ? p_1 : p_2;
}


// Fraction along line from hexagon centre at (0,0) through p to hexagon edge.
// Also 1 - distance from boundary (I think)
float hexFracToEdge(in vec2 p)
{
	p = abs(p);
	float right_frac = p.x;
	float upper_right_frac = dot(p, vec2(0.5, 0.5*SQRT_3));
	return max(right_frac, upper_right_frac);
}
#endif // MATERIALISE_EFFECT


void main()
{
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	MaterialData mat_data = material_data[material_index];
#endif

	vec3 use_normal_cs;
	vec3 use_normal_ws;
	vec2 use_texture_coords = texture_coords;
	if((MAT_UNIFORM.flags & HAVE_SHADING_NORMALS_FLAG) != 0)
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


	vec2 main_tex_coords = MAT_UNIFORM.texture_upper_left_matrix_col0 * use_texture_coords.x + MAT_UNIFORM.texture_upper_left_matrix_col1 * use_texture_coords.y + MAT_UNIFORM.texture_matrix_translation;

	vec4 sun_texture_diffuse_col;
	vec4 refl_texture_diffuse_col;
	if((MAT_UNIFORM.flags & HAVE_TEXTURE_FLAG) != 0)
	{
#if DOUBLE_SIDED
		// Work out if we are seeing the front or back face of the material
		float frag_to_cam_dot_normal = dot(frag_to_cam, unit_normal_cs);
		if(frag_to_cam_dot_normal < 0.f)
		{
			refl_texture_diffuse_col = texture(backface_diffuse_tex, main_tex_coords); // backface
			//refl_texture_diffuse_col.xyz = vec3(1,0,0);//TEMP
		}
		else
			refl_texture_diffuse_col = texture(DIFFUSE_TEX,          main_tex_coords); // frontface


		if(frag_to_cam_dot_normal * light_cos_theta <= 0.f) // If frag_to_cam and sundir_cs in different geometric hemispheres:
		{
			sun_texture_diffuse_col = texture(transmission_tex, main_tex_coords);
			//sun_texture_diffuse_col.xyz = vec3(1,0,0);//TEMP
		}
		else
			sun_texture_diffuse_col = refl_texture_diffuse_col; // Else sun is illuminating the face facing the camera.
#else
		sun_texture_diffuse_col = texture(DIFFUSE_TEX, main_tex_coords);
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
	vec4 sun_diffuse_col  = sun_texture_diffuse_col  * MAT_UNIFORM.diffuse_colour; // diffuse_colour is linear sRGB already.
	vec4 refl_diffuse_col = refl_texture_diffuse_col * MAT_UNIFORM.diffuse_colour;

	// Apply vertex colour, if enabled.
#if VERT_COLOURS
	vec3 linear_vert_col = toLinearSRGB(vert_colour);
	sun_diffuse_col.xyz *= linear_vert_col;
	refl_diffuse_col.xyz *= linear_vert_col;
#endif

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1 / 128.f)).x;
	// Fade out (to fade in an imposter), if enabled.
#if IMPOSTERABLE
	float dist_alpha_factor = smoothstep(MAT_UNIFORM.begin_fade_out_distance, MAT_UNIFORM.end_fade_out_distance,  /*dist=*/-pos_cs.z);
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
	{
		refl_diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
		sun_diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
	}
#endif

	vec3 unit_cam_to_pos_ws = normalize(cam_to_pos_ws);

	// Flip normal into hemisphere camera is in.
	vec3 unit_normal_ws = normalize(use_normal_ws);
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0)
		unit_normal_ws = -unit_normal_ws;

	float final_metallic_frac = ((MAT_UNIFORM.flags & HAVE_METALLIC_ROUGHNESS_TEX_FLAG) != 0) ? (MAT_UNIFORM.metallic_frac * texture(METALLIC_ROUGHNESS_TEX, main_tex_coords).b) : MAT_UNIFORM.metallic_frac;
	float final_roughness     = ((MAT_UNIFORM.flags & HAVE_METALLIC_ROUGHNESS_TEX_FLAG) != 0) ? (MAT_UNIFORM.roughness     * texture(METALLIC_ROUGHNESS_TEX, main_tex_coords).g) : MAT_UNIFORM.roughness;

	float final_fresnel_scale = MAT_UNIFORM.fresnel_scale * (1 - square(final_roughness)); // Reduce fresnel reflection at higher roughnesses

	//----------------------- Direct lighting from interior lights ----------------------------
	// Load indices into a local array, so we can iterate over the array in a for loop.  TODO: find a better way of doing this.
	int indices[8];
	indices[0] = MAT_UNIFORM.light_indices_0.x;
	indices[1] = MAT_UNIFORM.light_indices_0.y;
	indices[2] = MAT_UNIFORM.light_indices_0.z;
	indices[3] = MAT_UNIFORM.light_indices_0.w;
	indices[4] = MAT_UNIFORM.light_indices_1.x;
	indices[5] = MAT_UNIFORM.light_indices_1.y;
	indices[6] = MAT_UNIFORM.light_indices_1.z;
	indices[7] = MAT_UNIFORM.light_indices_1.w;

	vec4 local_light_radiance = vec4(0.f);
	for(int i=0; i<8; ++i)
	{
		int light_index = indices[i];
		if(light_index >= 0)
		{
			vec3 light_emitted_radiance = light_data[light_index].col.xyz;

			vec3 pos_to_light = light_data[light_index].pos.xyz - pos_ws;
			float pos_to_light_len2 = dot(pos_to_light, pos_to_light);
			vec3 unit_pos_to_light = pos_to_light * inversesqrt(pos_to_light_len2);

			float dir_factor;
			if(light_data[light_index].light_type == 0) // Point light:
			{
				dir_factor = 1;
			}
			else
			{
				// light_type == 1: spotlight
				float from_light_cos_angle = -dot(light_data[light_index].dir.xyz, unit_pos_to_light);
				dir_factor =
					smoothstep(0.4f, 0.9f, from_light_cos_angle) * 0.03 + // A little light outside of the main cone
					smoothstep(light_data[light_index].cone_cos_angle_start, light_data[light_index].cone_cos_angle_end, from_light_cos_angle);
			}

			float cos_theta_term = max(0.f, dot(unit_normal_ws, unit_pos_to_light));

			vec3 diffuse_bsdf = refl_diffuse_col.xyz * (1.0 / 3.141592653589793);

			// Compute specular bsdf
			vec3 h_ws = normalize(unit_pos_to_light - unit_cam_to_pos_ws);
			float h_cos_theta = abs(dot(h_ws, unit_normal_ws));
			vec4 specular_fresnel;
			{
				vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * final_fresnel_scale;
				vec4 metal_fresnel = vec4(
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.r),
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.g),
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.b),
					1);

				// Blend between metal_fresnel and dielectric_fresnel based on metallic_frac.
				specular_fresnel = mix(dielectric_fresnel, metal_fresnel, final_metallic_frac);
			}

			vec4 specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(final_roughness))) * specular_fresnel;

			vec3 bsdf = diffuse_bsdf + specular.xyz;
			vec3 reflected_radiance = bsdf * cos_theta_term * light_emitted_radiance * dir_factor / pos_to_light_len2;

			local_light_radiance.xyz += reflected_radiance;
		}
	}


	//------------- Compute specular microfacet terms for sun lighting --------------
	//float h_cos_theta = max(0.0, dot(h, unit_normal_cs));
	float h_cos_theta = abs(dot(h, unit_normal_cs));
	vec4 specular_fresnel;
	{
		vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * final_fresnel_scale;
		vec4 metal_fresnel = vec4(
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.r),
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.g),
			metallicFresnelApprox(h_cos_theta, refl_diffuse_col.b),
			1);

		// Blend between metal_fresnel and dielectric_fresnel based on metallic_frac.
		specular_fresnel = mix(dielectric_fresnel, metal_fresnel, final_metallic_frac);
	}

	vec4 specular = trowbridgeReitzPDF(h_cos_theta, max(1.0e-8f, alpha2ForRoughness(final_roughness))) * specular_fresnel;

	float shadow_factor = smoothstep(-0.3, 0.0, dot(sundir_cs.xyz, unit_normal_cs));
	specular *= shadow_factor;

	// Shadow mapping
	float sun_vis_factor;
#if SHADOW_MAPPING

#define VISUALISE_CASCADES 0

	float samples_scale = 1.f;

	float pattern_theta = pixel_hash * 6.283185307179586f;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta));

	sun_vis_factor = 0.0;

	float depth_map_0_bias = 8.0e-5f;
	float depth_map_1_bias = 4.0e-4f;
	float static_depth_map_bias = 2.2e-3f;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
		{
			float tex_0_vis = sampleDynamicDepthMap(R, shadow_tex_coords[0], /*bias=*/depth_map_0_bias);

			float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
			if(dist > edge_dist)
			{
				float tex_1_vis = sampleDynamicDepthMap(R, shadow_tex_coords[1], /*bias=*/depth_map_1_bias);

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
			sun_vis_factor = sampleDynamicDepthMap(R, shadow_tex_coords[1], /*bias=*/depth_map_1_bias);

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float static_sun_vis_factor = sampleStaticDepthMap(R, static_shadow_cds, static_depth_map_bias); // NOTE: had 0.999f cap and bias of 0.0005: min(static_shadow_cds.z, 0.999f) - bias

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
			sun_vis_factor = sampleStaticDepthMap(R, shadow_cds, static_depth_map_bias); // NOTE: had cap and bias

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					int next_tex_index = static_depth_tex_index + 1;
					vec3 next_shadow_cds = shadow_tex_coords[next_tex_index + NUM_DYNAMIC_DEPTH_TEXTURES];

					float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds, static_depth_map_bias); // NOTE: had cap and bias

					float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
					sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
				}
			}
#endif

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
#if RENDER_CLOUD_SHADOWS
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
#endif


	

	vec4 sky_irradiance;
#if LIGHTMAPPING
	// glTexImage2D expects the start of the texture data to be the lower left of the image, whereas it is actually the upper left.  So flip y coord to compensate.
	sky_irradiance = texture(LIGHTMAP_TEX, vec2(lightmap_coords.x, -lightmap_coords.y)) * 1.0e9f;
#else
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz) * 1.0e9f; // integral over hemisphere of cosine * incoming radiance from sky.
#endif

#if BLOB_SHADOWS
	for(int i=0; i<num_blob_positions; ++i)
	{
		vec3 pos_to_blob_centre = blob_positions[i].xyz - pos_ws;
		float r = 0.4;
		float r2 = r * r;
		float d2 = dot(pos_to_blob_centre, pos_to_blob_centre); // d^2
	
//		float bl = max(0.f, dot(unit_normal_ws, normalize(pos_to_blob_centre))) * min(1.f, r2 / d2);
		float bl = max(0.f, dot(unit_normal_ws, pos_to_blob_centre) * inversesqrt(d2)) * min(1.f, r2 / d2);
	
		float vis = 1 - bl * 0.7f;
		sky_irradiance *= vis;
		local_light_radiance *= vis;
	}
#endif


	// Reflect cam-to-fragment vector in ws normal
	vec3 reflected_dir_ws = unit_cam_to_pos_ws - unit_normal_ws * (2.0 * dot(unit_normal_ws, unit_cam_to_pos_ws));

	//========================= Look up env map for reflected dir ============================
	
	int map_lower = int(final_roughness * 6.9999);
	int map_higher = map_lower + 1;
	float map_t = final_roughness * 6.9999 - float(map_lower);

	float refl_theta = acos(reflected_dir_ws.z);
	float refl_phi = atan(reflected_dir_ws.y, reflected_dir_ws.x) - 1.f; // -1.f is to rotate reflection so it aligns with env rotation.
	vec2 refl_map_coords = vec2(refl_phi * (1.0 / 6.283185307179586), clamp(refl_theta * (1.0 / 3.141592653589793), 1.0 / 64, 1 - 1.0 / 64)); // Clamp to avoid texture coord wrapping artifacts.

	vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, map_lower  * (1.0/8) + refl_map_coords.y * (1.0/8))) * 1.0e9f; //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
	vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, map_higher * (1.0/8) + refl_map_coords.y * (1.0/8))) * 1.0e9f;
	vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t;


	float fresnel_cos_theta = max(0.0, dot(reflected_dir_ws, unit_normal_ws));
	vec4 dielectric_refl_fresnel = vec4(fresnelApprox(fresnel_cos_theta, 1.5) * final_fresnel_scale);
	vec4 metallic_refl_fresnel = vec4(
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.r),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.g),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.b),
		1)/* * fresnel_scale*/;

	vec4 refl_fresnel = metallic_refl_fresnel * final_metallic_frac + dielectric_refl_fresnel * (1.0f - final_metallic_frac);

	vec4 sun_light = vec4(1662102582.6479533,1499657101.1924045,1314152016.0871031, 1) * sun_vis_factor; // Sun spectral radiance multiplied by solid angle, see SkyModel2Generator::makeSkyEnvMap().

	vec4 emission_col = MAT_UNIFORM.emission_colour;
	if((MAT_UNIFORM.flags & HAVE_EMISSION_TEX_FLAG) != 0)
	{
		emission_col *= texture(EMISSION_TEX, main_tex_coords);
	}

	
#if MATERIALISE_EFFECT
	// box mapping
	vec2 materialise_coords;
	if(abs(normal_ws.x) > abs(normal_ws.y))
	{
		if(abs(normal_ws.x) > abs(normal_ws.z)) // |x| > |z| && |x| > |y|
			materialise_coords = pos_ws.yz;
		else // |z| >= |x| > |y|:
			materialise_coords = pos_ws.xy;
	}
	else // else |y| >= |x|:
	{
		if(abs(normal_ws.y) > abs(normal_ws.z))
			materialise_coords = pos_ws.xz;
		else // |z| >= |y| >= |x|
			materialise_coords = pos_ws.xy;
	}

	float sweep_speed_factor = 1.0;
	float sweep_frac = (pos_ws.z - MAT_UNIFORM.materialise_lower_z) / (MAT_UNIFORM.materialise_upper_z - MAT_UNIFORM.materialise_lower_z);
	float materialise_stage = fbmMix(materialise_coords * 0.2) * 0.4 + sweep_frac;
	float use_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor - materialise_stage - 0.3;

	float band_1_centre = 0.1;
	float band_2_centre = 0.8;
	float band_1 = cubicPulse(/*centre*/band_1_centre, /*width=*/0.06, use_frac);
	float band_2 = cubicPulse(/*centre*/band_2_centre, /*width=*/0.06, use_frac);

	if(use_frac < band_1_centre)
		discard;
	if(use_frac >= band_1_centre + 0.05 && use_frac < band_2_centre)
	{
		vec2 hex_centre = closestHexCentre(materialise_coords * 40.0);
		float hex_frac = hexFracToEdge(hex_centre - materialise_coords * 40.0);

		float materialise_pixel_hash = hash(uvec2(hex_centre + vec2(100000.0,100000.0)));

		float hex_interior_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor;// - materialise_stage - 0.3;
		if(hex_interior_frac < materialise_pixel_hash + /*fill-in delay=*/0.7 + sweep_frac)
			discard;

		emission_col =  vec4(0.0,0.2,0.5, 0) * //vec4(MAT_UNIFORM.materialise_r, MAT_UNIFORM.materialise_g, MAT_UNIFORM.materialise_b, 0.0) * 
			smoothstep(0.8, 0.9, hex_frac) * 2.0e9;
	}

	emission_col += (band_1 * vec4(1,1,1.0,0)  + band_2 * vec4(0.0,1,0.5,0)) * 5.0e9;
#endif // MATERIALISE_EFFECT


	vec4 col =
		sky_irradiance * sun_diffuse_col * (1.0 / 3.141592653589793) * (1.0 - refl_fresnel) * (1.0 - final_metallic_frac) +  // Diffuse substrate part of BRDF * incoming radiance from sky
		refl_fresnel * spec_refl_light + // Specular reflection of sky
		sun_light * (1.0 - refl_fresnel) * (1.0 - final_metallic_frac) * refl_diffuse_col * (1.0 / 3.141592653589793) * sun_light_cos_theta_factor + //  Diffuse substrate part of BRDF * sun light
		sun_light * specular + // sun light * specular microfacet terms
		local_light_radiance + // Reflected light from local light sources.
		emission_col;
	
	//vec4 col = (sun_light + 3000000000.0)  * diffuse_col;

#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	float fog_factor = 1.0f - exp(dist_ * -0.00015);
	vec4 sky_col = vec4(1.8, 4.7, 8.0, 1) * 2.0e7; // Bluish grey
	col = mix(col, sky_col, fog_factor);
#endif

	col *= 0.000000003; // tone-map
	
#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, 1); // toNonLinear will be done after adding blurs etc.
#else
	colour_out = vec4(toNonLinear(col.xyz), 1);
#endif
}
