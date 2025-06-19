

uniform sampler2D albedo_texture; // source texture

//uniform sampler2D main_colour_texture; // prepass colour texture (for roughness)
uniform sampler2D main_depth_texture; // prepass depth texture
#if NORMAL_TEXTURE_IS_UINT
uniform usampler2D main_normal_texture; // prepass normal texture
#else
uniform sampler2D main_normal_texture; // prepass normal texture
#endif

uniform int is_ssao_blur;
uniform int blur_x;

in vec2 pos; // [0, 1] x [0, 1]

out vec4 colour_out;


// See 'Calculations for recovering depth values from depth buffer', OpenGLEngine.cpp.
float getDepthFromDepthTexture(ivec2 px_coords)
{
	return getDepthFromDepthTextureValue(near_clip_dist, texelFetch(main_depth_texture, px_coords, /*mip level=*/0).x);
}

// Returns normalised vector in cam space
vec3 readNormalFromNormalTexture(ivec2 px_coords)
{
#if NORMAL_TEXTURE_IS_UINT
	return oct_to_float32x3(unorm8x3_to_snorm12x2(texelFetch(main_normal_texture, px_coords, /*mip level=*/0))); // Read normal from normal texture
#else
	return oct_to_float32x3(unorm8x3_to_snorm12x2(texelFetch(main_normal_texture, px_coords, /*mip level=*/0).xyz)); // Read normal from normal texture.  oct_to_float32x3 returns a normalised vector
#endif
}


vec3 camSpaceFromScreenSpacePos(vec2 normed_pos_ss, float depth)
{
	return vec3(
		(normed_pos_ss.x - 0.5) * depth / l_over_w,
		(normed_pos_ss.y - 0.5) * depth / l_over_h,
		-depth
	);
}


// See MitchellNetravali.h
float mitchellNetravaliEval(float x)
{
	float B = 1.0f; // max blur
	float C = 0.0f;

	float region_0_a = (float(12)  - B*9  - C*6) * (1.f/6);
	float region_0_b = (float(-18) + B*12 + C*6) * (1.f/6);
	float region_0_d = (float(6)   - B*2       ) * (1.f/6);

	float region_1_a = (-B - C*6)                * (1.f/6);
	float region_1_b = (B*6 + C*30)              * (1.f/6);
	float region_1_c = (B*-12 - C*48)            * (1.f/6);
	float region_1_d = (B*8 + C*24)              * (1.f/6);

	float region_0_f = region_0_a * (x*x*x) + region_0_b * (x*x) + region_0_d;
	float region_1_f = region_1_a * (x*x*x) + region_1_b * (x*x) + region_1_c * x + region_1_d;
	if(x < 1.0)
		return region_0_f;
	else if(x < 2.0)
		return region_1_f;
	else
		return 0.0;
}


void main()
{
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
	
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	float centre_depth = getDepthFromDepthTexture(px_coords);
	vec3 centre_n_cs = readNormalFromNormalTexture(px_coords);
	vec3 centre_p_cs = camSpaceFromScreenSpacePos(pos, centre_depth); // View/camera space 'fragment' position

	float V_dot_n = abs(dot(centre_n_cs, centre_p_cs)) / length(centre_p_cs);
	float depth_thresh = 0.03 * centre_depth / max(0.05, V_dot_n);


	float radius;
	if(is_ssao_blur != 0) // If this is the SSAO blur:
		radius = 5.0;
	else // else if specular reflection blur:
	{
		float roughness_times_trace_dist = texelFetch(albedo_texture, px_coords, /*mip level=*/0).w;
		radius = clamp(roughness_times_trace_dist * 200.0, 3.0, 25.0); // TODO: make radius calculation not ad hoc
	}
	int r = int(radius + 0.9999); // round up
	float radius_scale = 2.f / radius;

	vec4 val = vec4(0.0);
	float sum_weight = 0.0;
	if(blur_x != 0) // If should blur in x direction:
	{
		int y = px_coords.y;
		for(int x = px_coords.x - r; x <= px_coords.x + r; ++x)
		{
			if(x >= 0 && x < tex_res.x)
			{
				float depth = getDepthFromDepthTexture(ivec2(x, y));
				vec3 normal_cs = readNormalFromNormalTexture(ivec2(x, y));
				if((abs(depth - centre_depth) < depth_thresh) && (dot(centre_n_cs, normal_cs) > 0.7))
				{
					float weight = mitchellNetravaliEval(abs(x - px_coords.x) * radius_scale);
					val += texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0) * weight;
					sum_weight += weight;
				}
			}
		}
	}
	else // else if should blur in y direction:
	{
		int x = px_coords.x;
		for(int y = px_coords.y - r; y <= px_coords.y + r; ++y)
		{
			if(y >= 0 && y < tex_res.y)
			{
				float depth = getDepthFromDepthTexture(ivec2(x, y));
				vec3 normal_cs = readNormalFromNormalTexture(ivec2(x, y));
				if((abs(depth - centre_depth) < depth_thresh) && (dot(centre_n_cs, normal_cs) > 0.7))
				{
					float weight = mitchellNetravaliEval(abs(y - px_coords.y) * radius_scale);
					val += texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0) * weight;
					sum_weight += weight;
				}
			}
		}
	}

	val /= sum_weight;

	colour_out = val;
}
