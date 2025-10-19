// Common code used in various shaders


const float PI   = 3.1415926535897932384626433832795;
const float PI_2 = 1.5707963267948966192313216916398;


// From SRGBUtils::fastApproxLinearSRGBToNonLinearSRGB().
vec3 fastApproxLinearSRGBToNonLinearSRGB(vec3 c)
{
	vec3 sqrt_c = sqrt(c);
	vec3 nonlinear = c*(c*0.0404024488f + vec3(-0.19754737849999998f)) + 
		sqrt_c*1.0486722787999998f + sqrt(sqrt_c)*0.1634726509f - vec3(0.055f);

	vec3 linear = c * 12.92f;

	return mix(nonlinear, linear, lessThanEqual(c, vec3(0.0031308f)));
}


vec3 toNonLinear(vec3 v)
{
	return fastApproxLinearSRGBToNonLinearSRGB(v);
}


// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
vec3 fastApproxNonLinearSRGBToLinearSRGB(vec3 c)
{
	vec3 c2 = c * c;
	return c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
}


float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }
float pow5(float x) { return (x*x)*(x*x)*x; }


float length2(vec2 v) { return dot(v, v); }

float alpha2ForRoughness(float r)
{
	return pow4(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return /*cos_theta **/ alpha2 / (PI * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}


// From https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.pdf, B.3.2. Specular BRDF
// Note that we know dot(H, L) >= 0 and dot(H, V) >= from how we constructed H.
float smithMaskingShadowingV(vec3 N, /*vec3 H, */vec3 L, vec3 V, float alpha2)
{
	float N_dot_L = dot(N, L);
	float N_dot_V = dot(N, V);
	if(N_dot_L <= 0.0 || N_dot_V <= 0.0)
		return 0.0;

	return 1.0 / (
		(N_dot_L + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_L))) * 
		(N_dot_V + sqrt(alpha2 + (1.0 - alpha2)*square(N_dot_V)))
	);
}


// See 'Some Fresnel curve approximations', https://forwardscattering.org/post/65
float dielectricFresnelReflForIOR1_5(float cos_theta_i)
{
	float cos_theta_2 = cos_theta_i*cos_theta_i;
	return 
		(-2.4615278*cos_theta_2 +  3.473652*cos_theta_i + -1.9117112) /
		(-13.303401*cos_theta_2 + -7.186081*cos_theta_i + -1.9189386);
}

float dielectricFresnelReflForIOR1_333(float cos_theta_i)
{
	float cos_theta_2 = cos_theta_i*cos_theta_i;
	return 
		(1.1040283f*cos_theta_2 + -1.6791086f*cos_theta_i + 0.86057293f) /
		(9.739124f *cos_theta_2 + 3.293334f  *cos_theta_i + 0.8676968f);
}

float dielectricFresnelReflForIOR2(float cos_theta_i)
{
	float cos_theta_2 = cos_theta_i*cos_theta_i;
	return 
		(-2.703471f *cos_theta_2 + 2.4928381f*cos_theta_i + -1.932341f) /
		(-8.6749525f*cos_theta_2 + -8.674303f*cos_theta_i + -1.9317712f);
}


float metallicFresnelApprox(float cos_theta, float r_0)
{
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}

float rayPlaneIntersect(vec3 raystart, vec3 ray_unitdir, float plane_h)
{
	float start_to_plane_dist = raystart.z - plane_h;

	return start_to_plane_dist / -ray_unitdir.z;
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

float fbm(vec2 p, in sampler2D fbm_tex)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2.0;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbmMix(vec2 p, in sampler2D fbm_tex)
{
	return 
		fbm(p, fbm_tex) +
		fbm(rot(p * 2.0), fbm_tex) * 0.5;
}


float sampleDynamicDepthMap(mat2 R, vec3 shadow_coords, float bias, in sampler2DShadow dynamic_depth_tex)
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
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		// Use textureLod to specify the LOD level explicitly.
		// Without this, the ANGLE D3D11 backend will flatten the branches and execute all depth map lookups, so that it can calculate derivatives.
		// We don't actually need these derivatives since we are just doing bilinear filtering with no mipmaps.
		sum += textureLod(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias), /*lod=*/0.0);
	}
	return sum * (1.f / 16.f);
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords, float bias, in sampler2DShadow static_depth_tex)
{
	// This technique gives sharper shadows, so will use for static depth maps to avoid shadows on smaller objects being blurred away.
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += textureLod(static_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias), /*lod=*/0.0);
	}
	return sum * (1.f / 16.f);
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



#if SHADOW_MAPPING


float getShadowMappingSunVisFactor(in vec3 final_shadow_tex_coords[NUM_DEPTH_TEXTURES], in sampler2DShadow dynamic_depth_tex, in sampler2DShadow static_depth_tex,
	float pixel_hash, vec3 pos_cs, float shadow_map_samples_xy_scale_, float to_light_dot_n)
{
	float sun_vis_factor = 1.0;
	float pattern_theta = pixel_hash * 6.283185307179586f;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta)) * shadow_map_samples_xy_scale_;

	sun_vis_factor = 0.0;

	// These values are eyeballed to be approximately the smallest values without striping artifacts.
	float light_angle_factor = 1.0f / max(to_light_dot_n, 0.3);
	float depth_map_0_bias        = 5.0e-5f  * light_angle_factor;
	float depth_map_1_bias        = 12.0e-5f * light_angle_factor;
	float static_depth_map_0_bias = 5.0e-4f  * light_angle_factor;
	float static_depth_map_1_bias = 10.0e-4f * light_angle_factor;
	float static_depth_map_2_bias = 15.0e-4f * light_angle_factor;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
		{
			float tex_0_vis = sampleDynamicDepthMap(R, final_shadow_tex_coords[0], /*bias=*/depth_map_0_bias, dynamic_depth_tex);

			float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
			if(dist > edge_dist)
			{
				float tex_1_vis = sampleDynamicDepthMap(R, final_shadow_tex_coords[1], /*bias=*/depth_map_1_bias, dynamic_depth_tex);

				float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT, dist);
				sun_vis_factor = mix(tex_0_vis, tex_1_vis, blend_factor);
			}
			else
				sun_vis_factor = tex_0_vis;
		}
		else
		{
			sun_vis_factor = sampleDynamicDepthMap(R, final_shadow_tex_coords[1], /*bias=*/depth_map_1_bias, dynamic_depth_tex);

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = final_shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float static_sun_vis_factor = sampleStaticDepthMap(R, static_shadow_cds, static_depth_map_0_bias, static_depth_tex); // NOTE: had 0.999f cap and bias of 0.0005: min(static_shadow_cds.z, 0.999f) - bias

				float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT, dist);
				sun_vis_factor = mix(sun_vis_factor, static_sun_vis_factor, blend_factor);
			}
		}
	}
	else
	{
		float l1dist = dist;
	
		if(l1dist < 1024.0)
		{
			int static_depth_tex_index;
			float cascade_end_dist;
			vec3 shadow_cds, next_shadow_cds;
			float bias, next_bias;
			if(l1dist < 64.0)
			{
				static_depth_tex_index = 0;
				cascade_end_dist = 64.0;
				shadow_cds      = final_shadow_tex_coords[0 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = final_shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
				bias = static_depth_map_0_bias;
				next_bias = static_depth_map_1_bias;
			}
			else if(l1dist < 256.0)
			{
				static_depth_tex_index = 1;
				cascade_end_dist = 256.0;
				shadow_cds      = final_shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = final_shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
				bias      = static_depth_map_1_bias;
				next_bias = static_depth_map_2_bias;
			}
			else
			{
				static_depth_tex_index = 2;
				cascade_end_dist = 1024.0;
				shadow_cds = final_shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = vec3(0.0); // Suppress warning about being possibly uninitialised.
				bias      = static_depth_map_2_bias;
				next_bias = static_depth_map_2_bias;
			}

			sun_vis_factor = sampleStaticDepthMap(R, shadow_cds, bias, static_depth_tex);

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds, next_bias, static_depth_tex);

					float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
					sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
				}
			}
#endif
		}
		else
			sun_vis_factor = 1.0;
	}
	
	return sun_vis_factor;
}


#endif // end if SHADOW_MAPPING



// Converts a unit vector to a point in octahedral representation ('oct').
// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1.

// Returns +- 1
vec2 signNotZero(vec2 v) {
	return vec2(((v.x >= 0.0) ? 1.0 : -1.0), ((v.y >= 0.0) ? 1.0 : -1.0));
}
// Assume normalized input. Output is on [-1, 1] for each component.
vec2 float32x3_to_oct(in vec3 v) {
	// Project the sphere onto the octahedron, and then onto the xy plane
	vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
	// Reflect the folds of the lower hemisphere over the diagonals
	return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

// Optimized snorm12×2 packing into unorm8×3
// From 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
#if NORMAL_TEXTURE_IS_UINT
uvec4 snorm12x2_to_unorm8x3(vec2 f) {
	vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047.0 + 2047.0));
	float t = floor(u.y / 256.0);
	// If storing to GL_RGB8UI, omit the final division
	return uvec4(uint(u.x / 16.0),
		uint(fract(u.x / 16.0) * 256.0 + t),
		uint(u.y - t * 256.0),
		0);
}
#else
vec3 snorm12x2_to_unorm8x3(vec2 f) {
	vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047.0 + 2047.0));
	float t = floor(u.y / 256.0);
	return vec3(uint(u.x / 16.0),
		uint(fract(u.x / 16.0) * 256.0 + t),
		uint(u.y - t * 256.0)) / 255.0;
}
#endif



vec3 oct_to_float32x3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
	return normalize(v);
}

#if NORMAL_TEXTURE_IS_UINT
vec2 unorm8x3_to_snorm12x2(uvec4 u_) {
	vec3 u = vec3(u_.xyz);
	u.y *= (1.0 / 16.0);
	vec2 s = vec2(u.x * 16.0 + floor(u.y),
		fract(u.y) * (16.0 * 256.0) + u.z);
	return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
}
#else // else if !NORMAL_TEXTURE_IS_UINT:
// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
vec2 unorm8x3_to_snorm12x2(vec3 u) {
	u *= 255.0;
	u.y *= (1.0 / 16.0);
	vec2 s = vec2(u.x * 16.0 + floor(u.y),
		fract(u.y) * (16.0 * 256.0) + u.z);
	return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
}
#endif // !NORMAL_TEXTURE_IS_UINT



// From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.f, 1.f);
}


vec3 toneMapToNonLinear(vec3 col3)
{
	return toNonLinear(ACESFilm(col3 * 2.0));
	//return toNonLinear(col3 * 4.0); // linear tone-mapping
}


// See 'Calculations for recovering depth values from depth buffer' in OpenGLEngine.cpp
float getDepthFromDepthTextureValue(float near_clip_dist_, float val)
{
#if USE_REVERSE_Z
	return near_clip_dist_ / val;
#else
	return -near_clip_dist_ / (val - 1.0);
#endif
}


// See https://forwardscattering.org/post/66
float fastApproxACos(float x)
{
	if(x < 0.f)
		return 3.14159265f - ((x * 0.124605335f + 0.1570634f) * (0.99418175f + x) + sqrt(2.f + 2.f * x));
	else
		return (x * -0.124605335f + 0.1570634f) * (0.99418175f - x) + sqrt(2.f - 2.f * x);
}


float fastApproxAtan(float y, float x)
{
	return fastApproxACos(x / sqrt(x*x + y*y)) * sign(y);
}


// return (cloud frac, cumulus_edge)
vec2 getCloudFrac(vec3 env_campos_ws, vec3 dir_ws, float time, in sampler2D fbm_tex, in sampler2D cirrus_tex)
{
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
		float course_detail = fbmMix(vec2(coarse_noise_coords), fbm_tex);

		cirrus_cloudfrac = max(course_detail * 0.9, 0.f) * texture(cirrus_tex, p).x * 1.5;
	}
		
	{
		float cumulus_ray_t = rayPlaneIntersect(env_campos_ws, dir_ws, 1000.0);
		if(cumulus_ray_t > 0.0)
		{
			vec3 hitpos = env_campos_ws + dir_ws * cumulus_ray_t;
			vec2 p = hitpos.xy * 0.0001;
			p.x += time * 0.002;

			vec2 cumulus_coords = vec2(p.x * 1.0 + 2.3453, p.y * 1.0 + 1.4354);
			
			float cumulus_val = max(0.f, min(1.0, fbmMix(cumulus_coords, fbm_tex) * 1.6 - 1.0f));
			//cumulus_alpha = max(0.f, cumulus_val - 0.7f);

			cumulus_edge = smoothstep(0.0001, 0.1, cumulus_val) - smoothstep(0.2, 0.6, cumulus_val) * 0.5;

			float dist_factor = 1.f - smoothstep(20000.0, 40000.0, cumulus_ray_t);

			//cumulus_col = vec4(cumulus_val, cumulus_val, cumulus_val, 1);
			cumulus_cloudfrac = dist_factor * cumulus_val;
		}
	}

	float cloudfrac = max(cirrus_cloudfrac, cumulus_cloudfrac);
	return vec2(cloudfrac, cumulus_edge);
}


// return (cloud frac, cumulus_edge)
float getCumulusTransparencyFactor(vec3 pos_ws, vec3 sundir_ws, float time, in sampler2D fbm_tex)
{
	// Compute position on cumulus cloud layer
	vec3 cum_layer_pos = pos_ws + sundir_ws * (1000.f - pos_ws.z) / sundir_ws.z;
	
	vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
	cum_tex_coords.x += time * 0.002;
	
	vec2 cumulus_coords = vec2(cum_tex_coords.x * 1.0 + 2.3453, cum_tex_coords.y * 1.0 + 1.4354);
	float cumulus_val = max(0.f, fbmMix(cumulus_coords, fbm_tex) * 1.6 - 1.0f);
	
	float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
	return cumulus_trans;
}
