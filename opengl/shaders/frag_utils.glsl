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



// Shadow map sample offsets, in texture coordinates for a 2048^2 depth map.  Rotated per-pixel by R in the sampling
// functions below, and scaled by shadow_map_samples_xy_scale for other depth map resolutions.
#if 1
// Two rings of 4: an axis-aligned ring at radius 0.539 texels and a diagonal one at 1.382 texels, so the 8 tap
// directions are spaced exactly 45 degrees apart.
//
// These are not point samples: each sampler2DShadow lookup is a bilinear PCF tap, i.e. a tent-weighted average of
// the comparison results over a 2x2 texel support, weight 1 at the tap position falling linearly to 0 at +/-1 texel
// on each axis.  So what a pattern applies is the mean of its taps' tent kernels, and taps spaced much closer than
// a texel have strongly overlapping kernels and add little - which is why 8 taps suffice here.
//
// The two radii are fitted so the resulting kernel matches the radial profile of the 16-tap pattern below, i.e. how
// tight the shadows look (half-weight radius 0.93 texels, the same).  Even angular spacing then makes the response
// to a shadow edge nearly independent of the edge's orientation, which is what keeps the noise down.  Against a
// randomly oriented edge: 10%-90% edge width 2.25 texels, peak per-pixel noise 0.080, RMS noise 0.030 - slightly
// quieter than the 16-tap pattern, for half the taps.
const int NUM_SHADOW_SAMPLES = 8;
const vec2 samples[NUM_SHADOW_SAMPLES] = vec2[](
	vec2( 0.00026337,  0.00000000),
	vec2( 0.00000000,  0.00026337),
	vec2(-0.00026337,  0.00000000),
	vec2( 0.00000000, -0.00026337),
	vec2( 0.00047697,  0.00047697),
	vec2(-0.00047697,  0.00047697),
	vec2(-0.00047697, -0.00047697),
	vec2( 0.00047697, -0.00047697)
);
#else
// 16 hand-placed samples, spanning a disc of radius about 2 texels.  Half-weight radius 0.93 texels, 10%-90% edge
// width 2.41 texels, peak per-pixel noise 0.084, RMS noise 0.032.
const int NUM_SHADOW_SAMPLES = 16;
const vec2 samples[NUM_SHADOW_SAMPLES] = vec2[](
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
#endif

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
	for(int i = 0; i < NUM_SHADOW_SAMPLES; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		// Use textureLod to specify the LOD level explicitly.
		// Without this, the ANGLE D3D11 backend will flatten the branches and execute all depth map lookups, so that it can calculate derivatives.
		// We don't actually need these derivatives since we are just doing bilinear filtering with no mipmaps.
		sum += textureLod(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias), /*lod=*/0.0);
	}
	return sum * (1.f / float(NUM_SHADOW_SAMPLES));
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords, float bias, in sampler2DShadow static_depth_tex)
{
	// This technique gives sharper shadows, so will use for static depth maps to avoid shadows on smaller objects being blurred away.
	float sum = 0.0;
	for(int i = 0; i < NUM_SHADOW_SAMPLES; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += textureLod(static_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias), /*lod=*/0.0);
	}
	return sum * (1.f / float(NUM_SHADOW_SAMPLES));
}


float sampleDynamicDepthMapFast(vec3 shadow_coords, in sampler2DShadow dynamic_depth_tex)
{
	return textureLod(dynamic_depth_tex, shadow_coords, /*lod=*/0.0);
}


float sampleStaticDepthMapFast(vec3 shadow_coords, in sampler2DShadow static_depth_tex)
{
	return textureLod(static_depth_tex, shadow_coords, /*lod=*/0.0);
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
	float pattern_theta = pixel_hash * 6.283185307179586f;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta)) * shadow_map_samples_xy_scale_;

	float sun_vis_factor = 0.0;

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


// Sun visibility during a probe capture.
//
// getShadowMappingSunVisFactor() picks its cascade from -pos_cs.z, which during a capture is distance from the
// probe, not from the camera.  So a fragment a few metres from a probe selects dynamic cascade 0 however far the
// probe is from the camera, and the dynamic cascades are fitted to the main camera's view frustum - the fragment
// is nowhere near the volume those depth maps cover, and the lookup returns nonsense.  The shadow texture
// coordinates themselves are fine: the vertex shader builds them from pos_ws, so they do not depend on the
// capture's view matrix.
//
// The static cascades, unlike the dynamic ones, are axis-aligned boxes centred on the camera rather than fitted
// to its frustum - cascade 0 spans +/-64 m horizontally and +/-256 m vertically.  That contains the whole probe
// grid whichever direction a probe lies in, so selecting it unconditionally sidesteps the problem rather than
// papering over it.
//
// Static only, so dynamic objects cast no shadows into the grid.  Acceptable here: the bounce is dominated by
// static geometry, and the result is convolved down to an 8x8 irradiance tile regardless.
float getProbeCaptureSunVisFactor(in vec3 final_shadow_tex_coords[NUM_DEPTH_TEXTURES], in sampler2DShadow static_depth_tex,
	float pixel_hash, float shadow_map_samples_xy_scale_, float to_light_dot_n)
{
	float pattern_theta = pixel_hash * 6.283185307179586f;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta)) * shadow_map_samples_xy_scale_;

	float bias = 5.0e-4f / max(to_light_dot_n, 0.3); // Same as static_depth_map_0_bias in getShadowMappingSunVisFactor().

	return sampleStaticDepthMap(R, final_shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES], bias, static_depth_tex);
}


float getShadowMappingSunVisFactorFast(in vec3 final_shadow_tex_coords[NUM_DEPTH_TEXTURES], in sampler2DShadow dynamic_depth_tex, in sampler2DShadow static_depth_tex,
	vec3 pos_cs, float shadow_map_samples_xy_scale_)
{
	float sun_vis_factor = 0.0;

	float dist = -pos_cs.z;
	if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
	{
		if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
		{
			float tex_0_vis = sampleDynamicDepthMapFast(final_shadow_tex_coords[0], dynamic_depth_tex);

			float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
			if(dist > edge_dist)
			{
				float tex_1_vis = sampleDynamicDepthMapFast(final_shadow_tex_coords[1], dynamic_depth_tex);

				float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT, dist);
				sun_vis_factor = mix(tex_0_vis, tex_1_vis, blend_factor);
			}
			else
				sun_vis_factor = tex_0_vis;
		}
		else
		{
			sun_vis_factor = sampleDynamicDepthMapFast(final_shadow_tex_coords[1], dynamic_depth_tex);

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = final_shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float static_sun_vis_factor = sampleStaticDepthMapFast(static_shadow_cds, static_depth_tex);

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
			if(l1dist < 64.0)
			{
				static_depth_tex_index = 0;
				cascade_end_dist = 64.0;
				shadow_cds      = final_shadow_tex_coords[0 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = final_shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
			}
			else if(l1dist < 256.0)
			{
				static_depth_tex_index = 1;
				cascade_end_dist = 256.0;
				shadow_cds      = final_shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = final_shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
			}
			else
			{
				static_depth_tex_index = 2;
				cascade_end_dist = 1024.0;
				shadow_cds = final_shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
				next_shadow_cds = vec3(0.0); // Suppress warning about being possibly uninitialised.
			}

			sun_vis_factor = sampleStaticDepthMapFast(shadow_cds, static_depth_tex);

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					float next_sun_vis_factor = sampleStaticDepthMapFast(next_shadow_cds, static_depth_tex);

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

// Optimized snorm12x2 packing into unorm8x3
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


const float PRE_TONE_MAP_SCALE_FACTOR = 2.0;

vec3 toneMapToNonLinear(vec3 col3)
{
	return toNonLinear(ACESFilm(col3 * PRE_TONE_MAP_SCALE_FACTOR));
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


//========================= Irradiance probe atlas =========================
// Probes are stored as octahedral maps packed into a 2D atlas.  Each tile is PROBE_TILE_INTERIOR_RES^2
// interior texels surrounded by a PROBE_TILE_BORDER-texel ring holding wrapped-around copies of interior
// texels, so that a bilinear tap near a tile edge stays within the same probe.
// PROBE_* values are #defined by OpenGLEngine from IrradianceProbes::getShaderPreprocessorDefines().
//
// The whole section is compiled out unless OpenGLEngineSettings::irradiance_probes_support is set, since the
// PROBE_* defines are only emitted in that case.
#if IRRADIANCE_PROBES_SUPPORT

// The octahedral mapping itself is float32x3_to_oct() / oct_to_float32x3() above, which the normal encoding
// already uses.  Both are from Cigolle et al., "A Survey of Efficient Representations for Independent Unit
// Vectors", and are exact inverses of each other.

// Fold an octahedral coordinate that has strayed outside [-1, 1]^2 back onto the octahedron.  Used to give
// the border ring of a tile the directions its wrapped-around interior neighbours have.
vec2 wrapOctCoord(vec2 p)
{
	if(p.x >  1.0) { p.x =  2.0 - p.x; p.y = -p.y; }
	else if(p.x < -1.0) { p.x = -2.0 - p.x; p.y = -p.y; }

	if(p.y >  1.0) { p.y =  2.0 - p.y; p.x = -p.x; }
	else if(p.y < -1.0) { p.y = -2.0 - p.y; p.x = -p.x; }

	return p;
}

// Atlas texture coordinates at which to sample probe 'probe_index' for direction 'dir'.
// The irradiance tiles form a band across the top of the atlas; the depth tiles a band below it.  Columns are
// pitched at PROBE_ATLAS_COLUMN_PITCH in both.
vec2 probeAtlasTexCoords(int probe_index, vec3 dir)
{
	vec2 oct_uv = float32x3_to_oct(dir) * 0.5 + vec2(0.5); // To [0, 1] x [0, 1]

	// Texel position within the tile.  oct_uv = 0 lands on the boundary between the border texel and the
	// first interior texel, which is where the border ring is needed.
	vec2 tile_texel = vec2(float(PROBE_TILE_BORDER)) + oct_uv * float(PROBE_TILE_INTERIOR_RES);

	vec2 tile_origin = vec2(
		float(probe_index % PROBE_ATLAS_PROBES_PER_ROW) * float(PROBE_ATLAS_COLUMN_PITCH),
		float(probe_index / PROBE_ATLAS_PROBES_PER_ROW) * float(PROBE_TILE_RES));

	return (tile_origin + tile_texel) / vec2(float(PROBE_ATLAS_W), float(PROBE_ATLAS_H));
}


vec2 probeDepthAtlasTexCoords(int probe_index, vec3 dir)
{
	vec2 oct_uv = float32x3_to_oct(dir) * 0.5 + vec2(0.5);

	vec2 tile_texel = vec2(float(PROBE_DEPTH_TILE_BORDER)) + oct_uv * float(PROBE_DEPTH_TILE_INTERIOR_RES);

	vec2 tile_origin = vec2(
		float(probe_index % PROBE_ATLAS_PROBES_PER_ROW) * float(PROBE_ATLAS_COLUMN_PITCH),
		float(PROBE_DEPTH_REGION_Y) + float(probe_index / PROBE_ATLAS_PROBES_PER_ROW) * float(PROBE_DEPTH_TILE_RES));

	return (tile_origin + tile_texel) / vec2(float(PROBE_ATLAS_W), float(PROBE_ATLAS_H));
}


// Mean distance and mean squared distance stored for probe 'probe_index' in direction 'dir'.
vec2 sampleProbeDepth(int probe_index, vec3 dir, in sampler2D probe_irradiance_tex)
{
	return texture(probe_irradiance_tex, probeDepthAtlasTexCoords(probe_index, dir)).xy;
}

// Set to 0 to go back to plain bilinear, for comparison.
#define PROBE_IRRADIANCE_BICUBIC 1


#if PROBE_IRRADIANCE_BICUBIC

// Cubic B-spline reconstruction of a 2D texture, using 4 bilinear fetches rather than 16 point fetches: each
// pair of adjacent taps is folded into one fetch placed between them, weighted so the hardware's own linear
// blend produces the pair's contribution (Sigg & Hadwiger).
// See also ("Using a a single linearly interpolated sample to evaluate a weighted sum of two texels") https://forwardscattering.org/post/76
//
// tex_coord is in texel space, where texel k's centre is at k + 0.5.
vec3 sampleTextureBSplineBicubic(in sampler2D tex, vec2 tex_coord, vec2 tex_size)
{
	vec2 c = tex_coord - 0.5; // Sample-index space, where texel k sits at integer k.
	vec2 i = floor(c);
	vec2 f = c - i;
	vec2 f2 = f * f;
	vec2 f3 = f2 * f;

	// Uniform cubic B-spline basis over the 4 taps i-1, i, i+1, i+2.  These sum to 1.
	vec2 w0 = -(1.0 / 6.0) * f3 +       0.5 * f2 - 0.5 * f + (1.0 / 6.0);
	vec2 w1 =         0.5  * f3 -             f2           + (2.0 / 3.0);
	vec2 w2 =        -0.5  * f3 +       0.5 * f2 + 0.5 * f + (1.0 / 6.0);
	vec2 w3 =  (1.0 / 6.0) * f3;

	// Pair weights, and the position between each pair at which a bilinear fetch reproduces it.  Neither sum can
	// reach zero over f in [0, 1] - s0 runs 5/6 down to 1/6 and s1 the other way - so the divides are safe.
	vec2 s0 = w0 + w1;
	vec2 s1 = w2 + w3;
	vec2 t0 = (i - 1.0 + w1 / s0 + 0.5) / tex_size;
	vec2 t1 = (i + 1.0 + w3 / s1 + 0.5) / tex_size;

	return
		(texture(tex, vec2(t0.x, t0.y)).xyz * s0.x + texture(tex, vec2(t1.x, t0.y)).xyz * s1.x) * s0.y +
		(texture(tex, vec2(t0.x, t1.y)).xyz * s0.x + texture(tex, vec2(t1.x, t1.y)).xyz * s1.x) * s1.y;
}

#endif // PROBE_IRRADIANCE_BICUBIC


// Cosine-weighted irradiance arriving at a surface with normal 'dir', from probe 'probe_index'.
// Units match the old cosine_env_tex: integral over hemisphere of cosine * incoming radiance * 1.0e-9.
vec3 sampleProbeIrradiance(int probe_index, vec3 dir, in sampler2D probe_irradiance_tex)
{
	// Use bicubic sampling, since linear sampling has some nasty visible artifacts around x=0 and y=0 on the sphere of normals, due to the 45 degree slope of the texels in the 
	// octahedral encoding and the way bilinear sampling works.
#if PROBE_IRRADIANCE_BICUBIC
	// The B-spline reaches taps i-1 .. i+2.  With a 2-texel border the tile texel coordinate runs over
	// [2, 2 + PROBE_TILE_INTERIOR_RES], so the taps span exactly the tile's own texels and never stray into a
	// neighbouring probe - see IRRADIANCE_TILE_BORDER in IrradianceProbes.h.
	return sampleTextureBSplineBicubic(probe_irradiance_tex,
		probeAtlasTexCoords(probe_index, dir) * vec2(float(PROBE_ATLAS_W), float(PROBE_ATLAS_H)),
		vec2(float(PROBE_ATLAS_W), float(PROBE_ATLAS_H)));
#else
	return texture(probe_irradiance_tex, probeAtlasTexCoords(probe_index, dir)).xyz;
#endif
}


// Atlas index of the grid probe at window coordinates 'c'.
// grid_dims: xyz = probe counts along each axis, w = atlas index of the first grid probe.
// base_cell: world cell coordinates of window cell (0, 0, 0).
//
// Slots are assigned toroidally, by world cell modulo the grid dimensions, so that scrolling the window by one
// cell only invalidates the newly exposed slab instead of shifting every probe to a different slot.  Must agree
// with IrradianceProbes::gridProbeIndex().
int probeIndexForGridCoords(ivec3 c, ivec3 base_cell, ivec4 grid_dims)
{
	ivec3 slot = (c + base_cell) % grid_dims.xyz;
	slot += ivec3(lessThan(slot, ivec3(0))) * grid_dims.xyz; // % can be negative in GLSL, as in C.

	return grid_dims.w + (slot.z * grid_dims.y + slot.y) * grid_dims.x + slot.x;
}

// Distance outside the probe volume, in probe spacings, over which the result fades to the global sky probe.
// One spacing is enough to hide the transition without the band reaching so far in that it dilutes the
// outermost probes.
const float PROBE_VOLUME_FADE_BAND = 1.0;

// Irradiance at pos_ws for a surface with normal 'dir', trilinearly interpolated from the 8 grid probes
// surrounding pos_ws.
//
// Outside the volume the 8-tap lookup has nothing to interpolate - the grid coordinates clamp, so it just
// smears the boundary probes outwards, which is only defensible right at the boundary.  So the result fades to
// the global sky probe over PROBE_VOLUME_FADE_BAND, and beyond that the grid is not sampled at all.  Shading
// inside the volume is untouched, since the fade factor is zero there.
//
// dir is in world space.  So is the global sky probe tile - probe_bake_from_cubemap_frag_shader.glsl rotates out
// of cosine_env_tex's sun-aligned frame at bake time - so every tile here is indexed the same way.
// grid_origin: xyz = world space position of grid probe (0, 0, 0), w = probe spacing.
// Weights are accumulated and normalised rather than assumed to sum to 1, because individual probes get
// dropped by the backface and visibility tests.
// sky_fallback: blend to the global sky probe outside the volume.  False during a probe capture, where that sky
// is unoccluded and would be fed back into the grid on every iteration, putting a floor under how dark an
// enclosed space can get.  Fading to zero instead converges from below, which is the safer error.
vec3 sampleProbeGridIrradiance(vec3 pos_ws, vec3 dir, vec4 grid_origin, ivec4 grid_dims, bool use_visibility, bool sky_fallback, in sampler2D probe_irradiance_tex)
{
	float grid_spacing = grid_origin.w;

	vec3 grid_coords = (pos_ws - grid_origin.xyz) * (1.0 / grid_spacing);

	ivec3 max_coords = grid_dims.xyz - ivec3(1);

	// Per-axis overshoot past the outermost probe planes, negative when inside.  Taking the length of the
	// positive part gives the Euclidean distance to the volume, so the fade behaves the same across faces,
	// edges and corners.
	vec3 overshoot = max(-grid_coords, grid_coords - vec3(max_coords));
	float dist_outside = length(max(overshoot, vec3(0.0)));

	float sky_blend = clamp(dist_outside * (1.0 / PROBE_VOLUME_FADE_BAND), 0.0, 1.0);

	vec3 sky_probe_irradiance = (sky_blend > 0.0 && sky_fallback) ? sampleProbeIrradiance(GLOBAL_SKY_PROBE_INDEX, dir, probe_irradiance_tex) : vec3(0.0);

	if(sky_blend >= 1.0)
		return sky_probe_irradiance; // Fully outside the band: skip the 8 grid taps and their depth taps.

	vec3 base_coords = floor(grid_coords);
	vec3 frac_coords = grid_coords - base_coords;
	ivec3 base = ivec3(base_coords);

	// World cell of window cell (0,0,0), needed for the toroidal slot mapping.  Derived rather than passed in,
	// since grid_origin is snapped to whole probe spacings.
	ivec3 base_cell = ivec3(floor(grid_origin.xyz / grid_spacing + vec3(0.5)));

	vec3 irradiance_sum = vec3(0.0);
	float weight_sum = 0.0;

	// Accumulated without the visibility term, as a fallback for when every probe gets rejected.
	vec3 unweighted_sum = vec3(0.0);
	float unweighted_weight_sum = 0.0;

	for(int i=0; i<8; ++i)
	{
		ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);

		vec3 axis_weights = mix(vec3(1.0) - frac_coords, frac_coords, vec3(offset));
		float weight = axis_weights.x * axis_weights.y * axis_weights.z;
		if(weight <= 0.0)
			continue;

		ivec3 c = clamp(base + offset, ivec3(0), max_coords);
		int probe_index = probeIndexForGridCoords(c, base_cell, grid_dims);

		vec3 probe_irradiance = sampleProbeIrradiance(probe_index, dir, probe_irradiance_tex);

		unweighted_sum += probe_irradiance * weight;
		unweighted_weight_sum += weight;

		if(use_visibility)
		{
			vec3 probe_pos = grid_origin.xyz + vec3(c) * grid_spacing;
			vec3 to_probe = probe_pos - pos_ws;
			float dist_to_probe = length(to_probe);
			vec3 unit_to_probe = (dist_to_probe > 0.0) ? (to_probe / dist_to_probe) : dir;

			// Probes behind the surface see a different side of it, so fade them out.  Smooth rather than a hard
			// cutoff, otherwise the transition shows up as a seam.
			weight *= square(max(0.0, dot(dir, unit_to_probe)) * 0.5 + 0.5);

			// Chebyshev's inequality bounds the probability that the shading point is further from the probe than
			// whatever the probe can see in this direction - i.e. the probability it is not occluded from it.
			// The depth tile is indexed by direction away from the probe, hence -unit_to_probe.
			vec2 depth_stats = sampleProbeDepth(probe_index, -unit_to_probe, probe_irradiance_tex);
			float mean_dist = depth_stats.x;

			if(dist_to_probe > mean_dist)
			{
				float variance = max(0.0, depth_stats.y - mean_dist * mean_dist);
				float excess = dist_to_probe - mean_dist;
				float chebyshev = variance / (variance + excess * excess);

				weight *= chebyshev * chebyshev * chebyshev; // Cubed to bias hard towards rejecting occluded probes.
			}
		}

		irradiance_sum += probe_irradiance * weight;
		weight_sum += weight;
	}

	// Where every probe was rejected there is nothing sensible to interpolate, so fall back to plain trilinear
	// rather than going black.
	vec3 grid_irradiance;
	if(weight_sum <= 1.0e-6)
		grid_irradiance = (unweighted_weight_sum > 0.0) ? (unweighted_sum * (1.0 / unweighted_weight_sum)) : vec3(0.0);
	else
		grid_irradiance = irradiance_sum * (1.0 / weight_sum);

	return mix(grid_irradiance, sky_probe_irradiance, sky_blend);
}

#endif // IRRADIANCE_PROBES_SUPPORT
