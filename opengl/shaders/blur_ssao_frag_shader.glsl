

uniform sampler2D albedo_texture; // source texture

uniform sampler2D main_depth_texture;
#if NORMAL_TEXTURE_IS_UINT
uniform usampler2D main_normal_texture;
#else
uniform sampler2D main_normal_texture;
#endif

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


void main()
{
	ivec2 tex_res = textureSize(albedo_texture, /*mip level*/0);
	
	ivec2 px_coords = ivec2(int(float(tex_res.x) * pos.x), int(float(tex_res.y) * pos.y));

	float centre_depth = getDepthFromDepthTexture(px_coords);
	vec3 centre_n_cs = readNormalFromNormalTexture(px_coords);
	vec3 centre_p_cs = camSpaceFromScreenSpacePos(pos, centre_depth); // View/camera space 'fragment' position

	float V_dot_n = abs(dot(centre_n_cs, centre_p_cs)) / length(centre_p_cs);
	float depth_thresh = 0.005 * centre_depth / max(0.05, V_dot_n);

#if 0
	vec4 val = texelFetch(albedo_texture, px_coords, /*mip level=*/0);
#else
	vec4 val = vec4(0.0);
	float sum_weight = 0.0;

	const int r = 3;
	for(int y = px_coords.y-r; y <= px_coords.y + r; ++y)
	for(int x = px_coords.x-r; x <= px_coords.x + r; ++x)
	{
		if(x >= 0 && x < tex_res.x && y >= 0 && y < tex_res.y)
		{
			float depth = getDepthFromDepthTexture(ivec2(x, y));
			vec3 normal_cs = readNormalFromNormalTexture(ivec2(x, y));
			if((abs(depth - centre_depth) < depth_thresh) && (dot(centre_n_cs, normal_cs) > 0.7))
			{
				val += texelFetch(albedo_texture, ivec2(x, y), /*mip level=*/0);
				sum_weight += 1.0;
			}
		}
	}

	val /= sum_weight;
#endif
	
	colour_out = val;
}
