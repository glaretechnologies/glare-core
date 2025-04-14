// Common code used in various shaders


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

	float sintheta_i = sqrt(1.0 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	float sintheta_t = sintheta_i / n2; // Use Snell's law to get sin(theta_t)

	float costheta_t = sqrt(1.0 - sintheta_t*sintheta_t); // Get cos(theta_t)

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


#define VISUALISE_CASCADES 0

float getShadowMappingSunVisFactor(in vec3 final_shadow_tex_coords[NUM_DEPTH_TEXTURES], in sampler2DShadow dynamic_depth_tex, in sampler2DShadow static_depth_tex,
	float pixel_hash, vec3 pos_cs, float shadow_map_samples_xy_scale_)
{
	float sun_vis_factor = 1.0;
	float pattern_theta = pixel_hash * 6.283185307179586f;
	mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta)) * shadow_map_samples_xy_scale_;

	sun_vis_factor = 0.0;

	float depth_map_0_bias = 8.0e-5f;
	float depth_map_1_bias = 4.0e-4f;
	float static_depth_map_bias = 2.2e-3f;

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

#if VISUALISE_CASCADES
			refl_diffuse_col.yz *= 0.5f;
#endif
		}
		else
		{
			sun_vis_factor = sampleDynamicDepthMap(R, final_shadow_tex_coords[1], /*bias=*/depth_map_1_bias, dynamic_depth_tex);

			float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

			// Blending with static shadow map 0
			if(dist > edge_dist)
			{
				vec3 static_shadow_cds = final_shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

				float static_sun_vis_factor = sampleStaticDepthMap(R, static_shadow_cds, static_depth_map_bias, static_depth_tex); // NOTE: had 0.999f cap and bias of 0.0005: min(static_shadow_cds.z, 0.999f) - bias

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

			sun_vis_factor = sampleStaticDepthMap(R, shadow_cds, static_depth_map_bias, static_depth_tex);

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
			if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
			{
				float edge_dist = 0.7f * cascade_end_dist;

				// Blending with static shadow map static_depth_tex_index + 1
				if(l1dist > edge_dist)
				{
					float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds, static_depth_map_bias, static_depth_tex);

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
	
	return sun_vis_factor;
}


#endif // end if SHADOW_MAPPING
